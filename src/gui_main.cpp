// Interface graphique de Splendor (raylib).
// Ce fichier gère la fenêtre, les écrans, la souris, le clavier et
// l'enchaînement des coups. Le dessin est dans gui_draw.cpp.
#include <array>
#include <cmath>
#include <cstdio>
#include <functional>
#include <random>
#include <string>
#include <vector>

#include "raylib.h"

#include "ai.hpp"
#include "display.hpp"
#include "gui.hpp"
#include "gui_font.h"
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
        play_move(app, [&]() { return game.take_tokens(app.selection); });
        break;
    case ButtonId::Buy:
        if (app.target == Target::Visible) {
            play_move(app, [&]() { return game.buy_card(app.target_level, app.target_index + 1); });
        } else if (app.target == Target::Reserved) {
            play_move(app, [&]() { return game.buy_reserved(app.target_index + 1); });
        }
        break;
    case ButtonId::Reserve: {
        int position = app.target == Target::Deck ? 0 : app.target_index + 1;
        play_move(app, [&]() { return game.reserve_card(app.target_level, position); });
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
    Game& game = *app.game;
    play_move(app, [&]() { return gem == GOLD_INDEX ? game.discard_gold() : game.discard(color_at(gem)); });
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
                play_move(app, [&]() { return game.choose_noble(choice); });
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
    if (ctrl && IsKeyPressed(KEY_S) && !game.is_over()) {
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
    if (app.screen == Screen::Board && app.game) {
        update_board(app, mouse);
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
                    press_menu_button(app, button);
                }
                break;
            }
        }
        // Clic ailleurs : on quitte la saisie du nom
        if (!on_button) {
            app.setup.focused_name = -1;
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
