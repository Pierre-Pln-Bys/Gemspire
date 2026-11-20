// Interface graphique de Splendor (raylib).
// Ce fichier gère la fenêtre, les écrans, la souris, le clavier et
// l'enchaînement des coups. Le dessin est dans gui_draw.cpp.
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <functional>
#include <random>
#include <string>
#include <vector>

#include "raylib.h"

#include "json.hpp"

#include "ai.hpp"
#include "display.hpp"
#include "gui.hpp"
#include "gui_font.h"
#include "move_json.hpp"
#include "net.hpp"
#include "rules.hpp"
#include "save.hpp"
#include "version.hpp"

// ---------------------------------------------------------------- Outils

static void set_message(App& app, const std::string& message, bool is_error)
{
    app.message = message;
    app.message_is_error = is_error;
}

static void clear_selection(App& app)
{
    app.selection = {0, 0, 0, 0, 0};
    app.target = Target::None;
}

static bool same_card(const Card& a, const Card& b)
{
    return a.points == b.points && a.provides == b.provides && a.cost == b.cost;
}

static Vector2 center_of(Rectangle rect)
{
    return {rect.x + rect.width / 2, rect.y + rect.height / 2};
}

static int human_count(const Game& game)
{
    int count = 0;
    for (const Player& player : game.players()) {
        count += player.controller == Controller::Human;
    }
    return count;
}

// Prépare le plateau pour une partie qui commence (ou qui est rechargée)
static void start_board(App& app)
{
    app.screen = Screen::Board;
    clear_selection(app);
    app.shown_events = static_cast<int>(app.game->events().size());
    app.journal_open = false;
    app.curtain = false;
    app.last_player = app.game->current_index();
    app.animations.clear();
    app.flips.clear();
    app.ai_next_time = GetTime() + 0.8;
    set_message(app, "À " + app.game->current_player().name + " de jouer.", false);
}

// ---------------------------------------------------------------- Après un coup

// L'état du plateau juste avant un coup, pour savoir d'où partent les animations
struct Snapshot {
    std::array<std::vector<Card>, LEVEL_COUNT> visible;
    std::vector<Card> reserved;
};

static Snapshot take_snapshot(const Game& game)
{
    Snapshot snapshot;
    for (int level = 1; level <= LEVEL_COUNT; level++) {
        snapshot.visible[level - 1] = game.visible(level);
    }
    snapshot.reserved = game.current_player().reserved;
    return snapshot;
}

// Lance les animations correspondant aux nouveaux événements
static void animate_events(App& app, const Snapshot& before)
{
    const Game& game = *app.game;
    double now = GetTime();
    const std::vector<Event>& events = game.events();
    for (int e = app.shown_events; e < static_cast<int>(events.size()); e++) {
        const Event& event = events[e];
        Vector2 panel = center_of(player_rect(event.player));
        int k = 0;
        // Jetons pris (de la réserve vers la main) ou défaussés (dans l'autre sens)
        for (int gem = 0; gem <= GOLD_INDEX; gem++) {
            int count = gem == GOLD_INDEX ? event.gold : event.tokens[gem];
            for (int n = 0; n < count; n++) {
                Vector2 bank = bank_token_center(gem);
                Vector2 hand = player_token_center(event.player, gem);
                bool to_hand = event.type != EventType::Discard;
                app.animations.push_back({now + k * 0.08, 0.45, to_hand ? bank : hand, to_hand ? hand : bank, gem,
                                          event.card});
                k++;
            }
        }
        // Carte achetée ou réservée : elle part vers le joueur, la nouvelle se retourne
        if (event.type == EventType::BuyCard || (event.type == EventType::Reserve && !event.from_deck)) {
            for (int level = 1; level <= LEVEL_COUNT; level++) {
                const std::vector<Card>& row = before.visible[level - 1];
                for (int i = 0; i < static_cast<int>(row.size()); i++) {
                    if (same_card(row[i], event.card)) {
                        app.animations.push_back({now, 0.5, center_of(card_rect(level, i)), panel, -1, event.card});
                        if (i < static_cast<int>(game.visible(level).size())) {
                            app.flips.push_back({now, level, i});
                        }
                    }
                }
            }
        } else if (event.type == EventType::BuyReserved) {
            for (int i = 0; i < static_cast<int>(before.reserved.size()); i++) {
                if (same_card(before.reserved[i], event.card)) {
                    app.animations.push_back({now, 0.5, center_of(reserved_rect(i)), panel, -1, event.card});
                    break;
                }
            }
        }
    }
}

// Met dans la ligne d'état ce qui vient de se passer, puis ce qu'il faut faire
static void show_new_events(App& app)
{
    const Game& game = *app.game;
    std::string text;
    for (; app.shown_events < static_cast<int>(game.events().size()); app.shown_events++) {
        text += describe_event(game, game.events()[app.shown_events]) + " ";
    }
    if (game.is_over()) {
        text += "Partie terminée.";
    } else if (game.step() == TurnStep::Discard) {
        text += "Plus de 10 jetons : cliquez sur vos jetons (ou touches 1 à 6) pour en défausser "
                + std::to_string(game.tokens_to_discard()) + ".";
    } else if (game.step() == TurnStep::ChooseNoble) {
        text += "Plusieurs nobles veulent venir : cliquez sur celui de votre choix.";
    } else {
        text += "À " + game.current_player().name + " de jouer.";
    }
    set_message(app, text, false);
}

// Joue un coup (humain ou ordinateur) et met l'interface à jour
static void play_move(App& app, const std::function<std::string()>& move)
{
    Snapshot before = take_snapshot(*app.game);
    std::string error = move();
    if (!error.empty()) {
        set_message(app, error, true);
        return;
    }
    clear_selection(app);
    animate_events(app, before);
    show_new_events(app);
    Game& game = *app.game;
    // Changement de joueur : écran de passage si plusieurs humains en partie classique
    if (game.current_index() != app.last_player) {
        app.last_player = game.current_index();
        app.curtain = !game.is_over() && game.mode() == Mode::Classic && human_count(game) >= 2
                      && game.current_player().controller == Controller::Human;
    }
    app.ai_next_time = GetTime() + 0.8;
    if (game.is_over() && FileExists(SAVE_FILE)) {
        // Une partie finie ne se reprend plus
        std::remove(SAVE_FILE);
    }
}

// Repères des requêtes envoyées au serveur
enum NetTag { TagMe, TagPseudo, TagAuthPoll, TagCreate, TagJoin, TagLeave, TagStart, TagLobby, TagGame, TagMove,
              TagLeaderboard, TagLogout, TagDevStart };

const char* const SESSION_FILE = "session.txt";   // jeton de connexion
const char* const SERVER_FILE = "serveur.txt";    // adresse du serveur (facultatif)

// Un coup du joueur devant l'écran : appliqué tout de suite en local, envoyé
// au serveur en ligne (le serveur vérifie et renvoie la nouvelle partie)
static void submit_move(App& app, const Move& move)
{
    if (app.online.in_game) {
        if (app.online.move_pending) {
            return;
        }
        app.online.move_pending = true;
        app.net.send(TagMove, "POST", "/games/" + std::to_string(app.online.game_id) + "/move",
                     move_to_json(move).dump());
        return;
    }
    play_move(app, [&]() { return apply_move(*app.game, move); });
}

// ---------------------------------------------------------------- Accueil et nouvelle partie

static void start_new_game(App& app)
{
    Setup& setup = app.setup;
    std::vector<std::string> names;
    for (int p = 0; p < setup.count; p++) {
        names.push_back(setup.names[p].empty() ? "Joueur " + std::to_string(p + 1) : setup.names[p]);
    }
    app.game.reset(new Game(names, std::random_device{}(), setup.mode));
    app.player_colors.clear();
    for (int p = 0; p < setup.count; p++) {
        app.game->set_controller(p, setup.kinds[p]);
        app.player_colors.push_back(setup.colors[p]);
    }
    start_board(app);
}

static void resume_saved_game(App& app)
{
    std::unique_ptr<Game> loaded;
    std::string error = load_game_from_file(SAVE_FILE, loaded);
    if (!error.empty()) {
        set_message(app, error, true);
        return;
    }
    app.game = std::move(loaded);
    // Les couleurs ne sont pas dans la sauvegarde : couleurs par défaut
    app.player_colors.clear();
    for (int p = 0; p < static_cast<int>(app.game->players().size()); p++) {
        app.player_colors.push_back(p);
    }
    start_board(app);
    set_message(app, "Partie reprise. À " + app.game->current_player().name + " de jouer.", false);
}

// Nombre de caractères (UTF-8) d'un texte
static int utf8_length(const std::string& text)
{
    int count = 0;
    for (unsigned char c : text) {
        count += (c & 0xC0) != 0x80;
    }
    return count;
}

static void update_name_input(Setup& setup)
{
    if (setup.focused_name < 0) {
        return;
    }
    std::string& name = setup.names[setup.focused_name];
    int codepoint = GetCharPressed();
    while (codepoint > 0) {
        if (codepoint >= 32 && utf8_length(name) < 16) {
            int size = 0;
            const char* bytes = CodepointToUTF8(codepoint, &size);
            name.append(bytes, size);
        }
        codepoint = GetCharPressed();
    }
    if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && !name.empty()) {
        // Retire le dernier caractère complet (plusieurs octets pour un accent)
        while (!name.empty() && (static_cast<unsigned char>(name.back()) & 0xC0) == 0x80) {
            name.pop_back();
        }
        if (!name.empty()) {
            name.pop_back();
        }
    }
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        setup.focused_name = -1;
    } else if (IsKeyPressed(KEY_TAB)) {
        setup.focused_name = (setup.focused_name + 1) % setup.count;
    }
}

static void press_menu_button(App& app, const Button& button)
{
    Setup& setup = app.setup;
    switch (button.id) {
    case ButtonId::NewGame:
        app.message.clear();
        app.screen = Screen::Setup;
        break;
    case ButtonId::Resume:
        resume_saved_game(app);
        break;
    case ButtonId::Quit:
        app.quit = true;
        break;
    case ButtonId::Mode:
        setup.mode = button.value == 1 ? Mode::Classic : Mode::Quick;
        break;
    case ButtonId::Count:
        setup.count = button.value;
        // Une couleur déjà prise par un nouveau joueur : on lui en donne une libre
        for (int p = 0; p < setup.count; p++) {
            for (int other = 0; other < p; other++) {
                if (setup.colors[other] == setup.colors[p]) {
                    for (int c = 0; c < PLAYER_COLOR_COUNT; c++) {
                        bool used = false;
                        for (int k = 0; k < setup.count; k++) {
                            used = used || (k != p && setup.colors[k] == c);
                        }
                        if (!used) {
                            setup.colors[p] = c;
                            break;
                        }
                    }
                }
            }
        }
        break;
    case ButtonId::NameBox:
        setup.focused_name = button.value;
        break;
    case ButtonId::Swatch:
        setup.colors[button.value / 10] = button.value % 10;
        break;
    case ButtonId::Kind: {
        const Controller kinds[] = {Controller::Human, Controller::AiEasy, Controller::AiMedium};
        setup.kinds[button.value / 10] = kinds[button.value % 10];
        break;
    }
    case ButtonId::Back:
        setup.focused_name = -1;
        app.screen = Screen::Home;
        break;
    case ButtonId::Start:
        setup.focused_name = -1;
        start_new_game(app);
        break;
    default:
        break;
    }
}

// ---------------------------------------------------------------- En ligne

static std::string read_first_line(const std::string& path)
{
    std::ifstream file(path);
    std::string line;
    std::getline(file, line);
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
        line.pop_back();
    }
    return line;
}

static void save_session(const std::string& token)
{
    if (token.empty()) {
        std::remove(SESSION_FILE);
        return;
    }
    std::ofstream file(SESSION_FILE);
    file << token << "\n";
}

// Adresse du serveur : variable SPLENDOR_SERVER, sinon serveur.txt, sinon local
static void configure_network(App& app)
{
    const char* from_env = std::getenv("SPLENDOR_SERVER");
    std::string server = from_env != nullptr ? from_env : read_first_line(SERVER_FILE);
    app.net.set_server(server.empty() ? "http://localhost:8080" : server);
    app.net.set_token(read_first_line(SESSION_FILE));
    const char* dev = std::getenv("SPLENDOR_DEV");
    app.online.dev_login = dev != nullptr && std::string(dev) == "1";
}

static void go_online(App& app)
{
    app.message.clear();
    if (read_first_line(SESSION_FILE).empty()) {
        app.screen = Screen::Login;
        return;
    }
    set_message(app, "Connexion au serveur…", false);
    app.net.send(TagMe, "GET", "/me");
}

static void start_login(App& app, const std::string& provider)
{
    app.online.login_id = random_login_id();
    app.online.next_login_poll = GetTime() + 2;
    app.message.clear();
    if (provider == "dev") {
        std::string name = "test-" + app.online.login_id.substr(0, 6);
        app.net.send(TagDevStart, "GET", "/auth/dev/start?login=" + app.online.login_id + "&name=" + name);
        app.online.next_login_poll = GetTime() + 0.5;
        return;
    }
    std::string url = app.net.server() + "/auth/" + provider + "/start?login=" + app.online.login_id;
    OpenURL(url.c_str());
}

static void poll_lobby(App& app)
{
    OnlineState& online = app.online;
    if (online.lobby_poll_running || online.lobby_code.empty() || GetTime() < online.next_game_poll) {
        return;
    }
    online.lobby_poll_running = true;
    app.net.send(TagLobby, "GET", "/lobbies/" + online.lobby_code + "?since=" + std::to_string(online.lobby_version));
}

static void poll_game(App& app)
{
    OnlineState& online = app.online;
    if (online.game_poll_running || online.game_id == 0 || GetTime() < online.next_game_poll) {
        return;
    }
    online.game_poll_running = true;
    app.net.send(TagGame, "GET", "/games/" + std::to_string(online.game_id) + "?since=" + std::to_string(online.game_version));
}

static void enter_online_game(App& app, int game_id)
{
    OnlineState& online = app.online;
    online.game_id = game_id;
    online.game_version = 0;
    online.game_over = false;
    online.move_pending = false;
    online.log.clear();
    online.lobby_code.clear();
    online.in_game = true;
    app.game.reset();
    app.screen = Screen::Board;
    set_message(app, "Chargement de la partie…", false);
    poll_game(app);
}

// Nouvel état de la partie envoyé par le serveur
static void apply_game_state(App& app, const nlohmann::json& body)
{
    OnlineState& online = app.online;
    long version = body.value("version", 0L);
    if (version <= online.game_version && app.game) {
        return;
    }
    std::unique_ptr<Game> loaded;
    std::string error = load_game(body.value("save", std::string()), loaded);
    if (!error.empty()) {
        set_message(app, error, true);
        return;
    }
    bool first = !app.game;
    int previous_player = app.game ? app.game->current_index() : -1;
    app.game = std::move(loaded);
    online.game_version = version;
    online.my_seat = body.value("your_seat", -1);
    online.game_over = body.value("over", false);
    online.elo_changes = body.value("elo_changes", std::vector<int>());
    online.seat_elos.clear();
    for (const nlohmann::json& seat : body["seats"]) {
        online.seat_elos.push_back(seat.value("elo", 1000));
    }
    std::vector<std::string> log = body.value("log", std::vector<std::string>());
    std::string news;
    for (size_t i = online.log.size(); i < log.size(); i++) {
        news += log[i] + " ";
    }
    online.log = log;
    if (first) {
        start_board(app);
        app.player_colors.clear();
        for (int p = 0; p < static_cast<int>(app.game->players().size()); p++) {
            app.player_colors.push_back(p);
        }
    }
    if (app.game->current_index() != previous_player) {
        clear_selection(app);
    }
    std::string next;
    if (online.game_over) {
        next = "Partie terminée.";
        app.net.send(TagMe, "GET", "/me");  // nouvel Elo
    } else if (app.game->current_index() == online.my_seat) {
        next = app.game->step() == TurnStep::Discard ? "Défaussez vos jetons en trop." : "À vous de jouer !";
    } else {
        next = "Au tour de " + app.game->current_player().name + ".";
    }
    set_message(app, news + next, false);
}

static std::string error_of(const NetResponse& response, const nlohmann::json& body, const App& app)
{
    if (response.status == 0) {
        return "Serveur injoignable (" + app.net.server() + ").";
    }
    if (body.is_object() && body.contains("error") && body["error"].is_string()) {
        return body["error"].get<std::string>();
    }
    return "Erreur du serveur (" + std::to_string(response.status) + ").";
}

static void handle_responses(App& app)
{
    OnlineState& online = app.online;
    for (const NetResponse& response : app.net.poll()) {
        nlohmann::json body = nlohmann::json::parse(response.body, nullptr, false);
        bool ok = response.status == 200 && body.is_object();
        switch (response.tag) {
        case TagMe:
            if (ok) {
                online.account = body.value("id", 0);
                online.pseudo = body.value("pseudo", std::string());
                online.elo = body.value("elo", 1000);
                online.games = body.value("games", 0);
                online.wins = body.value("wins", 0);
                if (online.pseudo.empty()) {
                    app.screen = Screen::Pseudo;
                    online.focus = 1;
                } else if (app.screen != Screen::Board && app.screen != Screen::Lobby) {
                    app.screen = Screen::Online;
                }
                if (app.screen != Screen::Board) {
                    app.message.clear();
                }
            } else if (response.status == 401) {
                save_session("");
                app.net.set_token("");
                app.screen = Screen::Login;
                set_message(app, "Session expirée : reconnectez-vous.", true);
            } else {
                set_message(app, error_of(response, body, app), true);
            }
            break;
        case TagAuthPoll:
            if (online.login_id.empty()) {
                break;
            }
            if (ok && body.value("status", std::string()) == "done") {
                std::string token = body.value("token", std::string());
                online.login_id.clear();
                save_session(token);
                app.net.set_token(token);
                app.net.send(TagMe, "GET", "/me");
            } else if (ok && body.value("status", std::string()) == "error") {
                online.login_id.clear();
                set_message(app, body.value("error", std::string("Connexion impossible.")), true);
            } else if (!ok) {
                online.login_id.clear();
                set_message(app, error_of(response, body, app), true);
            }
            break;
        case TagPseudo:
            if (ok) {
                app.net.send(TagMe, "GET", "/me");
            } else {
                set_message(app, error_of(response, body, app), true);
            }
            break;
        case TagCreate:
        case TagJoin:
            if (ok) {
                online.lobby_code = response.tag == TagCreate ? body.value("code", std::string()) : online.code_input;
                online.lobby_version = 0;
                online.lobby_members.clear();
                app.screen = Screen::Lobby;
                app.message.clear();
                poll_lobby(app);
            } else {
                set_message(app, error_of(response, body, app), true);
            }
            break;
        case TagStart:
            if (!ok) {
                set_message(app, error_of(response, body, app), true);
            }
            break;
        case TagLobby:
            online.lobby_poll_running = false;
            if (app.screen != Screen::Lobby || !ok || body.value("code", std::string()) != online.lobby_code) {
                if (app.screen == Screen::Lobby && response.status == 404) {
                    online.lobby_code.clear();
                    app.screen = Screen::Online;
                    set_message(app, "Le salon n'existe plus.", true);
                } else if (app.screen == Screen::Lobby && !ok) {
                    online.next_game_poll = GetTime() + 2;  // nouvelle tentative
                }
                break;
            }
            online.lobby_version = body.value("version", 0L);
            online.lobby_host = body.value("host", 0);
            online.lobby_seats = body.value("seats", 2);
            online.lobby_mode = body.value("mode", std::string());
            online.lobby_members.clear();
            online.lobby_elos.clear();
            for (const nlohmann::json& member : body["members"]) {
                online.lobby_members.push_back(member.value("pseudo", std::string()));
                online.lobby_elos.push_back(member.value("elo", 1000));
            }
            if (body.value("game_id", 0) > 0) {
                enter_online_game(app, body.value("game_id", 0));
            } else {
                poll_lobby(app);
            }
            break;
        case TagGame:
            online.game_poll_running = false;
            if (!online.in_game) {
                break;
            }
            if (ok) {
                apply_game_state(app, body);
                if (!online.game_over) {
                    poll_game(app);
                }
            } else if (response.status == 404) {
                online.in_game = false;
                online.game_id = 0;
                app.screen = Screen::Online;
                set_message(app, "Cette partie n'existe plus.", true);
            } else {
                online.next_game_poll = GetTime() + 2;
                set_message(app, error_of(response, body, app) + " Nouvelle tentative…", true);
            }
            break;
        case TagMove:
            online.move_pending = false;
            if (ok) {
                clear_selection(app);
            } else {
                set_message(app, error_of(response, body, app), true);
            }
            break;
        case TagLeaderboard:
            if (ok) {
                online.board_names.clear();
                online.board_elos.clear();
                online.board_games.clear();
                online.board_wins.clear();
                for (const nlohmann::json& player : body["players"]) {
                    online.board_names.push_back(player.value("pseudo", std::string()));
                    online.board_elos.push_back(player.value("elo", 1000));
                    online.board_games.push_back(player.value("games", 0));
                    online.board_wins.push_back(player.value("wins", 0));
                }
            } else {
                set_message(app, error_of(response, body, app), true);
            }
            break;
        default:
            break;
        }
    }
}

// Saisie dans un champ texte. `code` : majuscules et chiffres seulement.
static void edit_text(std::string& text, int max_chars, bool code)
{
    int codepoint = GetCharPressed();
    while (codepoint > 0) {
        if (code && codepoint >= 'a' && codepoint <= 'z') {
            codepoint -= 32;
        }
        bool allowed = code ? ((codepoint >= 'A' && codepoint <= 'Z') || (codepoint >= '0' && codepoint <= '9'))
                            : codepoint >= 32;
        if (allowed && utf8_length(text) < max_chars) {
            int size = 0;
            const char* bytes = CodepointToUTF8(codepoint, &size);
            text.append(bytes, size);
        }
        codepoint = GetCharPressed();
    }
    if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && !text.empty()) {
        while (!text.empty() && (static_cast<unsigned char>(text.back()) & 0xC0) == 0x80) {
            text.pop_back();
        }
        if (!text.empty()) {
            text.pop_back();
        }
    }
}

static void press_online_button(App& app, const Button& button)
{
    OnlineState& online = app.online;
    switch (button.id) {
    case ButtonId::PlayOnline:
        go_online(app);
        break;
    case ButtonId::LoginDiscord:
        start_login(app, "discord");
        break;
    case ButtonId::LoginGoogle:
        start_login(app, "google");
        break;
    case ButtonId::LoginDev:
        start_login(app, "dev");
        break;
    case ButtonId::BackHome:
        online.login_id.clear();
        app.message.clear();
        app.screen = Screen::Home;
        break;
    case ButtonId::PseudoBox:
        online.focus = 1;
        break;
    case ButtonId::PseudoOk:
        app.net.send(TagPseudo, "POST", "/me/pseudo", nlohmann::json({{"pseudo", online.pseudo_input}}).dump());
        break;
    case ButtonId::CodeBox:
        online.focus = 2;
        break;
    case ButtonId::CreateMode:
        online.create_mode = button.value == 1 ? Mode::Classic : Mode::Quick;
        break;
    case ButtonId::CreateSeats:
        online.create_seats = button.value;
        break;
    case ButtonId::CreateLobby: {
        nlohmann::json body = {{"mode", online.create_mode == Mode::Classic ? "classique" : "rapide"},
                               {"seats", online.create_seats}};
        app.net.send(TagCreate, "POST", "/lobbies", body.dump());
        break;
    }
    case ButtonId::JoinLobby:
        app.net.send(TagJoin, "POST", "/lobbies/" + online.code_input + "/join");
        break;
    case ButtonId::ShowLeaderboard:
        online.board_names.clear();
        app.screen = Screen::Leaderboard;
        app.net.send(TagLeaderboard, "GET", "/leaderboard");
        break;
    case ButtonId::Logout:
        app.net.send(TagLogout, "POST", "/auth/logout");
        save_session("");
        app.net.set_token("");
        online = OnlineState();
        configure_network(app);
        app.screen = Screen::Home;
        set_message(app, "Vous êtes déconnecté.", false);
        break;
    case ButtonId::BackOnline:
        online.in_game = false;
        app.screen = Screen::Online;
        app.message.clear();
        app.net.send(TagMe, "GET", "/me");
        break;
    case ButtonId::ReturnToGame:
        enter_online_game(app, online.game_id);
        break;
    case ButtonId::CopyCode:
        SetClipboardText(online.lobby_code.c_str());
        set_message(app, "Code copié.", false);
        break;
    case ButtonId::LobbyStart:
        app.net.send(TagStart, "POST", "/lobbies/" + online.lobby_code + "/start");
        break;
    case ButtonId::LobbyLeave:
        app.net.send(TagLeave, "POST", "/lobbies/" + online.lobby_code + "/leave");
        online.lobby_code.clear();
        app.screen = Screen::Online;
        app.message.clear();
        break;
    default:
        break;
    }
}

// ---------------------------------------------------------------- Plateau

static void save_current_game(App& app)
{
    std::string error = save_game_to_file(*app.game, SAVE_FILE);
    if (error.empty()) {
        set_message(app, std::string("Partie sauvegardée (") + SAVE_FILE + ").", false);
    } else {
        set_message(app, error, true);
    }
}

static void press_board_button(App& app, ButtonId id)
{
    Game& game = *app.game;
    switch (id) {
    case ButtonId::Take:
        submit_move(app, take_move(app.selection));
        break;
    case ButtonId::Buy:
        if (app.target == Target::Visible) {
            submit_move(app, buy_move(app.target_level, app.target_index + 1));
        } else if (app.target == Target::Reserved) {
            submit_move(app, buy_reserved_move(app.target_index + 1));
        }
        break;
    case ButtonId::Reserve: {
        int position = app.target == Target::Deck ? 0 : app.target_index + 1;
        submit_move(app, reserve_move(app.target_level, position));
        break;
    }
    case ButtonId::Cancel:
        clear_selection(app);
        break;
    case ButtonId::Save:
        save_current_game(app);
        break;
    case ButtonId::Journal:
        app.journal_open = !app.journal_open;
        break;
    case ButtonId::Menu:
        // On ne perd pas la partie en cours : elle est sauvegardée
        if (!game.is_over()) {
            save_current_game(app);
        } else {
            app.message.clear();
        }
        app.screen = Screen::Home;
        break;
    case ButtonId::Replay:
        app.screen = Screen::Setup;
        break;
    case ButtonId::BackOnline:
        press_online_button(app, {ButtonId::BackOnline, "", true, {}, 0, false});
        break;
    default:
        break;
    }
}

// Clic sur un jeton de la réserve : 0 -> 1 -> 2 -> 0 jeton de cette couleur
static void toggle_token(App& app, int gem)
{
    const Game& game = *app.game;
    if (gem == GOLD_INDEX) {
        set_message(app, "L'or ne se prend pas : on le reçoit en réservant une carte.", true);
        return;
    }
    if (game.bank()[gem] == 0) {
        set_message(app, "Il n'y a plus de jetons " + color_name(color_at(gem)) + ".", true);
        return;
    }
    app.target = Target::None;
    app.selection[gem] = (app.selection[gem] + 1) % 3;
    app.message.clear();
}

static void click_action_step(App& app, Vector2 mouse)
{
    const Game& game = *app.game;
    int gems = game.mode() == Mode::Classic ? GOLD_INDEX : COLOR_COUNT - 1;
    for (int gem = 0; gem <= gems; gem++) {
        Vector2 center = bank_token_center(gem);
        if (std::hypot(mouse.x - center.x, mouse.y - center.y) <= BANK_TOKEN_RADIUS) {
            toggle_token(app, gem);
            return;
        }
    }
    for (int level = 1; level <= LEVEL_COUNT; level++) {
        for (int i = 0; i < static_cast<int>(game.visible(level).size()); i++) {
            if (point_in(mouse, card_rect(level, i))) {
                clear_selection(app);
                app.target = Target::Visible;
                app.target_level = level;
                app.target_index = i;
                app.message.clear();
                return;
            }
        }
        if (point_in(mouse, deck_rect(level))) {
            if (game.mode() != Mode::Classic) {
                set_message(app, "La pioche ne sert qu'à réserver (partie classique).", true);
            } else if (game.deck_size(level) > 0) {
                clear_selection(app);
                app.target = Target::Deck;
                app.target_level = level;
                app.message.clear();
            }
            return;
        }
    }
    for (int i = 0; i < static_cast<int>(game.current_player().reserved.size()); i++) {
        if (point_in(mouse, reserved_rect(i))) {
            clear_selection(app);
            app.target = Target::Reserved;
            app.target_index = i;
            app.message.clear();
            return;
        }
    }
}

static void discard_gem(App& app, int gem)
{
    submit_move(app, discard_move(gem));
}

static void click_board(App& app, Vector2 mouse)
{
    Game& game = *app.game;
    if (game.step() == TurnStep::Action) {
        click_action_step(app, mouse);
    } else if (game.step() == TurnStep::Discard) {
        for (int gem = 0; gem <= GOLD_INDEX; gem++) {
            Vector2 center = player_token_center(game.current_index(), gem);
            if (std::hypot(mouse.x - center.x, mouse.y - center.y) <= PLAYER_TOKEN_RADIUS) {
                discard_gem(app, gem);
                return;
            }
        }
    } else if (game.step() == TurnStep::ChooseNoble) {
        for (int choice : game.noble_choices()) {
            if (point_in(mouse, noble_rect(choice))) {
                submit_move(app, noble_move(choice));
                return;
            }
        }
    }
}

// Raccourcis clavier pendant le tour d'un humain
static void board_keys(App& app)
{
    Game& game = *app.game;
    const int digit_keys[] = {KEY_ONE, KEY_TWO, KEY_THREE, KEY_FOUR, KEY_FIVE, KEY_SIX};
    if (game.step() == TurnStep::Discard) {
        for (int gem = 0; gem <= GOLD_INDEX; gem++) {
            if (IsKeyPressed(digit_keys[gem])) {
                discard_gem(app, gem);
            }
        }
        return;
    }
    if (game.step() != TurnStep::Action) {
        return;
    }
    for (int gem = 0; gem < COLOR_COUNT; gem++) {
        if (IsKeyPressed(digit_keys[gem])) {
            toggle_token(app, gem);
        }
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        clear_selection(app);
    }
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        if (total(app.selection) > 0) {
            press_board_button(app, ButtonId::Take);
        } else if (app.target == Target::Visible || app.target == Target::Reserved) {
            press_board_button(app, ButtonId::Buy);
        }
    }
    if (IsKeyPressed(KEY_R) && (app.target == Target::Visible || app.target == Target::Deck)) {
        press_board_button(app, ButtonId::Reserve);
    }
}

static void update_board(App& app, Vector2 mouse)
{
    Game& game = *app.game;
    double now = GetTime();

    // Fin des animations
    std::vector<Animation> running;
    for (const Animation& anim : app.animations) {
        if (now < anim.start + anim.duration) {
            running.push_back(anim);
        }
    }
    app.animations = running;
    std::vector<Flip> flipping;
    for (const Flip& flip : app.flips) {
        if (now < flip.start + 0.5) {
            flipping.push_back(flip);
        }
    }
    app.flips = flipping;

    bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    if (ctrl && IsKeyPressed(KEY_S) && !game.is_over() && !app.online.in_game) {
        save_current_game(app);
    }
    if (IsKeyPressed(KEY_J) && !game.is_over()) {
        app.journal_open = !app.journal_open;
        return;
    }

    // Boutons (toujours actifs)
    if (clicked) {
        for (const Button& button : visible_buttons(app)) {
            if (point_in(mouse, button.rect)) {
                if (button.enabled) {
                    press_board_button(app, button.id);
                }
                return;
            }
        }
    }
    if (app.journal_open) {
        if (clicked || IsKeyPressed(KEY_ESCAPE)) {
            app.journal_open = false;
        }
        return;
    }
    if (app.curtain) {
        if (clicked || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || IsKeyPressed(KEY_SPACE)) {
            app.curtain = false;
        }
        return;
    }
    if (game.is_over()) {
        return;
    }

    // Tour de l'ordinateur : un coup toutes les 0,8 s, après les animations
    if (ai_to_play(game)) {
        if (app.animations.empty() && now >= app.ai_next_time) {
            play_move(app, [&]() { return ai_play(game); });
        }
        return;
    }

    board_keys(app);
    if (clicked) {
        click_board(app, mouse);
    }
}

static void update(App& app, Vector2 mouse)
{
    OnlineState& online = app.online;
    handle_responses(app);
    // Connexion en cours : on demande au serveur si le navigateur a fini
    if (!online.login_id.empty() && GetTime() >= online.next_login_poll) {
        online.next_login_poll = GetTime() + 1.5;
        app.net.send(TagAuthPoll, "GET", "/auth/poll?login=" + online.login_id);
    }
    if (app.screen == Screen::Lobby) {
        poll_lobby(app);
    }
    if (online.in_game) {
        poll_game(app);
    }
    if (app.screen == Screen::Board && online.in_game && !app.game) {
        return;  // la partie arrive du serveur
    }
    if (app.screen == Screen::Board && app.game) {
        update_board(app, mouse);
        return;
    }
    if (online.focus == 1 && app.screen == Screen::Pseudo) {
        edit_text(online.pseudo_input, 16, false);
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
            press_online_button(app, {ButtonId::PseudoOk, "", true, {}, 0, false});
        }
    }
    if (online.focus == 2 && app.screen == Screen::Online) {
        edit_text(online.code_input, 5, true);
        if ((IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) && online.code_input.size() == 5) {
            press_online_button(app, {ButtonId::JoinLobby, "", true, {}, 0, false});
        }
    }
    if (app.screen == Screen::Setup) {
        update_name_input(app.setup);
        if (IsKeyPressed(KEY_ESCAPE) && app.setup.focused_name < 0) {
            app.screen = Screen::Home;
            return;
        }
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        bool on_button = false;
        for (const Button& button : visible_buttons(app)) {
            if (point_in(mouse, button.rect)) {
                on_button = true;
                if (button.enabled) {
                    if (app.screen == Screen::Home || app.screen == Screen::Setup) {
                        press_menu_button(app, button);
                    }
                    if (app.screen != Screen::Setup) {
                        press_online_button(app, button);
                    }
                }
                break;
            }
        }
        // Clic ailleurs : on quitte la saisie
        if (!on_button) {
            app.setup.focused_name = -1;
            online.focus = 0;
        }
    }
}

// ---------------------------------------------------------------- Fenêtre

// Mise à l'échelle : le plateau garde ses proportions et reste centré
static Camera2D board_camera()
{
    float width = static_cast<float>(GetScreenWidth());
    float height = static_cast<float>(GetScreenHeight());
    float scale = std::fmin(width / BOARD_WIDTH, height / BOARD_HEIGHT);
    Camera2D camera = {};
    camera.offset = {(width - BOARD_WIDTH * scale) / 2, (height - BOARD_HEIGHT * scale) / 2};
    camera.target = {0, 0};
    camera.rotation = 0;
    camera.zoom = scale;
    return camera;
}

static void prepare_font(Font& font)
{
    GenTextureMipmaps(&font.texture);
    SetTextureFilter(font.texture, TEXTURE_FILTER_TRILINEAR);
}

int main()
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    std::string title = std::string("Splendor ") + SPLENDOR_VERSION;
    InitWindow(1280, 800, title.c_str());
    SetWindowMinSize(640, 400);
    SetExitKey(KEY_NULL);  // Échap sert à annuler, pas à quitter
    SetTargetFPS(60);

    App app;
    configure_network(app);
    app.font = load_font_regular();
    app.bold = load_font_bold();
    prepare_font(app.font);
    prepare_font(app.bold);

    while (!WindowShouldClose() && !app.quit) {
        Camera2D camera = board_camera();
        Vector2 mouse = GetScreenToWorld2D(GetMousePosition(), camera);
        update(app, mouse);

        BeginDrawing();
        ClearBackground({5, 20, 13, 255});
        BeginMode2D(camera);
        draw_app(app, mouse);
        EndMode2D();
        EndDrawing();
    }

    // Les glyphes des polices sont des tableaux statiques : on ne libère que la texture
    UnloadTexture(app.font.texture);
    UnloadTexture(app.bold.texture);
    CloseWindow();
    return 0;
}
