#include "rooms.hpp"

#include <chrono>
#include <ctime>

#include "../src/ai.hpp"
#include "../src/display.hpp"
#include "../src/elo.hpp"
#include "../src/save.hpp"

Rooms::Rooms(Store& store) : store_(store), random_(std::random_device{}()) {}

void Rooms::set_wait_seconds(int seconds)
{
    wait_seconds_ = seconds;
}

void Rooms::set_turn_seconds(int seconds)
{
    turn_seconds_ = seconds;
}

static long long now_seconds()
{
    return static_cast<long long>(std::time(nullptr));
}

// Code de salon de 5 caractères, sans lettres ambiguës (O/0, I/1)
std::string Rooms::new_code()
{
    const std::string letters = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    std::string code;
    do {
        code.clear();
        for (int i = 0; i < 5; i++) {
            code += letters[random_() % letters.size()];
        }
    } while (lobbies_.count(code) > 0);
    return code;
}

// Un joueur n'est que dans un salon à la fois (salons pas encore lancés)
void Rooms::remove_from_lobbies(int account)
{
    for (auto it = lobbies_.begin(); it != lobbies_.end();) {
        Lobby& lobby = it->second;
        if (lobby.game_id == 0) {
            for (auto seat = lobby.members.begin(); seat != lobby.members.end(); ++seat) {
                if (seat->account == account) {
                    lobby.members.erase(seat);
                    lobby.version++;
                    break;
                }
            }
            if (lobby.members.empty()) {
                lobby_changed(lobby, true);
                it = lobbies_.erase(it);
                continue;
            }
            if (lobby.host == account) {
                lobby.host = lobby.members[0].account;
            }
            lobby_changed(lobby, false);
        }
        ++it;
    }
}

void Rooms::wait_for_change(std::unique_lock<std::mutex>& lock, const std::function<long()>& version, long since)
{
    if (since <= 0) {
        return;
    }
    changed_.wait_for(lock, std::chrono::seconds(wait_seconds_), [&]() { return version() > since; });
}

// ---------------------------------------------------------------- Salons

// Le bot Discord suit les salons liés à un salon vocal (places, lancement)
void Rooms::lobby_changed(const Lobby& lobby, bool closed)
{
    if (lobby.voice_channel.empty()) {
        return;
    }
    nlohmann::json event = {{"id", next_bot_event_++},
                            {"code", lobby.code},
                            {"voice_channel", lobby.voice_channel},
                            {"seats", lobby.seats},
                            {"players", lobby.members.size()},
                            {"started", lobby.game_id != 0},
                            {"closed", closed}};
    bot_events_.push_back(event);
    if (bot_events_.size() > 200) {
        bot_events_.erase(bot_events_.begin());
    }
}

std::string Rooms::create_lobby(const Seat& me, Mode mode, int seats, std::string& code, const std::string& voice_channel)
{
    if (seats < 2 || seats > 4) {
        return "Une partie se joue à 2, 3 ou 4.";
    }
    Lobby created;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        remove_from_lobbies(me.account);
        code = new_code();
        Lobby& lobby = lobbies_[code];
        lobby.code = code;
        lobby.host = me.account;
        lobby.mode = mode;
        lobby.seats = seats;
        lobby.voice_channel = voice_channel;
        lobby.members.push_back(me);
        lobby_changed(lobby, false);
        created = lobby;
    }
    changed_.notify_all();
    if (on_lobby_created) {
        on_lobby_created(created);
    }
    return "";
}

std::string Rooms::join_lobby(const std::string& code, const Seat& me)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto found = lobbies_.find(code);
        if (found == lobbies_.end()) {
            return "Aucun salon avec ce code.";
        }
        for (const Seat& seat : found->second.members) {
            if (seat.account == me.account) {
                return "";  // déjà dedans
            }
        }
        if (found->second.game_id != 0) {
            return "La partie a déjà commencé.";
        }
        if (static_cast<int>(found->second.members.size()) >= found->second.seats) {
            return "Le salon est complet.";
        }
        remove_from_lobbies(me.account);
        // Le salon a pu disparaître si on en était le seul membre
        found = lobbies_.find(code);
        if (found == lobbies_.end()) {
            return "Aucun salon avec ce code.";
        }
        found->second.members.push_back(me);
        found->second.version++;
        lobby_changed(found->second, false);
    }
    changed_.notify_all();
    return "";
}

std::string Rooms::leave_lobby(const std::string& code, int account)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (lobbies_.count(code) == 0) {
            return "Aucun salon avec ce code.";
        }
        remove_from_lobbies(account);
    }
    changed_.notify_all();
    return "";
}

std::string Rooms::start_lobby(const std::string& code, int account, int& game_id)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto found = lobbies_.find(code);
        if (found == lobbies_.end()) {
            return "Aucun salon avec ce code.";
        }
        Lobby& lobby = found->second;
        if (lobby.host != account) {
            return "Seul l'hôte peut lancer la partie.";
        }
        if (lobby.game_id != 0) {
            game_id = lobby.game_id;
            return "";
        }
        if (lobby.members.size() < 2) {
            return "Il faut au moins 2 joueurs.";
        }
        std::vector<std::string> names;
        for (const Seat& seat : lobby.members) {
            names.push_back(seat.pseudo);
        }
        OnlineGame& online = games_[next_game_];
        online.id = next_game_;
        online.game.reset(new Game(names, random_(), lobby.mode));
        online.seats = lobby.members;
        online.missed.assign(lobby.members.size(), 0);
        online.turn_start = now_seconds();
        lobby.game_id = next_game_;
        lobby.version++;
        lobby_changed(lobby, false);
        game_id = next_game_;
        next_game_++;
    }
    changed_.notify_all();
    return "";
}

bool Rooms::lobby_json(const std::string& code, long since, nlohmann::json& out)
{
    std::unique_lock<std::mutex> lock(mutex_);
    wait_for_change(lock, [&]() { auto it = lobbies_.find(code); return it == lobbies_.end() ? since + 1 : it->second.version; }, since);
    auto found = lobbies_.find(code);
    if (found == lobbies_.end()) {
        return false;
    }
    const Lobby& lobby = found->second;
    out = nlohmann::json::object();
    out["code"] = lobby.code;
    out["mode"] = lobby.mode == Mode::Classic ? "classique" : "rapide";
    out["seats"] = lobby.seats;
    out["host"] = lobby.host;
    out["version"] = lobby.version;
    out["game_id"] = lobby.game_id;
    out["members"] = nlohmann::json::array();
    for (const Seat& seat : lobby.members) {
        out["members"].push_back({{"account", seat.account}, {"pseudo", seat.pseudo}, {"elo", seat.elo}});
    }
    return true;
}

// ---------------------------------------------------------------- Parties

void Rooms::finish_game(OnlineGame& online)
{
    // Elo actuel de chaque joueur (il a pu changer pendant la partie)
    std::vector<int> ratings;
    for (const Seat& seat : online.seats) {
        Account account;
        ratings.push_back(store_.get_account(seat.account, account) ? account.elo : seat.elo);
    }
    std::vector<int> places = final_places(*online.game);
    online.elo_changes = elo_changes(ratings, places);
    std::vector<GameResult> results;
    for (int i = 0; i < static_cast<int>(online.seats.size()); i++) {
        results.push_back({online.seats[i].account, places[i], online.game->players()[i].score(), ratings[i],
                           ratings[i] + online.elo_changes[i]});
    }
    store_.record_game(online.game->mode(), results);
    online.recorded = true;
}

std::string Rooms::play(int game_id, int account, const Move& move)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto found = games_.find(game_id);
        if (found == games_.end()) {
            return "Partie introuvable.";
        }
        OnlineGame& online = found->second;
        Game& game = *online.game;
        if (game.is_over()) {
            return "La partie est terminée.";
        }
        int current = game.current_index();
        if (online.seats[current].account != account) {
            return "Ce n'est pas votre tour.";
        }
        size_t before = game.events().size();
        std::string error = apply_move(game, move);
        if (!error.empty()) {
            return error;
        }
        online.missed[current] = 0;  // il a joué lui-même
        after_change(online, before);
    }
    changed_.notify_all();
    return "";
}

// Après un changement : historique, fin de partie, nouveau tour
void Rooms::after_change(OnlineGame& online, size_t events_before)
{
    Game& game = *online.game;
    for (size_t i = events_before; i < game.events().size(); i++) {
        online.log.push_back(describe_event(game, game.events()[i]));
    }
    if (game.is_over() && !online.recorded) {
        finish_game(online);
        online.finished_at = now_seconds();
    }
    online.turn_start = now_seconds();
    online.version++;
}

std::string Rooms::forfeit(int game_id, int account)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto found = games_.find(game_id);
        if (found == games_.end()) {
            return "Partie introuvable.";
        }
        OnlineGame& online = found->second;
        Game& game = *online.game;
        if (game.is_over()) {
            return "La partie est terminée.";
        }
        int seat = -1;
        for (int i = 0; i < static_cast<int>(online.seats.size()); i++) {
            if (online.seats[i].account == account) {
                seat = i;
            }
        }
        if (seat < 0) {
            return "Vous ne jouez pas dans cette partie.";
        }
        size_t before = game.events().size();
        std::string error = game.forfeit(seat);
        if (!error.empty()) {
            return error;
        }
        after_change(online, before);
    }
    changed_.notify_all();
    return "";
}

void Rooms::remove_account(int account)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        remove_from_lobbies(account);
        for (auto& entry : games_) {
            OnlineGame& online = entry.second;
            Game& game = *online.game;
            for (int seat = 0; seat < static_cast<int>(online.seats.size()); seat++) {
                if (online.seats[seat].account != account || game.is_over()) {
                    continue;
                }
                size_t before = game.events().size();
                // Tour en cours à moitié joué (défausse, noble) : on le termine d'abord
                while (!game.is_over() && game.current_index() == seat && game.step() != TurnStep::Action) {
                    if (!apply_move(game, ai_choose(game, false)).empty()) {
                        break;
                    }
                }
                game.forfeit(seat);
                after_change(online, before);
            }
        }
        last_seen_.erase(account);
    }
    changed_.notify_all();
}

void Rooms::tick()
{
    bool any_change = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        long long now = now_seconds();
        for (auto it = games_.begin(); it != games_.end();) {
            OnlineGame& online = it->second;
            Game& game = *online.game;
            if (game.is_over()) {
                // Partie finie depuis longtemps : on l'oublie (elle est enregistrée)
                if (online.finished_at > 0 && now - online.finished_at > FINISHED_GAME_SECONDS) {
                    it = games_.erase(it);
                    continue;
                }
                ++it;
                continue;
            }
            if (now - online.turn_start >= turn_seconds_) {
                int late = game.current_index();
                size_t before = game.events().size();
                // Défausse ou choix du noble en attente : le serveur les termine
                // (coups imposés, choisis par l'IA facile), puis le tour passe
                while (!game.is_over() && game.current_index() == late && game.step() != TurnStep::Action) {
                    if (!apply_move(game, ai_choose(game, false)).empty()) {
                        break;
                    }
                }
                if (!game.is_over() && game.current_index() == late && game.step() == TurnStep::Action) {
                    game.skip_turn();
                }
                online.missed[late]++;
                if (!game.is_over() && online.missed[late] >= MAX_MISSED_TURNS) {
                    game.forfeit(late);
                }
                after_change(online, before);
                any_change = true;
            }
            ++it;
        }
    }
    if (any_change) {
        changed_.notify_all();
    }
}

bool Rooms::game_json(int game_id, int account, long since, nlohmann::json& out)
{
    std::unique_lock<std::mutex> lock(mutex_);
    wait_for_change(lock, [&]() { auto it = games_.find(game_id); return it == games_.end() ? since + 1 : it->second.version; }, since);
    auto found = games_.find(game_id);
    if (found == games_.end()) {
        return false;
    }
    const OnlineGame& online = found->second;
    int my_seat = -2;  // spectateur : toutes les informations cachées
    for (int i = 0; i < static_cast<int>(online.seats.size()); i++) {
        if (online.seats[i].account == account) {
            my_seat = i;
        }
    }
    out = nlohmann::json::object();
    out["id"] = online.id;
    out["version"] = online.version;
    out["your_seat"] = my_seat >= 0 ? my_seat : -1;
    out["save"] = save_game_for(*online.game, my_seat);
    out["log"] = online.log;
    out["over"] = online.game->is_over();
    // Secondes restantes pour jouer (le tour passe ensuite)
    long long left = online.turn_start + turn_seconds_ - now_seconds();
    out["turn_seconds_left"] = online.game->is_over() ? 0 : (left < 0 ? 0 : left);
    out["missed"] = online.missed;
    out["elo_changes"] = online.elo_changes;
    out["seats"] = nlohmann::json::array();
    for (const Seat& seat : online.seats) {
        out["seats"].push_back({{"account", seat.account}, {"pseudo", seat.pseudo}, {"elo", seat.elo}});
    }
    return true;
}

// ---------------------------------------------------------------- Présence et notifications

void Rooms::touch(int account)
{
    std::lock_guard<std::mutex> lock(mutex_);
    last_seen_[account] = std::time(nullptr);
}

bool Rooms::is_online(int account)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto found = last_seen_.find(account);
    // Le jeu interroge ses notifications au moins toutes les 30 secondes
    return found != last_seen_.end() && std::time(nullptr) - found->second < 70;
}

void Rooms::notify(int account, nlohmann::json notification)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        notification["id"] = next_notification_++;
        std::vector<nlohmann::json>& list = notifications_[account];
        list.push_back(notification);
        if (list.size() > 20) {
            list.erase(list.begin());
        }
    }
    changed_.notify_all();
}

void Rooms::notifications_json(int account, long since, nlohmann::json& out)
{
    std::unique_lock<std::mutex> lock(mutex_);
    auto newest = [&]() -> long {
        const std::vector<nlohmann::json>& list = notifications_[account];
        return list.empty() ? 0 : list.back()["id"].get<long>();
    };
    // since = -1 : juste l'identifiant le plus récent, sans attendre
    if (since >= 0) {
        changed_.wait_for(lock, std::chrono::seconds(wait_seconds_), [&]() { return newest() > since; });
    }
    out = nlohmann::json::object();
    out["last"] = newest();
    out["notifications"] = nlohmann::json::array();
    if (since >= 0) {
        for (const nlohmann::json& notification : notifications_[account]) {
            if (notification["id"].get<long>() > since) {
                out["notifications"].push_back(notification);
            }
        }
    }
}

void Rooms::bot_events_json(long since, nlohmann::json& out)
{
    std::unique_lock<std::mutex> lock(mutex_);
    auto newest = [&]() -> long { return bot_events_.empty() ? 0 : bot_events_.back()["id"].get<long>(); };
    changed_.wait_for(lock, std::chrono::seconds(wait_seconds_), [&]() { return newest() > since; });
    out = nlohmann::json::object();
    out["last"] = newest();
    out["events"] = nlohmann::json::array();
    for (const nlohmann::json& event : bot_events_) {
        if (event["id"].get<long>() > since) {
            out["events"].push_back(event);
        }
    }
}
