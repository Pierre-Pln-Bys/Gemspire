#include "rooms.hpp"

#include <algorithm>
#include <cstdlib>
#include <chrono>
#include <ctime>

#include "../src/achievements.hpp"
#include "../src/ai.hpp"
#include "../src/display.hpp"
#include "../src/elo.hpp"
#include "../src/move_json.hpp"
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
        return "A game is played with 2, 3 or 4 players.";
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

std::string Rooms::activity_lobby(const Seat& me, const std::string& instance, std::string& code)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& entry : lobbies_) {
            Lobby& lobby = entry.second;
            if (lobby.activity != instance || lobby.game_id != 0) {
                continue;
            }
            for (const Seat& seat : lobby.members) {
                if (seat.account == me.account) {
                    code = lobby.code;  // déjà dedans
                    return "";
                }
            }
            if (static_cast<int>(lobby.members.size()) < lobby.seats) {
                remove_from_lobbies(me.account);
                lobby.members.push_back(me);
                lobby_changed(lobby, false);
                code = lobby.code;
                changed_.notify_all();
                return "";
            }
        }
        // Pas de salon libre pour cette Activité : on en crée un (sans annonce Discord)
        remove_from_lobbies(me.account);
        code = new_code();
        Lobby& lobby = lobbies_[code];
        lobby.code = code;
        lobby.host = me.account;
        lobby.mode = Mode::Classic;
        lobby.seats = 4;
        lobby.activity = instance;
        lobby.members.push_back(me);
        lobby_changed(lobby, false);
    }
    changed_.notify_all();
    return "";
}

std::string Rooms::join_lobby(const std::string& code, const Seat& me)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto found = lobbies_.find(code);
        if (found == lobbies_.end()) {
            return "No lobby with this code.";
        }
        for (const Seat& seat : found->second.members) {
            if (seat.account == me.account) {
                return "";  // déjà dedans
            }
        }
        if (found->second.game_id != 0) {
            return "The game has already started.";
        }
        if (static_cast<int>(found->second.members.size()) >= found->second.seats) {
            return "The lobby is full.";
        }
        remove_from_lobbies(me.account);
        // Le salon a pu disparaître si on en était le seul membre
        found = lobbies_.find(code);
        if (found == lobbies_.end()) {
            return "No lobby with this code.";
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
            return "No lobby with this code.";
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
            return "No lobby with this code.";
        }
        Lobby& lobby = found->second;
        if (lobby.host != account) {
            return "Only the host can start the game.";
        }
        if (lobby.game_id != 0) {
            game_id = lobby.game_id;
            return "";
        }
        if (lobby.members.size() < 2) {
            return "At least 2 players are needed.";
        }
        game_id = new_game(lobby.members, lobby.mode, GameKind::Friendly);  // salons : toujours amicale
        lobby.game_id = game_id;
        lobby.version++;
        lobby_changed(lobby, false);
    }
    changed_.notify_all();
    return "";
}

// Elo d'un joueur qui compte pour ce type de partie (Elo global pour une amicale :
// seulement affiché)
static int rating(const Seat& seat, GameKind kind)
{
    return kind == GameKind::Season ? seat.season_elo : seat.elo;
}

static int rating(const Account& account, GameKind kind)
{
    return kind == GameKind::Season ? account.season_elo : account.elo;
}

int Rooms::new_game(const std::vector<Seat>& seats, Mode mode, GameKind kind)
{
    std::vector<std::string> names;
    for (const Seat& seat : seats) {
        names.push_back(seat.pseudo);
    }
    OnlineGame& online = games_[next_game_];
    online.id = next_game_;
    online.seed = random_();
    online.game.reset(new Game(names, online.seed, mode));
    online.seats = seats;
    online.kind = kind;
    online.missed.assign(seats.size(), 0);
    online.turn_start = now_seconds();
    return next_game_++;
}

// ---------------------------------------------------------------- Recherche de partie

// Écart d'Elo accepté après `waited` secondes d'attente : 100, puis +20 par seconde
static int allowed_gap(long long waited)
{
    return MATCH_BASE_GAP + MATCH_GAP_PER_SECOND * static_cast<int>(waited);
}

void Rooms::try_match()
{
    long long now = now_seconds();
    // Jeu fermé pendant l'attente (plus de nouvelles depuis 15 s) : retiré de la file
    for (auto it = queue_.begin(); it != queue_.end();) {
        auto seen = last_seen_.find(it->seat.account);
        bool gone = seen == last_seen_.end() || std::time(nullptr) - seen->second > 15;
        it = gone && now - it->since > 15 ? queue_.erase(it) : std::next(it);
    }
    bool found = true;
    while (found) {
        found = false;
        int best_a = -1;
        int best_b = -1;
        int best_gap = 0;
        for (int a = 0; a < static_cast<int>(queue_.size()); a++) {
            for (int b = a + 1; b < static_cast<int>(queue_.size()); b++) {
                if (queue_[a].mode != queue_[b].mode || queue_[a].kind != queue_[b].kind) {
                    continue;
                }
                GameKind kind = queue_[a].kind;
                int gap = std::abs(rating(queue_[a].seat, kind) - rating(queue_[b].seat, kind));
                long long waited = now - std::min(queue_[a].since, queue_[b].since);
                if (gap <= allowed_gap(waited) && (best_a < 0 || gap < best_gap)) {
                    best_a = a;
                    best_b = b;
                    best_gap = gap;
                }
            }
        }
        if (best_a >= 0) {
            QueueEntry first = queue_[best_a];
            QueueEntry second = queue_[best_b];
            int game_id = new_game({first.seat, second.seat}, first.mode, first.kind);
            matched_[first.seat.account] = game_id;
            matched_[second.seat.account] = game_id;
            queue_.erase(queue_.begin() + best_b);
            queue_.erase(queue_.begin() + best_a);
            found = true;
        }
    }
}

void Rooms::join_queue(const Seat& me, Mode mode, GameKind kind)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        remove_from_lobbies(me.account);
        matched_.erase(me.account);
        for (auto it = queue_.begin(); it != queue_.end(); ++it) {
            if (it->seat.account == me.account) {
                queue_.erase(it);
                break;
            }
        }
        queue_.push_back({me, mode, now_seconds(), kind});
        try_match();
    }
    changed_.notify_all();
}

void Rooms::leave_queue(int account)
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = queue_.begin(); it != queue_.end(); ++it) {
        if (it->seat.account == account) {
            queue_.erase(it);
            break;
        }
    }
}

void Rooms::queue_json(int account, nlohmann::json& out)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto matched = matched_.find(account);
    if (matched != matched_.end()) {
        out = {{"status", "matched"}, {"game_id", matched->second}};
        matched_.erase(matched);
        return;
    }
    for (const QueueEntry& entry : queue_) {
        if (entry.seat.account == account) {
            int same_queue = 0;
            for (const QueueEntry& other : queue_) {
                same_queue += other.kind == entry.kind ? 1 : 0;
            }
            out = {{"status", "waiting"}, {"waited", now_seconds() - entry.since},
                   {"players_waiting", same_queue}, {"kind", kind_name(entry.kind)}};
            return;
        }
    }
    out = {{"status", "idle"}};
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
    // Elo actuel de chaque joueur (il a pu changer pendant la partie), celui
    // du type de partie : global (classée), de saison, ou global pour une
    // amicale (qui ne le change pas)
    std::vector<int> ratings;
    for (const Seat& seat : online.seats) {
        Account account;
        ratings.push_back(store_.get_account(seat.account, account) ? rating(account, online.kind)
                                                                    : rating(seat, online.kind));
    }
    std::vector<int> places = final_places(*online.game);
    online.elo_changes = online.kind == GameKind::Friendly ? std::vector<int>(ratings.size(), 0)
                                                           : elo_changes(ratings, places);
    std::vector<GameResult> results;
    for (int i = 0; i < static_cast<int>(online.seats.size()); i++) {
        results.push_back({online.seats[i].account, places[i], online.game->players()[i].score(), ratings[i],
                           ratings[i] + online.elo_changes[i]});
    }
    // Rediffusion : même graine, mêmes joueurs, mêmes actions = même partie
    nlohmann::json names = nlohmann::json::array();
    for (const Player& player : online.game->players()) {
        names.push_back(player.name);
    }
    nlohmann::json replay = {{"seed", online.seed}, {"mode", online.game->mode() == Mode::Classic ? "classique" : "rapide"},
                             {"names", names}, {"actions", online.actions}};
    online.record_id = store_.record_game(online.game->mode(), online.kind, results, replay.dump());
    online.recorded = true;
    // Succès : seuls les nouveaux sont gardés pour l'écran de fin
    online.new_achievements.assign(online.seats.size(), {});
    for (int i = 0; i < static_cast<int>(online.seats.size()); i++) {
        Account after;
        if (!store_.get_account(online.seats[i].account, after)) {
            continue;
        }
        AchievementContext context = {i, places, ratings, after.games, after.elo};
        for (const std::string& code : earned_achievements(*online.game, context)) {
            if (store_.unlock_achievement(online.seats[i].account, code)) {
                online.new_achievements[i].push_back(code);
            }
        }
    }
}

std::string Rooms::play(int game_id, int account, const Move& move)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto found = games_.find(game_id);
        if (found == games_.end()) {
            return "Game not found.";
        }
        OnlineGame& online = found->second;
        Game& game = *online.game;
        if (game.is_over()) {
            return "The game is over.";
        }
        int current = game.current_index();
        if (online.seats[current].account != account) {
            return "It's not your turn.";
        }
        size_t before = game.events().size();
        std::string error = apply_move(game, move);
        if (!error.empty()) {
            return error;
        }
        online.actions.push_back({{"move", move_to_json(move)}});
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
        online.log.push_back(describe_event(game, game.events()[i], Lang::English));
        online.log_fr.push_back(describe_event(game, game.events()[i], Lang::French));
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
            return "Game not found.";
        }
        OnlineGame& online = found->second;
        Game& game = *online.game;
        if (game.is_over()) {
            return "The game is over.";
        }
        int seat = -1;
        for (int i = 0; i < static_cast<int>(online.seats.size()); i++) {
            if (online.seats[i].account == account) {
                seat = i;
            }
        }
        if (seat < 0) {
            return "You're not playing in this game.";
        }
        size_t before = game.events().size();
        std::string error = game.forfeit(seat);
        if (!error.empty()) {
            return error;
        }
        online.actions.push_back({{"forfeit", seat}});
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
        for (auto it = queue_.begin(); it != queue_.end(); ++it) {
            if (it->seat.account == account) {
                queue_.erase(it);
                break;
            }
        }
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
                    Move forced = ai_choose(game, false);
                    if (!apply_move(game, forced).empty()) {
                        break;
                    }
                    online.actions.push_back({{"move", move_to_json(forced)}});
                }
                if (game.forfeit(seat).empty()) {
                    online.actions.push_back({{"forfeit", seat}});
                }
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
        // L'écart d'Elo accepté grandit avec l'attente : nouvelle tentative chaque seconde
        try_match();
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
                    Move forced = ai_choose(game, false);
                    if (!apply_move(game, forced).empty()) {
                        break;
                    }
                    online.actions.push_back({{"move", move_to_json(forced)}});
                }
                if (!game.is_over() && game.current_index() == late && game.step() == TurnStep::Action) {
                    game.skip_turn();
                    online.actions.push_back({{"skip", true}});
                }
                online.missed[late]++;
                if (!game.is_over() && online.missed[late] >= MAX_MISSED_TURNS && game.forfeit(late).empty()) {
                    online.actions.push_back({{"forfeit", late}});
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
    out["log_fr"] = online.log_fr;
    out["over"] = online.game->is_over();
    // Secondes restantes pour jouer (le tour passe ensuite)
    long long left = online.turn_start + turn_seconds_ - now_seconds();
    out["turn_seconds_left"] = online.game->is_over() ? 0 : (left < 0 ? 0 : left);
    out["missed"] = online.missed;
    out["elo_changes"] = online.elo_changes;
    out["kind"] = kind_name(online.kind);
    out["seats"] = nlohmann::json::array();
    for (const Seat& seat : online.seats) {
        out["seats"].push_back({{"account", seat.account}, {"pseudo", seat.pseudo}, {"elo", rating(seat, online.kind)}});
    }
    out["achievements"] = online.new_achievements;
    out["replay_id"] = online.record_id;
    out["chat"] = nlohmann::json::array();
    for (const ChatMessage& message : online.chat) {
        out["chat"].push_back({{"id", message.id}, {"seat", message.seat}, {"text", message.text},
                               {"reaction", message.reaction}});
    }
    return true;
}

// Nettoie un message : caractères de contrôle remplacés par une espace, espaces au bord
// enlevés ; "" si le texte n'est pas de l'UTF-8 valide ou est trop long
static std::string clean_chat_text(const std::string& text)
{
    std::string out;
    int chars = 0;
    for (size_t i = 0; i < text.size();) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        size_t length = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 0;
        if (length == 0 || i + length > text.size()) {
            return "";
        }
        for (size_t k = 1; k < length; k++) {
            if ((static_cast<unsigned char>(text[i + k]) & 0xC0) != 0x80) {
                return "";
            }
        }
        if (length > 1 || c >= 32) {
            out.append(text, i, length);
            chars++;
        } else if (!out.empty() && out.back() != ' ') {
            out += ' ';  // retour à la ligne, tabulation... : une espace
            chars++;
        }
        i += length;
    }
    size_t start = out.find_first_not_of(' ');
    size_t end = out.find_last_not_of(' ');
    if (start == std::string::npos || chars > CHAT_MAX_CHARS) {
        return "";
    }
    return out.substr(start, end - start + 1);
}

std::string Rooms::chat(int game_id, int account, const std::string& text, int reaction)
{
    std::string cleaned = reaction >= 0 ? "" : clean_chat_text(text);
    if (reaction >= REACTION_COUNT || (reaction < 0 && cleaned.empty())) {
        return "Invalid message (1 to 120 characters).";
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto found = games_.find(game_id);
        if (found == games_.end()) {
            return "Game not found.";
        }
        OnlineGame& online = found->second;
        int seat = -1;
        for (int i = 0; i < static_cast<int>(online.seats.size()); i++) {
            if (online.seats[i].account == account) {
                seat = i;
            }
        }
        if (seat < 0) {
            return "You're not playing in this game.";
        }
        long long now = now_seconds();
        std::vector<long long>& times = chat_times_[account];
        std::vector<long long> recent;
        for (long long t : times) {
            if (now - t < 20) {
                recent.push_back(t);
            }
        }
        if (recent.size() >= 6 || (!recent.empty() && now - recent.back() < 1)) {
            return "Slow down: too many messages.";
        }
        recent.push_back(now);
        times = recent;
        online.chat.push_back({online.next_chat++, seat, cleaned, reaction, now});
        if (static_cast<int>(online.chat.size()) > CHAT_KEPT) {
            online.chat.erase(online.chat.begin());
        }
        online.version++;
    }
    changed_.notify_all();
    return "";
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

int Rooms::playing_game(int account)
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& entry : games_) {
        if (entry.second.game->is_over()) {
            continue;
        }
        for (const Seat& seat : entry.second.seats) {
            if (seat.account == account) {
                return entry.first;
            }
        }
    }
    return 0;
}

std::string Rooms::status_of(int account)
{
    if (playing_game(account) != 0) {
        return "playing";
    }
    return is_online(account) ? "online" : "offline";
}

void Rooms::live_games_json(nlohmann::json& out)
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::pair<int, int>> order;  // (Elo moyen, numéro)
    for (const auto& entry : games_) {
        if (entry.second.game->is_over()) {
            continue;
        }
        int total = 0;
        for (const Seat& seat : entry.second.seats) {
            total += seat.elo;
        }
        order.push_back({total / static_cast<int>(entry.second.seats.size()), entry.first});
    }
    std::sort(order.rbegin(), order.rend());
    out = nlohmann::json::array();
    for (size_t i = 0; i < order.size() && i < 20; i++) {
        const OnlineGame& online = games_.at(order[i].second);
        nlohmann::json players = nlohmann::json::array();
        for (int p = 0; p < static_cast<int>(online.seats.size()); p++) {
            players.push_back({{"pseudo", online.seats[p].pseudo}, {"elo", online.seats[p].elo},
                               {"score", online.game->players()[p].score()}});
        }
        out.push_back({{"id", online.id}, {"mode", online.game->mode() == Mode::Classic ? "classique" : "rapide"},
                       {"round", online.game->round()}, {"players", players}, {"kind", kind_name(online.kind)}});
    }
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

void Rooms::notify_online(const nlohmann::json& notification)
{
    std::vector<int> online;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        long long now = std::time(nullptr);
        for (const auto& entry : last_seen_) {
            if (now - entry.second < 70) {
                online.push_back(entry.first);
            }
        }
    }
    for (int account : online) {
        notify(account, notification);
    }
}

std::string Rooms::end_game(int game_id)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto found = games_.find(game_id);
        if (found == games_.end() || found->second.game->is_over()) {
            return "No game in progress with this number.";
        }
        games_.erase(found);
    }
    changed_.notify_all();  // les attentes longues sur cette partie répondent « introuvable »
    return "";
}

RoomsStats Rooms::stats()
{
    std::lock_guard<std::mutex> lock(mutex_);
    RoomsStats stats;
    long long now = std::time(nullptr);
    for (const auto& entry : last_seen_) {
        stats.online += now - entry.second < 70 ? 1 : 0;
    }
    for (const auto& entry : games_) {
        stats.games += entry.second.game->is_over() ? 0 : 1;
    }
    stats.lobbies = static_cast<int>(lobbies_.size());
    stats.queue = static_cast<int>(queue_.size());
    return stats;
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

void Rooms::add_bot_event(nlohmann::json event)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        event["id"] = next_bot_event_++;
        bot_events_.push_back(event);
        if (bot_events_.size() > 200) {
            bot_events_.erase(bot_events_.begin());
        }
    }
    changed_.notify_all();
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
