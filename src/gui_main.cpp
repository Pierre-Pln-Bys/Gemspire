// Interface graphique de Gemspire (raylib).
// Ce fichier gère la fenêtre, les écrans, la souris, le clavier et
// l'enchaînement des coups. Le dessin est dans gui_draw.cpp.
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
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

#ifdef __EMSCRIPTEN__
// Version navigateur : boucle confiée au navigateur, saisie et fichiers adaptés
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

#include "platform_web.hpp"
#endif

// ---------------------------------------------------------------- Outils

// Ligne d'état ; un texte anglais simple (règles, serveur) est traduit ici
static void set_message(App& app, const std::string& message, bool is_error)
{
    app.message = tr(app.lang, message);
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
    app.banners.clear();
    app.ai_next_time = GetTime() + 0.8;
    set_message(app, tr(app.lang, "{0}'s turn.", {app.game->current_player().name}), false);
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

// Ce qu'il faut comparer avant et après un coup pour faire les annonces
struct Highlights {
    std::vector<int> nobles;   // nombre de nobles de chaque joueur
    bool last_round = false;
};

static Highlights take_highlights(const Game& game)
{
    Highlights h;
    for (const Player& player : game.players()) {
        h.nobles.push_back(static_cast<int>(player.nobles.size()));
    }
    h.last_round = game.is_last_round();
    return h;
}

// Annonces : un noble rend visite à un joueur, quelqu'un atteint 15 points.
// Fonctionne aussi en ligne (état rechargé depuis le serveur, sans événements).
static void announce_changes(App& app, const Highlights& before)
{
    const Game& game = *app.game;
    const std::vector<Player>& players = game.players();
    for (int p = 0; p < static_cast<int>(players.size()) && p < static_cast<int>(before.nobles.size()); p++) {
        for (int n = before.nobles[p]; n < static_cast<int>(players[p].nobles.size()); n++) {
            const Noble& noble = players[p].nobles[n];
            app.banners.push_back({tr(app.lang, "{0} visits {1}", {noble.name, players[p].name}),
                                   tr(app.lang, "+{0} prestige points", {std::to_string(noble.points)}), p});
        }
    }
    if (game.is_last_round() && !before.last_round && !game.is_over()) {
        int leader = 0;
        for (int p = 0; p < static_cast<int>(players.size()); p++) {
            if (players[p].score() > players[leader].score()) {
                leader = p;
            }
        }
        app.banners.push_back({"Last round!",
                               tr(app.lang, "{0} reached {1} points: the round is finished.",
                                  {players[leader].name, std::to_string(WINNING_SCORE)}),
                               leader});
    }
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
        text += describe_event(game, game.events()[app.shown_events], app.lang) + " ";
    }
    if (game.is_over()) {
        text += tr(app.lang, "Game over.");
    } else if (game.step() == TurnStep::Discard) {
        text += tr(app.lang, "More than 10 tokens: click your tokens (or keys 1 to 6) to discard {0}.",
                   {std::to_string(game.tokens_to_discard())});
    } else if (game.step() == TurnStep::ChooseNoble) {
        text += tr(app.lang, "Several nobles want to visit: click the one you choose.");
    } else {
        text += tr(app.lang, "{0}'s turn.", {game.current_player().name});
    }
    set_message(app, text, false);
}

// Joue un coup (humain ou ordinateur) et met l'interface à jour
static void play_move(App& app, const std::function<std::string()>& move)
{
    Snapshot before = take_snapshot(*app.game);
    Highlights highlights = take_highlights(*app.game);
    std::string error = move();
    if (!error.empty()) {
        set_message(app, error, true);
        return;
    }
    clear_selection(app);
    animate_events(app, before);
    show_new_events(app);
    announce_changes(app, highlights);
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
              TagLeaderboard, TagLogout, TagDevStart, TagConfig, TagNotes, TagMeQuiet, TagActivity, TagActivityLobby, TagChat, TagFriends, TagFriendChange, TagLive, TagHistory, TagReplay, TagMatch, TagMatchLeave, TagLink, TagBegin, TagPass, TagEquip };

const char* const DOWNLOAD_URL = "https://github.com/Pierre-Pln-Bys/Gemspire/releases/latest";

const char* const SESSION_FILE = "session.txt";   // jeton de connexion
const char* const SERVER_FILE = "serveur.txt";    // adresse du serveur (facultatif)
const char* const LANGUAGE_FILE = "langue.txt";   // langue choisie ("en" ou "fr")
const char* const THEME_FILE = "theme.txt";       // thème choisi (numéro)
const char* const LOOK_FILE = "apparence.txt";    // objets du passe portés sur cet écran (thème, dos, jetons)

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
        names.push_back(setup.names[p].empty() ? tr(app.lang, "Player {0}", {std::to_string(p + 1)}) : setup.names[p]);
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
    set_message(app, tr(app.lang, "Game resumed. {0}'s turn.", {app.game->current_player().name}), false);
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

// Change la langue et la retient (fichier langue.txt). Les noms de joueurs
// encore par défaut (« Player 1 » / « Joueur 1 ») suivent la langue.
static void set_language(App& app, Lang lang)
{
    for (int p = 0; p < 4; p++) {
        if (app.setup.names[p] == tr(app.lang, "Player {0}", {std::to_string(p + 1)})) {
            app.setup.names[p] = tr(lang, "Player {0}", {std::to_string(p + 1)});
        }
    }
    app.lang = lang;
    {
        std::ofstream file(LANGUAGE_FILE);
        file << lang_code(lang) << "\n";
    }
#ifdef __EMSCRIPTEN__
    web_save_files();
#endif
}

// Change le thème du plateau et le retient (fichier theme.txt)
static void set_theme(App& app, int theme)
{
    app.theme = theme;
    {
        std::ofstream file(THEME_FILE);
        file << theme << "\n";
    }
#ifdef __EMSCRIPTEN__
    web_save_files();
#endif
}

// Objets du passe qui ne concernent que cet écran (thème, dos de cartes,
// jetons) : appliqués et gardés dans apparence.txt pour jouer hors ligne
static void set_local_look(App& app, const std::string& theme, const std::string& cardback, const std::string& tokens)
{
    Cosmetic cosmetic;
    app.look_theme = find_cosmetic(theme, cosmetic) && cosmetic.kind == CosmeticKind::Theme ? theme : "";
    if (!app.look_theme.empty()) {
        app.pass_theme = theme_from_cosmetic(cosmetic);
    }
    app.look_cardback = find_cosmetic(cardback, cosmetic) && cosmetic.kind == CosmeticKind::CardBack ? cardback : "";
    app.look_tokens = find_cosmetic(tokens, cosmetic) && cosmetic.kind == CosmeticKind::Tokens ? tokens : "";
    {
        std::ofstream file(LOOK_FILE);
        file << app.look_theme << "\n" << app.look_cardback << "\n" << app.look_tokens << "\n";
    }
#ifdef __EMSCRIPTEN__
    web_save_files();
#endif
}

// Objets portés envoyés par le serveur ({"frame": code, ...})
static Look look_from_json(const nlohmann::json& body)
{
    Look look;
    if (body.is_object()) {
        look.frame = body.value("frame", std::string());
        look.title = body.value("title", std::string());
        look.avatar = body.value("avatar", std::string());
    }
    return look;
}

// Tout ce que je porte, d'après la liste du serveur (GET /me/pass, POST /me/equip)
static void apply_equipped(App& app, const nlohmann::json& equipped)
{
    app.online.look = look_from_json(equipped);
    if (equipped.is_object()) {
        set_local_look(app, equipped.value("theme", std::string()), equipped.value("cardback", std::string()),
                       equipped.value("tokens", std::string()));
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
    case ButtonId::Language:
        set_language(app, app.lang == Lang::French ? Lang::English : Lang::French);
        break;
    case ButtonId::ShowRules:
        app.message.clear();
        app.screen = Screen::Rules;
        break;
    case ButtonId::NextTheme:
        // Thème du passe porté : on revient d'abord aux thèmes de base
        if (!app.look_theme.empty()) {
            set_local_look(app, "", app.look_cardback, app.look_tokens);
            if (!app.online.pseudo.empty()) {
                app.net.send(TagEquip, "POST", "/me/equip", nlohmann::json({{"kind", "theme"}, {"code", ""}}).dump());
            }
        } else {
            set_theme(app, (app.theme + 1) % THEME_COUNT);
        }
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
#ifdef __EMSCRIPTEN__
        // Navigateur : boîte de saisie (ouvre le clavier du téléphone)
        web_prompt(tr(app.lang, "Player {0} name:", {std::to_string(button.value + 1)}), setup.names[button.value]);
        setup.names[button.value] = setup.names[button.value].substr(0, 32);
        setup.focused_name = -1;
#endif
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
    {
        std::ofstream file(SESSION_FILE);
        file << token << "\n";
    }
#ifdef __EMSCRIPTEN__
    web_save_files();
#endif
}

// Adresse du serveur : variable SPLENDOR_SERVER, sinon serveur.txt, sinon le serveur officiel
static void configure_network(App& app)
{
    const char* from_env = std::getenv("SPLENDOR_SERVER");
    std::string server = from_env != nullptr ? from_env : read_first_line(SERVER_FILE);
    app.net.set_server(server.empty() ? DEFAULT_SERVER : server);
#ifdef __EMSCRIPTEN__
    app.net.set_server(web_origin());  // le serveur qui a servi la page
#endif
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
    set_message(app, "Connecting to the server…", false);
    app.net.send(TagMe, "GET", "/me");
}

static void start_login(App& app, const std::string& provider)
{
    app.online.login_id = random_login_id();
    app.online.login_verifier = random_login_id();  // secret : il faudra le présenter à /auth/poll
    app.online.next_login_poll = GetTime() + 2;
    app.message.clear();
    if (provider == "dev") {
        // Connexion de test (locale) : le secret peut passer par l'URL ici.
        std::string name = "test-" + app.online.login_id.substr(0, 6);
        app.net.send(TagDevStart, "GET", "/auth/dev/start?login=" + app.online.login_id + "&name=" + name
                                             + "&verifier=" + app.online.login_verifier);
        app.online.next_login_poll = GetTime() + 0.5;
        return;
    }
    // On enregistre d'abord le secret sur notre propre canal (POST /auth/begin) ;
    // le navigateur n'est ouvert qu'au retour de cette requête (TagBegin), pour
    // que le secret ne transite jamais par une URL de navigateur.
    app.online.login_provider = provider;
    app.net.send(TagBegin, "POST", "/auth/begin",
                 nlohmann::json({{"provider", provider},
                                 {"login", app.online.login_id},
                                 {"verifier", app.online.login_verifier}})
                     .dump());
}

static void poll_lobby(App& app)
{
    OnlineState& online = app.online;
    if (online.lobby_poll_running || online.lobby_code.empty() || GetTime() < online.next_game_poll) {
        return;
    }
    online.lobby_poll_running = true;
    app.net.send(TagLobby, "GET", "/lobbies/" + online.lobby_code + "?since=" + std::to_string(online.lobby_version), "",
                 true);
}

static void poll_game(App& app)
{
    OnlineState& online = app.online;
    if (online.game_poll_running || online.game_id == 0 || GetTime() < online.next_game_poll) {
        return;
    }
    online.game_poll_running = true;
    app.net.send(TagGame, "GET", "/games/" + std::to_string(online.game_id) + "?since=" + std::to_string(online.game_version),
                 "", true);
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
    online.chat.clear();
    online.chat_seen = 0;
    online.chat_open = false;
    online.chat_toast_until = 0;
    for (bool& muted : online.muted) {
        muted = false;
    }
    online.in_game = true;
    app.game.reset();
    app.screen = Screen::Board;
    set_message(app, "Loading the game…", false);
    poll_game(app);
}

// Nouvel état de la partie envoyé par le serveur
static void apply_game_state(App& app, const nlohmann::json& body)
{
    OnlineState& online = app.online;
    if (app.replay.active || !online.in_game) {
        return;  // une rediffusion, ou on a quitté la partie : la réponse arrive trop tard
    }
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
    Highlights highlights = app.game ? take_highlights(*app.game) : take_highlights(*loaded);
    app.game = std::move(loaded);
    announce_changes(app, highlights);
    online.game_version = version;
    online.my_seat = body.value("your_seat", -1);
    online.game_over = body.value("over", false);
    online.turn_deadline = GetTime() + body.value("turn_seconds_left", 0);
    online.elo_changes = body.value("elo_changes", std::vector<int>());
    online.game_kind = body.value("kind", std::string("friendly"));
    online.replay_id = body.value("replay_id", 0);
    online.new_achievements.clear();
    // Passe de saison : ce que la partie m'a rapporté (à la fin)
    online.gain_xp = -1;
    online.gain_unlocked.clear();
    if (body.contains("pass") && body["pass"].is_object()) {
        const nlohmann::json& gain = body["pass"];
        online.gain_xp = gain.value("xp", 0);
        online.gain_tier_before = gain.value("tier_before", 0);
        online.gain_tier_after = gain.value("tier_after", 0);
        online.gain_unlocked = gain.value("unlocked", std::vector<std::string>());
        for (const std::string& code : online.gain_unlocked) {
            if (std::find(online.owned.begin(), online.owned.end(), code) == online.owned.end()) {
                online.owned.push_back(code);
            }
        }
    }
    if (body.contains("achievements") && body["achievements"].is_array() && online.my_seat >= 0
        && online.my_seat < static_cast<int>(body["achievements"].size())) {
        online.new_achievements = body["achievements"][online.my_seat].get<std::vector<std::string>>();
    }
    online.seat_elos.clear();
    online.seat_looks.clear();
    for (const nlohmann::json& seat : body["seats"]) {
        online.seat_elos.push_back(seat.value("elo", 1000));
        online.seat_looks.push_back(look_from_json(seat.value("look", nlohmann::json::object())));
    }
    std::vector<std::string> log = body.value("log", std::vector<std::string>());
    std::vector<std::string> log_fr = body.value("log_fr", std::vector<std::string>());
    const std::vector<std::string>& shown = app.lang == Lang::French && log_fr.size() == log.size() ? log_fr : log;
    std::string news;
    for (size_t i = online.log.size(); i < shown.size(); i++) {
        news += shown[i] + " ";
    }
    online.log = log;
    online.log_fr = log_fr;
    // Chat : les nouveaux messages des autres s'affichent un moment en bas
    long last_id = online.chat.empty() ? 0 : online.chat.back().id;
    online.chat.clear();
    if (body.contains("chat") && body["chat"].is_array()) {
        for (const nlohmann::json& message : body["chat"]) {
            ChatLine line = {message.value("id", 0L), message.value("seat", 0), message.value("text", std::string()),
                             message.value("reaction", -1)};
            if (line.seat < 0 || line.seat >= static_cast<int>(app.game->players().size())) {
                continue;
            }
            online.chat.push_back(line);
            if (line.id > last_id && !first && line.seat != online.my_seat && !online.muted[line.seat]
                && !online.chat_open) {
                online.chat_toast = app.game->players()[line.seat].name + " : " + chat_text(app, line);
                online.chat_toast_until = GetTime() + 5;
            }
        }
    }
    if (online.chat_open && !online.chat.empty()) {
        online.chat_seen = online.chat.back().id;
    }
    if (first) {
        online.chat_seen = online.chat.empty() ? 0 : online.chat.back().id;
    }
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
        next = tr(app.lang, "Game over.");
        app.net.send(TagMe, "GET", "/me");  // nouvel Elo
    } else if (app.game->current_index() == online.my_seat) {
        next = tr(app.lang, app.game->step() == TurnStep::Discard ? "Discard your extra tokens." : "Your turn!");
    } else {
        next = tr(app.lang, "{0}'s turn.", {app.game->current_player().name});
    }
    set_message(app, news + next, false);
}

static std::string error_of(const NetResponse& response, const nlohmann::json& body, const App& app)
{
    if (response.status == 0) {
        return tr(app.lang, "Server unreachable ({0}).", {app.net.server()});
    }
    if (body.is_object() && body.contains("error") && body["error"].is_string()) {
        return tr(app.lang, body["error"].get<std::string>());  // message anglais du serveur
    }
    return tr(app.lang, "Server error ({0}).", {std::to_string(response.status)});
}

static void save_current_game(App& app);  // plus bas
static void start_replay(App& app, const std::string& data);  // plus bas

static void handle_responses(App& app)
{
    OnlineState& online = app.online;
    for (const NetResponse& response : app.net.poll()) {
        nlohmann::json body = nlohmann::json::parse(response.body, nullptr, false);
        bool ok = response.status == 200 && body.is_object();
        // Jeu trop ancien : le serveur refuse tout, on bloque sur l'écran de mise à jour
        if (response.status == 426) {
            app.min_version = body.is_object() ? body.value("min_version", std::string("?")) : std::string("?");
            app.download_url = body.is_object() ? body.value("download", std::string()) : std::string();
            if (app.screen == Screen::Board && app.game && !online.in_game && !app.game->is_over()) {
                save_current_game(app);  // partie locale gardée pour après la mise à jour
            }
            app.screen = Screen::Update;
            online.in_game = false;
            continue;
        }
        switch (response.tag) {
        case TagMeQuiet:
            // Au démarrage : session encore valide ? (sans changer d'écran)
            if (ok) {
                online.account = body.value("id", 0);
                online.pseudo = body.value("pseudo", std::string());
                online.elo = body.value("elo", 1000);
            } else if (response.status == 401) {
                save_session("");
                app.net.set_token("");
            }
            break;
        case TagConfig:
            if (ok) {
                online.announcement = body.value("announcement", std::string());
            }
            if (ok && body.value("discord_app_id", std::string()) != "") {
                app.discord.start(body.value("discord_app_id", std::string()));
#ifdef __EMSCRIPTEN__
                // Activité Discord : connexion automatique avec le compte Discord
                if (web_in_activity() && read_first_line(SESSION_FILE).empty()) {
                    web_activity_start(body.value("discord_app_id", std::string()));
                    app.screen = Screen::Login;
                    set_message(app, "Signing in with Discord…", false);
                }
#endif
            }
            break;
        case TagActivity:
            if (ok && !body.value("token", std::string()).empty()) {
                std::string token = body.value("token", std::string());
                save_session(token);
                app.net.set_token(token);
                app.net.send(TagMe, "GET", "/me");
            } else {
                set_message(app, error_of(response, body, app), true);
            }
            break;
        case TagNotes:
            online.notification_poll_running = false;
            if (!ok) {
                online.next_game_poll = std::max(online.next_game_poll, GetTime() + 5);
                break;
            }
            if (online.notification_last >= 0) {
                for (const nlohmann::json& note : body["notifications"]) {
                    // Défi lancé depuis Discord (/challenge) : le salon est prêt
                    if (note.value("type", std::string()) == "challenge_lobby" && !online.in_game) {
                        online.code_input = note.value("code", std::string());
                        app.net.send(TagJoin, "POST", "/lobbies/" + online.code_input + "/join");
                    }
                    if (note.value("type", std::string()) == "challenge") {
                        std::string text = tr(app.lang, "{0} challenges you! Lobby code: {1}",
                                              {note.value("from", std::string()), note.value("code", std::string())});
                        if (online.in_game) {
                            online.chat_toast = text;
                            online.chat_toast_until = GetTime() + 8;
                        } else {
                            online.code_input = note.value("code", std::string());
                            app.screen = Screen::Online;
                            set_message(app, text, false);
                        }
                    }
                    if (note.value("type", std::string()) == "friend_request") {
                        std::string text = tr(app.lang, "{0} wants to be your friend.",
                                              {note.value("pseudo", std::string())});
                        if (app.screen == Screen::Board) {
                            online.chat_toast = text;
                            online.chat_toast_until = GetTime() + 5;
                        } else {
                            set_message(app, text, false);
                        }
                        online.next_friends_poll = 0;
                    }
                    // Annonce des administrateurs (panel ou /dev announce)
                    if (note.value("type", std::string()) == "announcement") {
                        online.announcement = note.value("text", std::string());
                        if (app.screen == Screen::Board) {
                            online.chat_toast = online.announcement;
                            online.chat_toast_until = GetTime() + 10;
                        }
                    }
                    // Nouvelle saison (calendrier) : Elo de saison remis à 1000
                    if (note.value("type", std::string()) == "season") {
                        std::string text = tr(app.lang, "Season {0} has started: season Elo back to 1000!",
                                              {std::to_string(note.value("season", 0))});
                        if (app.screen == Screen::Board) {
                            online.chat_toast = text;
                            online.chat_toast_until = GetTime() + 10;
                        } else {
                            set_message(app, text, false);
                        }
                        app.net.send(TagMe, "GET", "/me");
                    }
                    if (note.value("type", std::string()) == "voice_offer") {
                        online.offer_channel = note.value("voice_channel", std::string());
                        online.offer_name = note.value("channel_name", std::string("Discord"));
                        if (!online.in_game) {
                            app.screen = Screen::Online;
                        }
                    }
                }
            }
            online.notification_last = body.value("last", 0L);
            break;
        case TagMe:
            if (ok) {
                online.look = look_from_json(body.value("look", nlohmann::json::object()));
                app.net.send(TagPass, "GET", "/me/pass");  // objets gagnés et portés
                online.account = body.value("id", 0);
                online.pseudo = body.value("pseudo", std::string());
                online.elo = body.value("elo", 1000);
                online.games = body.value("games", 0);
                online.wins = body.value("wins", 0);
                online.rank = body.value("rank", 0);
                online.season_elo = body.value("season_elo", 1000);
                online.season_games = body.value("season_games", 0);
                online.season_rank = body.value("season_rank", 0);
                online.season_min_games = body.value("season_min_games", 10);
                online.season_ends_at = body.value("season_ends_at", 0LL);
                online.achievements = body.value("achievements", std::vector<std::string>());
                online.providers = body.value("providers", std::vector<std::string>());
                online.announcement = body.value("announcement", online.announcement);
                online.season = body.value("season", 0);
                online.badges.clear();
                for (const nlohmann::json& badge : body.value("badges", nlohmann::json::array())) {
                    online.badges += (online.badges.empty() ? "" : " · ") + std::string("S")
                                     + std::to_string(badge.value("season", 0)) + " #"
                                     + std::to_string(badge.value("rank", 0));
                }
                if (online.pseudo.empty()) {
                    app.screen = Screen::Pseudo;
                    online.focus = 1;
                } else if (!online.pending_join.empty()) {
                    // Invitation acceptée dans Discord avant la connexion
                    online.code_input = online.pending_join;
                    online.pending_join.clear();
                    app.net.send(TagJoin, "POST", "/lobbies/" + online.code_input + "/join");
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
                set_message(app, "Session expired: please sign in again.", true);
            } else {
                set_message(app, error_of(response, body, app), true);
            }
            break;
        case TagAuthPoll:
            if (online.login_id.empty()) {
                break;
            }
            if (ok && body.value("status", std::string()) == "done" && body.contains("linked")) {
                // Connexion ajoutée au compte (Lier Google / Discord) : même session
                online.login_id.clear();
                std::string linked = body.value("linked", std::string()) == "google" ? "Google" : "Discord";
                set_message(app, tr(app.lang, "{0} account linked: you can now sign in with it too.", {linked}), false);
                app.net.send(TagMe, "GET", "/me");
            } else if (ok && body.value("status", std::string()) == "done") {
                std::string token = body.value("token", std::string());
                online.login_id.clear();
                save_session(token);
                app.net.set_token(token);
                app.net.send(TagMe, "GET", "/me");
            } else if (ok && body.value("status", std::string()) == "error") {
                online.login_id.clear();
                set_message(app, body.value("error", std::string("Sign-in failed.")), true);
            } else if (response.status == 429) {
                // Trop de requêtes depuis cette adresse (réseau partagé) : on
                // garde la connexion en cours et on réessaie un peu plus tard.
                online.next_login_poll = GetTime() + 10;
            } else if (!ok) {
                online.login_id.clear();
                set_message(app, error_of(response, body, app), true);
            }
            break;
        case TagBegin:
            // Secret enregistré : on peut ouvrir le navigateur pour l'OAuth
            if (ok && !body.value("login", std::string()).empty()) {
                std::string url = app.net.server() + "/auth/" + online.login_provider
                                  + "/start?login=" + online.login_id;
                set_message(app, "Finish signing in in your browser…", false);
                online.next_login_poll = GetTime() + 2;
#ifdef __EMSCRIPTEN__
                web_remember_verifier(online.login_verifier);  // survit à l'aller-retour
                web_go_to(url + "&from=play");
#else
                OpenURL(url.c_str());
#endif
            } else {
                online.login_id.clear();
                set_message(app, error_of(response, body, app), true);
            }
            break;
        case TagPass:
            if (ok) {
                online.pass_loaded = true;
                online.pass_season = body.value("season", 0);
                online.pass_xp = body.value("xp", 0);
                online.owned = body.value("owned", std::vector<std::string>());
                online.week_ends_at = body.value("week_ends_at", 0LL);
                online.challenges.clear();
                for (const nlohmann::json& entry : body.value("challenges", nlohmann::json::array())) {
                    online.challenges.push_back({entry.value("code", std::string()), entry.value("progress", 0),
                                                 entry.value("target", 0), entry.value("xp", 0),
                                                 entry.value("done", false)});
                }
                apply_equipped(app, body.value("equipped", nlohmann::json::object()));
            } else if (app.screen == Screen::Pass) {
                set_message(app, error_of(response, body, app), true);
            }
            break;
        case TagEquip:
            if (ok) {
                apply_equipped(app, body.value("equipped", nlohmann::json::object()));
            } else {
                set_message(app, error_of(response, body, app), true);
            }
            break;
        case TagLink:
            if (ok && !body.value("login", std::string()).empty()) {
                online.login_id = body.value("login", std::string());
                // L'ajout d'une connexion ne renvoie pas de jeton : pas de secret à présenter.
                online.login_verifier.clear();
                online.next_login_poll = GetTime() + 2;
                std::string provider = online.link_provider;
                std::string url = app.net.server() + "/auth/" + provider + "/start?login=" + online.login_id;
                set_message(app, "Finish signing in in your browser…", false);
#ifdef __EMSCRIPTEN__
                web_go_to(url + "&from=play");
#else
                OpenURL(url.c_str());
#endif
            } else {
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
        case TagActivityLobby:
        case TagJoin:
            if (ok) {
                online.lobby_code = response.tag == TagJoin ? online.code_input : body.value("code", std::string());
                online.lobby_version = 0;
                online.lobby_members.clear();
                app.screen = Screen::Lobby;
                app.message.clear();
                poll_lobby(app);
            } else {
                set_message(app, error_of(response, body, app), true);
            }
            break;
        case TagFriends:
            if (ok && body.contains("friends") && body["friends"].is_array()) {
                online.friends.clear();
                // Demandes reçues d'abord, puis amis, puis demandes envoyées
                for (const char* relation : {"incoming", "friend", "outgoing"}) {
                    for (const nlohmann::json& entry : body["friends"]) {
                        if (entry.value("relation", std::string()) == relation) {
                            online.friends.push_back({entry.value("pseudo", std::string()), entry.value("elo", 1000),
                                                      relation, entry.value("status", std::string()),
                                                      entry.value("game_id", 0)});
                        }
                    }
                }
            }
            break;
        case TagMatch:
            if (!ok) {
                set_message(app, error_of(response, body, app), true);
                if (app.screen == Screen::Matchmaking) {
                    app.screen = Screen::Online;
                }
            } else if (body.value("status", std::string()) == "matched") {
                enter_online_game(app, body.value("game_id", 0));
            } else if (app.screen == Screen::Matchmaking && body.value("status", std::string()) == "waiting") {
                online.match_season = body.value("kind", std::string()) == "season";
                online.match_waited = body.value("waited", 0);
                online.match_players = body.value("players_waiting", 0);
            }
            break;
        case TagHistory:
            if (ok && body.contains("games") && body["games"].is_array()) {
                online.past_games.clear();
                for (const nlohmann::json& entry : body["games"]) {
                    if (entry.value("cancelled", false)) {
                        continue;
                    }
                    online.past_games.push_back({entry.value("id", 0), entry.value("mode", std::string()),
                                                 entry.value("finished_at", 0LL), entry.value("place", 0),
                                                 entry.value("score", 0), entry.value("elo_change", 0),
                                                 entry.value("kind", std::string("ranked")), entry.value("replay", false),
                                                 entry.value("players", std::vector<std::string>())});
                }
            } else if (!ok) {
                set_message(app, error_of(response, body, app), true);
            }
            break;
        case TagReplay:
            if (ok) {
                start_replay(app, response.body);
            } else {
                set_message(app, error_of(response, body, app), true);
            }
            break;
        case TagLive:
            if (ok && body.contains("games") && body["games"].is_array()) {
                online.live_games.clear();
                for (const nlohmann::json& entry : body["games"]) {
                    LiveGame live = {entry.value("id", 0), entry.value("mode", std::string()), entry.value("round", 1), {}};
                    for (const nlohmann::json& player : entry.value("players", nlohmann::json::array())) {
                        live.players.push_back(player.value("pseudo", std::string()) + " ("
                                               + std::to_string(player.value("elo", 1000)) + ") — "
                                               + std::to_string(player.value("score", 0)) + " pts");
                    }
                    online.live_games.push_back(live);
                }
            }
            break;
        case TagFriendChange:
            if (ok) {
                online.next_friends_poll = 0;
                app.message.clear();
            } else {
                set_message(app, error_of(response, body, app), true);
            }
            break;
        case TagChat:
            if (!ok) {
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
                    set_message(app, "The lobby no longer exists.", true);
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
            online.lobby_looks.clear();
            for (const nlohmann::json& member : body["members"]) {
                online.lobby_members.push_back(member.value("pseudo", std::string()));
                online.lobby_elos.push_back(member.value("elo", 1000));
                online.lobby_looks.push_back(look_from_json(member.value("look", nlohmann::json::object())));
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
                set_message(app, "This game no longer exists.", true);
            } else {
                online.next_game_poll = GetTime() + 2;
                set_message(app, error_of(response, body, app) + " " + tr(app.lang, "Retrying…"), true);
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
                online.season = body.value("season", online.season);
                online.season_ends_at = body.value("season_ends_at", online.season_ends_at);
                online.season_min_games = body.value("season_min_games", online.season_min_games);
                online.board_global.clear();
                online.board_season.clear();
                for (const char* key : {"players", "season_players"}) {
                    std::vector<BoardLine>& lines = std::string(key) == "players" ? online.board_global
                                                                                  : online.board_season;
                    for (const nlohmann::json& player : body.value(key, nlohmann::json::array())) {
                        lines.push_back({player.value("pseudo", std::string()), player.value("elo", 1000),
                                         player.value("games", 0), player.value("wins", 0)});
                    }
                }
                online.board_loaded = true;
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
    case ButtonId::LinkAccount:
        online.link_provider = button.value == 1 ? "discord" : "google";
        app.net.send(TagLink, "POST", "/me/link", nlohmann::json({{"provider", online.link_provider}}).dump());
        break;
    case ButtonId::BackHome:
        online.login_id.clear();
        app.message.clear();
        app.screen = Screen::Home;
        break;
    case ButtonId::PseudoBox:
        online.focus = 1;
#ifdef __EMSCRIPTEN__
        web_prompt(tr(app.lang, "Username (3 to 16 characters):"), online.pseudo_input);
        online.focus = 0;
#endif
        break;
    case ButtonId::PseudoOk:
        app.net.send(TagPseudo, "POST", "/me/pseudo", nlohmann::json({{"pseudo", online.pseudo_input}}).dump());
        break;
    case ButtonId::CodeBox:
        online.focus = 2;
#ifdef __EMSCRIPTEN__
        if (web_prompt(tr(app.lang, "Lobby code (5 characters):"), online.code_input)) {
            std::string code;
            for (char c : online.code_input) {
                if (std::isalnum(static_cast<unsigned char>(c)) && code.size() < 5) {
                    code += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                }
            }
            online.code_input = code;
        }
        online.focus = 0;
#endif
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
    case ButtonId::ShowLive:
        app.screen = Screen::Live;
        app.message.clear();
        online.live_games.clear();
        online.next_live_poll = 0;
        break;
    case ButtonId::WatchGame:
        if (button.value > 0) {
            enter_online_game(app, button.value);
        }
        break;
    case ButtonId::FindMatch:
        // value 0 : file Classé (Elo global) ; 1 : file Saison (Elo de saison)
        app.screen = Screen::Matchmaking;
        app.message.clear();
        online.match_season = button.value == 1;
        online.match_waited = 0;
        online.match_players = 0;
        online.next_match_poll = GetTime() + 2;
        app.net.send(TagMatch, "POST", "/matchmaking/join",
                     nlohmann::json({{"mode", online.create_mode == Mode::Classic ? "classique" : "rapide"},
                                     {"kind", online.match_season ? "season" : "ranked"}})
                         .dump());
        break;
    case ButtonId::CancelMatch:
        app.net.send(TagMatchLeave, "POST", "/matchmaking/leave");  // réponse ignorée
        app.screen = Screen::Online;
        break;
    case ButtonId::ShowHistory:
        app.screen = Screen::History;
        app.message.clear();
        online.past_games.clear();
        app.net.send(TagHistory, "GET", "/me/games");
        break;
    case ButtonId::ReplayGame:
        app.net.send(TagReplay, "GET", "/replays/" + std::to_string(button.value));
        break;
    case ButtonId::ShowAchievements:
        app.screen = Screen::Achievements;
        app.message.clear();
        break;
    case ButtonId::ShowFriends:
        app.screen = Screen::Friends;
        app.message.clear();
        online.next_friends_poll = 0;  // liste demandée tout de suite
        break;
    case ButtonId::FriendBox:
        online.focus = 3;
#ifdef __EMSCRIPTEN__
        web_prompt(tr(app.lang, "Username"), online.friend_input);
        online.friend_input = online.friend_input.substr(0, 64);
        online.focus = 0;
#endif
        break;
    case ButtonId::FriendAdd:
        app.net.send(TagFriendChange, "POST", "/friends/add", nlohmann::json({{"pseudo", online.friend_input}}).dump());
        online.friend_input.clear();
        break;
    case ButtonId::FriendAccept:
    case ButtonId::FriendRemove:
        if (button.value >= 0 && button.value < static_cast<int>(online.friends.size())) {
            app.net.send(TagFriendChange, "POST", button.id == ButtonId::FriendAccept ? "/friends/add" : "/friends/remove",
                         nlohmann::json({{"pseudo", online.friends[button.value].pseudo}}).dump());
        }
        break;
    case ButtonId::FriendChallenge:
        // Défi amical : salon de 2 (mode choisi sur l'écran En ligne) ; on y entre
        // comme pour « Créer », l'ami reçoit une notification avec le code
        if (button.value >= 0 && button.value < static_cast<int>(online.friends.size())) {
            app.net.send(TagCreate, "POST", "/friends/challenge",
                         nlohmann::json({{"pseudo", online.friends[button.value].pseudo},
                                         {"mode", online.create_mode == Mode::Classic ? "classique" : "rapide"}})
                             .dump());
        }
        break;
    case ButtonId::ShowLeaderboard:
        online.board_loaded = false;
        app.screen = Screen::Leaderboard;
        app.net.send(TagLeaderboard, "GET", "/leaderboard");
        break;
    case ButtonId::LeaderboardTab:
        online.board_show_season = button.value == 1;
        break;
    case ButtonId::ShowPass:
        app.screen = Screen::Pass;
        app.message.clear();
        app.net.send(TagPass, "GET", "/me/pass");
        break;
    case ButtonId::ShowAppearance:
        app.screen = Screen::Appearance;
        app.message.clear();
        break;
    case ButtonId::AppearanceTab:
        online.appearance_tab = std::max(0, std::min(button.value, APPEARANCE_TABS - 1));
        break;
    case ButtonId::EquipItem: {
        // value -1 : apparence normale ; sinon le n-ième objet de la sorte affichée
        CosmeticKind kind = static_cast<CosmeticKind>(online.appearance_tab);
        std::vector<std::string> items = owned_items(online, kind);
        std::string code = button.value >= 0 && button.value < static_cast<int>(items.size()) ? items[button.value] : "";
        app.net.send(TagEquip, "POST", "/me/equip", nlohmann::json({{"kind", kind_code(kind)}, {"code", code}}).dump());
        break;
    }
    case ButtonId::Logout:
        app.net.send(TagLogout, "POST", "/auth/logout");
        save_session("");
        app.net.set_token("");
        online = OnlineState();
        set_local_look(app, "", "", "");  // les objets du passe sont ceux du compte
        configure_network(app);
        app.screen = Screen::Home;
        set_message(app, "You are signed out.", false);
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
        set_message(app, "Code copied.", false);
        break;
    case ButtonId::Invite: {
        std::string invitation = tr(app.lang, "Join my Gemspire game! Code: {0} (Play online > Join). Download the game: {1}",
                                    {online.lobby_code, DOWNLOAD_URL});
        SetClipboardText(invitation.c_str());
        set_message(app, "Invite copied: paste it in Discord.", false);
        break;
    }
    case ButtonId::OfferCreate: {
        nlohmann::json body = {{"mode", online.create_mode == Mode::Classic ? "classique" : "rapide"},
                               {"seats", online.create_seats},
                               {"voice_channel", online.offer_channel}};
        online.offer_channel.clear();
        app.net.send(TagCreate, "POST", "/lobbies", body.dump());
        break;
    }
    case ButtonId::OfferIgnore:
        online.offer_channel.clear();
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
#ifdef __EMSCRIPTEN__
    web_save_files();
#endif
    if (error.empty()) {
        set_message(app, tr(app.lang, "Game saved ({0}).", {SAVE_FILE}), false);
    } else {
        set_message(app, error, true);
    }
}

// Envoie un message (texte ou réaction rapide) au chat de la partie
static void send_chat(App& app, const std::string& text, int reaction)
{
    nlohmann::json body = reaction >= 0 ? nlohmann::json({{"reaction", reaction}}) : nlohmann::json({{"text", text}});
    app.net.send(TagChat, "POST", "/games/" + std::to_string(app.online.game_id) + "/chat", body.dump());
}

static void press_chat_button(App& app, const Button& button)
{
    OnlineState& online = app.online;
    switch (button.id) {
    case ButtonId::Chat:
        online.chat_open = !online.chat_open;
        app.journal_open = false;
        if (!online.chat.empty()) {
            online.chat_seen = online.chat.back().id;
        }
        break;
    case ButtonId::ChatBox:
#ifdef __EMSCRIPTEN__
        // Navigateur : boîte de saisie (clavier du téléphone), envoi direct
        if (web_prompt(tr(app.lang, "Message:"), online.chat_input) && !online.chat_input.empty()) {
            send_chat(app, online.chat_input.substr(0, 4 * CHAT_MAX_CHARS), -1);
            online.chat_input.clear();
        }
#endif
        break;
    case ButtonId::ChatSend:
        if (!online.chat_input.empty()) {
            send_chat(app, online.chat_input, -1);
            online.chat_input.clear();
        }
        break;
    case ButtonId::ChatReaction:
        send_chat(app, "", button.value);
        break;
    case ButtonId::ChatPassReaction: {
        std::vector<std::string> reactions = owned_items(online, CosmeticKind::Reaction);
        if (button.value >= 0 && button.value < static_cast<int>(reactions.size())) {
            app.net.send(TagChat, "POST", "/games/" + std::to_string(online.game_id) + "/chat",
                         nlohmann::json({{"reaction_code", reactions[button.value]}}).dump());
        }
        break;
    }
    case ButtonId::ChatMute:
        if (button.value >= 0 && button.value < 4) {
            online.muted[button.value] = !online.muted[button.value];
        }
        break;
    default:
        break;
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
        app.online.chat_open = false;
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
    case ButtonId::Forfeit:
        // Deux clics : « Abandonner » puis « Confirmer ? » dans les 4 secondes
        if (GetTime() < app.online.forfeit_confirm) {
            app.online.forfeit_confirm = 0;
            app.net.send(TagMove, "POST", "/games/" + std::to_string(app.online.game_id) + "/forfeit");
        } else {
            app.online.forfeit_confirm = GetTime() + 4;
            set_message(app, "Forfeiting counts as a loss (Elo lost). Click again to confirm.", true);
        }
        break;
    default:
        break;
    }
}

// Clic sur un jeton de la réserve : 0 -> 1 -> 2 -> 0 jeton de cette couleur, en
// sautant les choix impossibles (2 identiques si la pile a moins de 4 jetons ou
// si d'autres couleurs sont choisies, plus de 3 couleurs)
static void toggle_token(App& app, int gem)
{
    const Game& game = *app.game;
    if (gem == GOLD_INDEX) {
        set_message(app, "Gold can't be taken: you get it by reserving a card.", true);
        return;
    }
    if (game.bank()[gem] == 0) {
        set_message(app, tr(app.lang, "No {0} tokens left.", {color_name(color_at(gem), app.lang)}), true);
        return;
    }
    app.target = Target::None;
    app.message.clear();
    Resources& selection = app.selection;
    int other_colors = 0;    // autres couleurs déjà choisies
    bool other_double = false;
    for (int c = 0; c < COLOR_COUNT; c++) {
        if (c != gem && selection[c] > 0) {
            other_colors++;
            other_double = other_double || selection[c] == 2;
        }
    }
    if (selection[gem] == 0) {
        // Nouvelle couleur : jamais avec 2 jetons identiques, et 3 couleurs au plus
        if (other_double) {
            clear_selection(app);
        } else if (other_colors >= 3) {
            set_message(app, "At most 3 tokens of different colours.", true);
            return;
        }
        selection[gem] = 1;
    } else if (selection[gem] == 1 && other_colors == 0 && game.bank()[gem] >= MIN_STACK_FOR_DOUBLE) {
        selection[gem] = 2;  // 2 jetons identiques : seule couleur choisie, pile d'au moins 4
    } else {
        selection[gem] = 0;
    }
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
                set_message(app, "Decks are only for reserving (classic game).", true);
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

// Rediffusion : refait la partie jusqu'à l'action `step`
static void show_replay_step(App& app, int step)
{
    ReplayState& replay = app.replay;
    std::unique_ptr<Game> game;
    int count = 0;
    std::string error = replay_game(nlohmann::json::parse(replay.data, nullptr, false), step, game, count);
    if (!error.empty() || !game) {
        replay.active = false;
        app.screen = Screen::History;
        set_message(app, error.empty() ? "Damaged replay." : error, true);
        return;
    }
    replay.count = count;
    replay.step = step < 0 || step > count ? count : step;
    app.game = std::move(game);
    app.curtain = false;
    app.shown_events = static_cast<int>(app.game->events().size());
    std::string last = app.game->events().empty()
                           ? tr(app.lang, "{0}'s turn.", {app.game->current_player().name})
                           : describe_event(*app.game, app.game->events().back(), app.lang);
    set_message(app, tr(app.lang, "Action {0}/{1}", {std::to_string(replay.step), std::to_string(replay.count)}) + " — "
                         + last, false);
}

static void start_replay(App& app, const std::string& data)
{
    app.replay = ReplayState();
    app.replay.active = true;
    app.replay.data = data;
    app.online.in_game = false;
    show_replay_step(app, 0);
    if (!app.replay.active) {
        return;
    }
    start_board(app);
    app.player_colors.clear();
    for (int p = 0; p < static_cast<int>(app.game->players().size()); p++) {
        app.player_colors.push_back(p);
    }
    show_replay_step(app, 0);
}

// Boutons et touches de la rediffusion (flèches, Espace, Échap)
static void update_replay(App& app, Vector2 mouse)
{
    ReplayState& replay = app.replay;
    ButtonId pressed = ButtonId::Back;  // aucun
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        for (const Button& button : visible_buttons(app)) {
            if (point_in(mouse, button.rect) && button.enabled) {
                pressed = button.id;
            }
        }
    }
    if (IsKeyPressed(KEY_LEFT)) {
        pressed = ButtonId::ReplayPrevious;
    } else if (IsKeyPressed(KEY_RIGHT)) {
        pressed = ButtonId::ReplayNext;
    } else if (IsKeyPressed(KEY_SPACE)) {
        pressed = ButtonId::ReplayPlay;
    } else if (IsKeyPressed(KEY_ESCAPE)) {
        pressed = ButtonId::ReplayExit;
    }
    if (pressed == ButtonId::ReplayExit) {
        replay.active = false;
        app.game.reset();
        app.screen = Screen::History;
        app.message.clear();
        return;
    }
    if (pressed == ButtonId::ReplayPrevious && replay.step > 0) {
        replay.playing = false;
        show_replay_step(app, replay.step - 1);
    } else if (pressed == ButtonId::ReplayNext && replay.step < replay.count) {
        replay.playing = false;
        show_replay_step(app, replay.step + 1);
    } else if (pressed == ButtonId::ReplayPlay && replay.step < replay.count) {
        replay.playing = !replay.playing;
        replay.next_step = GetTime();
    }
    // Lecture automatique : une action toutes les 0,8 s
    if (replay.playing && GetTime() >= replay.next_step) {
        replay.next_step = GetTime() + 0.8;
        show_replay_step(app, replay.step + 1);
        if (replay.step >= replay.count) {
            replay.playing = false;
        }
    }
}

static void update_board(App& app, Vector2 mouse)
{
    if (app.replay.active) {
        update_replay(app, mouse);
        return;
    }
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
    // Chat ouvert : le clavier écrit le message (Entrée envoie, Échap ferme)
    if (app.online.chat_open) {
        if (game.is_over()) {
            app.online.chat_open = false;
        }
        edit_text(app.online.chat_input, CHAT_MAX_CHARS, false);
        if ((IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) && !app.online.chat_input.empty()) {
            send_chat(app, app.online.chat_input, -1);
            app.online.chat_input.clear();
        }
        if (IsKeyPressed(KEY_ESCAPE)) {
            app.online.chat_open = false;
            return;
        }
        if (clicked) {
            for (const Button& button : visible_buttons(app)) {
                if (point_in(mouse, button.rect) && button.enabled) {
                    if (button.id >= ButtonId::Chat && button.id <= ButtonId::ChatMute) {
                        press_chat_button(app, button);
                    } else {
                        press_board_button(app, button.id);
                    }
                    return;
                }
            }
        }
        return;
    }
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
                if (button.enabled && button.id == ButtonId::Chat) {
                    press_chat_button(app, button);
                } else if (button.enabled && button.id == ButtonId::ReplayGame) {
                    app.net.send(TagReplay, "GET", "/replays/" + std::to_string(button.value));
                } else if (button.enabled) {
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

// Ce que montre la Rich Presence Discord selon l'écran
static void update_presence(App& app)
{
    const OnlineState& online = app.online;
    PresenceInfo info;
    info.start = app.screen_start;
    if (app.screen == Screen::Lobby) {
        info.details = tr(app.lang, "In a lobby ({0})", {tr(app.lang, online.lobby_mode == "classique" ? "classic" : "quick")});
        info.state = tr(app.lang, "Waiting for players");
        info.party_id = online.lobby_code;
        info.party_size = static_cast<int>(online.lobby_members.size());
        info.party_max = online.lobby_seats;
        if (info.party_size < info.party_max) {
            info.join_secret = online.lobby_code;  // les amis peuvent rejoindre depuis Discord
        }
    } else if (app.screen == Screen::Board && app.game) {
        const Game& game = *app.game;
        std::string mode = tr(app.lang, game.mode() == Mode::Classic ? "classic" : "quick");
        info.details = tr(app.lang, online.in_game ? "Online game ({0})" : "Local game ({0})", {mode});
        int seat = viewer_seat(app);
        info.state = game.is_over() ? tr(app.lang, "Game over")
                                    : tr(app.lang, "Round {0} · {1} pts",
                                         {std::to_string(game.round()), std::to_string(game.players()[seat].score())});
    } else if (app.screen == Screen::Online || app.screen == Screen::Leaderboard) {
        info.details = tr(app.lang, app.screen == Screen::Leaderboard ? "Viewing the leaderboard" : "Looking for an online game");
        info.state = online.pseudo.empty() ? "" : online.pseudo + " · Elo " + std::to_string(online.elo);
    } else {
        info.details = tr(app.lang, "In the menus");
    }
    app.discord.update(info);
}

static void update(App& app, Vector2 mouse)
{
    OnlineState& online = app.online;
    handle_responses(app);
    // Nouvel écran : le chronomètre de la Rich Presence repart
    if (app.screen != app.presence_screen) {
        app.presence_screen = app.screen;
        app.screen_start = static_cast<long long>(std::time(nullptr));
    }
    update_presence(app);
#ifndef __EMSCRIPTEN__
    // F11 : plein écran (fenêtre sans bord à la taille de l'écran) ; dans le
    // navigateur, F11 est géré par le navigateur lui-même
    if (IsKeyPressed(KEY_F11)) {
        ToggleBorderlessWindowed();
    }
#endif
    // Annonces : la première s'affiche BANNER_SECONDS, puis la suivante
    if (!app.banners.empty()) {
        if (app.screen != Screen::Board) {
            app.banners.clear();  // plus de plateau : annonces sans objet
        } else if (app.banners.front().start < 0) {
            if (!app.curtain && !app.journal_open) {  // attend que le plateau soit visible
                app.banners.front().start = GetTime();
            }
        } else if (GetTime() - app.banners.front().start > BANNER_SECONDS) {
            app.banners.erase(app.banners.begin());
        }
    }
    // « Rejoindre » cliqué dans Discord : le secret est le code du salon
    std::string join_code;
    if (app.discord.take_join(join_code) && join_code.size() == 5) {
        online.pending_join = join_code;
        online.in_game = false;
        go_online(app);
    }
    // Connecté : on attend les notifications (sert aussi de présence pour le bot)
    if (online.account != 0 && !online.notification_poll_running && GetTime() >= online.next_game_poll) {
        online.notification_poll_running = true;
        app.net.send(TagNotes, "GET", "/me/notifications?since=" + std::to_string(online.notification_last), "", true);
    }
#ifdef __EMSCRIPTEN__
    // Activité Discord : Discord a donné un code, le serveur l'échange contre une session
    std::string activity = web_activity_take();
    if (activity.compare(0, 5, "code:") == 0) {
        app.net.send(TagActivity, "POST", "/auth/discord/activity",
                     nlohmann::json({{"code", activity.substr(5)}}).dump());
    } else if (activity.compare(0, 6, "error:") == 0) {
        set_message(app, activity.substr(6), true);
    }
    // Activité Discord : dès qu'on est connecté avec un pseudo, on rejoint le
    // salon de la partie de ce salon vocal (le même pour tous les participants)
    if (web_in_activity() && !online.activity_lobby_asked && online.account != 0 && !online.pseudo.empty()
        && !online.in_game && app.screen != Screen::Board) {
        online.activity_lobby_asked = true;
        app.net.send(TagActivityLobby, "POST", "/lobbies/activity",
                     nlohmann::json({{"instance", web_activity_instance()}}).dump());
    }
#endif
    // Connexion en cours : on demande au serveur si le navigateur a fini
    if (!online.login_id.empty() && GetTime() >= online.next_login_poll) {
        online.next_login_poll = GetTime() + 1.5;
        app.net.send(TagAuthPoll, "GET", "/auth/poll?login=" + online.login_id + "&verifier=" + online.login_verifier);
    }
    if (app.screen == Screen::Lobby) {
        poll_lobby(app);
    }
    if (online.in_game) {
        poll_game(app);
    }
    if (app.screen == Screen::Update) {
        // Bloqué : seulement « Télécharger » et « Quitter »
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            for (const Button& button : visible_buttons(app)) {
                if (point_in(mouse, button.rect) && button.id == ButtonId::Download) {
                    OpenURL(app.download_url.empty() ? "https://github.com/Pierre-Pln-Bys/Gemspire/releases/latest"
                                                     : app.download_url.c_str());
                } else if (point_in(mouse, button.rect) && button.id == ButtonId::Quit) {
                    app.quit = true;
                }
            }
        }
        return;
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
    if (app.screen == Screen::Matchmaking && GetTime() >= online.next_match_poll) {
        online.next_match_poll = GetTime() + 2;
        app.net.send(TagMatch, "GET", "/matchmaking");
    }
    if (app.screen == Screen::Live && GetTime() >= online.next_live_poll && app.net.pending() == 0) {
        online.next_live_poll = GetTime() + 5;
        app.net.send(TagLive, "GET", "/games/live");
    }
    if (app.screen == Screen::Friends) {
        if (GetTime() >= online.next_friends_poll && app.net.pending() == 0) {
            online.next_friends_poll = GetTime() + 8;  // statuts rafraîchis toutes les 8 secondes
            app.net.send(TagFriends, "GET", "/friends");
        }
        if (online.focus == 3) {
            edit_text(online.friend_input, 16, false);
            if ((IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) && !online.friend_input.empty()) {
                press_online_button(app, {ButtonId::FriendAdd, "", true, {}, 0, false});
            }
        }
    }
    if (online.focus == 2 && app.screen == Screen::Online) {
        edit_text(online.code_input, 5, true);
        if ((IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) && online.code_input.size() == 5) {
            press_online_button(app, {ButtonId::JoinLobby, "", true, {}, 0, false});
        }
    }
    if (app.screen == Screen::Rules && IsKeyPressed(KEY_ESCAPE)) {
        app.screen = Screen::Home;
        return;
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

#ifdef __EMSCRIPTEN__
// Navigateur : le dessin prend la taille réelle du canevas à l'écran (pixels
// de l'écran, écrans Retina compris) au lieu d'agrandir une image 1280 x 800
static void fit_canvas()
{
    double css_width = 0;
    double css_height = 0;
    emscripten_get_element_css_size("#canvas", &css_width, &css_height);
    double ratio = std::fmin(emscripten_get_device_pixel_ratio(), 2.5);  // limite pour les téléphones
    int width = static_cast<int>(css_width * ratio + 0.5);
    int height = static_cast<int>(css_height * ratio + 0.5);
    if (width >= 320 && height >= 200 && (width != GetScreenWidth() || height != GetScreenHeight())) {
        SetWindowSize(width, height);
    }
}
#endif

// Une image : entrées, mise à jour, dessin
static void draw_frame(void* data)
{
    App& app = *static_cast<App*>(data);
#ifdef __EMSCRIPTEN__
    fit_canvas();
#endif
    Camera2D camera = board_camera();
    Vector2 mouse = GetScreenToWorld2D(GetMousePosition(), camera);
    update(app, mouse);

    BeginDrawing();
    ClearBackground(current_theme(app).background);
    BeginMode2D(camera);
    draw_app(app, mouse);
    EndMode2D();
    EndDrawing();
}

int main()
{
#ifdef __EMSCRIPTEN__
    // Navigateur : dessin de taille fixe, agrandi par la page (shell.html)
    SetConfigFlags(FLAG_MSAA_4X_HINT);
#else
#ifdef _WIN32
    // Windows : écran agrandi à 125 % ou 150 % : on dessine en vrais pixels (net)
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI);
#else
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
#endif
#endif
    std::string title = std::string("Gemspire ") + SPLENDOR_VERSION;
    InitWindow(1280, 800, title.c_str());
#ifndef __EMSCRIPTEN__
    SetWindowMinSize(640, 400);
#endif
    SetExitKey(KEY_NULL);  // Échap sert à annuler, pas à quitter
    SetTargetFPS(60);

    App app;
    // Langue : celle choisie avant (langue.txt), sinon celle du système / du navigateur
    std::string saved_lang = read_first_line(LANGUAGE_FILE);
#ifdef __EMSCRIPTEN__
    app.lang = saved_lang.empty() ? lang_from_code(web_language()) : lang_from_code(saved_lang);
#else
    app.lang = saved_lang.empty() ? system_lang() : lang_from_code(saved_lang);
#endif
    for (int p = 0; p < 4; p++) {
        app.setup.names[p] = tr(app.lang, "Player {0}", {std::to_string(p + 1)});
    }
    std::string saved_theme = read_first_line(THEME_FILE);
    if (saved_theme.size() == 1 && saved_theme[0] >= '0' && saved_theme[0] < '0' + THEME_COUNT) {
        app.theme = saved_theme[0] - '0';
    }
    // Objets du passe portés la dernière fois (thème, dos de cartes, jetons)
    {
        std::ifstream file(LOOK_FILE);
        std::string theme, cardback, tokens;
        std::getline(file, theme);
        std::getline(file, cardback);
        std::getline(file, tokens);
        Cosmetic cosmetic;
        app.look_theme = find_cosmetic(theme, cosmetic) && cosmetic.kind == CosmeticKind::Theme ? theme : "";
        if (!app.look_theme.empty()) {
            app.pass_theme = theme_from_cosmetic(cosmetic);
        }
        app.look_cardback = find_cosmetic(cardback, cosmetic) && cosmetic.kind == CosmeticKind::CardBack ? cardback : "";
        app.look_tokens = find_cosmetic(tokens, cosmetic) && cosmetic.kind == CosmeticKind::Tokens ? tokens : "";
    }
    configure_network(app);
    app.screen_start = static_cast<long long>(std::time(nullptr));
    app.net.send(TagConfig, "GET", "/config", "", true);
    if (!read_first_line(SESSION_FILE).empty()) {
        app.net.send(TagMeQuiet, "GET", "/me", "", true);
    }
#ifdef __EMSCRIPTEN__
    // Retour de Discord / Google : on termine la connexion commencée avant
    std::string returning_login = web_login_from_address();
    if (!returning_login.empty()) {
        app.online.login_id = returning_login;
        app.online.login_verifier = web_take_verifier();  // secret gardé pendant l'aller-retour
        app.online.next_login_poll = 0;
        app.screen = Screen::Login;
    }
#endif
    app.font = load_font_regular();
    app.bold = load_font_bold();
    prepare_font(app.font);
    prepare_font(app.bold);

#ifdef __EMSCRIPTEN__
    // Le navigateur appelle draw_frame à chaque image (pas de boucle bloquante)
    emscripten_set_main_loop_arg(draw_frame, &app, 0, true);
#else
    while (!WindowShouldClose() && !app.quit) {
        draw_frame(&app);
    }
#endif

    // Les glyphes des polices sont des tableaux statiques : on ne libère que la texture
    UnloadTexture(app.font.texture);
    UnloadTexture(app.bold.texture);
    CloseWindow();
    return 0;
}
