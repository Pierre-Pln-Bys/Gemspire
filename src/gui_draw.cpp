#include "gui.hpp"

#include <algorithm>
#include <cmath>

#include "display.hpp"
#include "rules.hpp"
#include "version.hpp"

// ---------------------------------------------------------------- Placement

// Ordonnée de la rangée d'un niveau (niveau 3 en haut, comme sur la table)
static float row_y(int level)
{
    return 150.0f + (LEVEL_COUNT - level) * 150.0f;
}

Rectangle noble_rect(int index) { return {150.0f + index * 105, 40, 90, 100}; }
Rectangle deck_rect(int level) { return {30, row_y(level), 100, 140}; }
Rectangle card_rect(int level, int index) { return {150.0f + index * 115, row_y(level), 100, 140}; }
Rectangle reserved_rect(int index) { return {150.0f + index * 115, 615, 100, 140}; }
Rectangle player_rect(int player) { return {790, 46.0f + player * 187, 470, 178}; }

Vector2 bank_token_center(int gem)
{
    return {668, 192.0f + gem * 70};
}

Vector2 player_token_center(int player, int gem)
{
    Rectangle panel = player_rect(player);
    return {panel.x + 110 + gem * 52, panel.y + 120};
}

bool point_in(Vector2 point, Rectangle rect)
{
    return point.x >= rect.x && point.x <= rect.x + rect.width
           && point.y >= rect.y && point.y <= rect.y + rect.height;
}

// ---------------------------------------------------------------- Boutons

static bool human_turn(const App& app)
{
    return app.game && !app.game->is_over() && app.game->current_player().controller == Controller::Human;
}

static void add(std::vector<Button>& buttons, ButtonId id, const std::string& label, Rectangle rect,
                int value = 0, bool enabled = true, bool selected = false)
{
    buttons.push_back({id, label, enabled, rect, value, selected});
}

static std::vector<Button> home_buttons()
{
    std::vector<Button> buttons;
    add(buttons, ButtonId::NewGame, "Nouvelle partie", {490, 400, 300, 54});
    add(buttons, ButtonId::Resume, "Reprendre la sauvegarde", {490, 468, 300, 54}, 0, FileExists(SAVE_FILE));
    add(buttons, ButtonId::Quit, "Quitter", {490, 536, 300, 54});
    return buttons;
}

static std::vector<Button> setup_buttons(const Setup& setup)
{
    std::vector<Button> buttons;
    add(buttons, ButtonId::Mode, "Rapide", {360, 120, 200, 40}, 0, true, setup.mode == Mode::Quick);
    add(buttons, ButtonId::Mode, "Classique", {580, 120, 200, 40}, 1, true, setup.mode == Mode::Classic);
    for (int count = 2; count <= 4; count++) {
        add(buttons, ButtonId::Count, std::to_string(count), {360.0f + (count - 2) * 70, 190, 56, 40}, count, true,
            setup.count == count);
    }
    const char* kinds[] = {"Humain", "Ordi facile", "Ordi moyen"};
    const Controller kind_values[] = {Controller::Human, Controller::AiEasy, Controller::AiMedium};
    for (int p = 0; p < setup.count; p++) {
        float y = 280.0f + p * 90;
        add(buttons, ButtonId::NameBox, setup.names[p], {200, y, 300, 40}, p, true, setup.focused_name == p);
        for (int c = 0; c < PLAYER_COLOR_COUNT; c++) {
            bool taken = false;
            for (int other = 0; other < setup.count; other++) {
                taken = taken || (other != p && setup.colors[other] == c);
            }
            add(buttons, ButtonId::Swatch, "", {530.0f + c * 46, y + 2, 36, 36}, p * 10 + c, !taken,
                setup.colors[p] == c);
        }
        for (int k = 0; k < 3; k++) {
            add(buttons, ButtonId::Kind, kinds[k], {790.0f + k * 128, y, 120, 40}, p * 10 + k, true,
                setup.kinds[p] == kind_values[k]);
        }
    }
    add(buttons, ButtonId::Back, "Retour", {200, 690, 200, 50});
    add(buttons, ButtonId::Start, "Jouer", {880, 690, 200, 50});
    return buttons;
}

static std::vector<Button> board_buttons(const App& app)
{
    std::vector<Button> buttons;
    const Game& game = *app.game;

    if (game.is_over()) {
        add(buttons, ButtonId::Replay, "Rejouer", {490, 540, 140, 44});
        add(buttons, ButtonId::Menu, "Menu", {650, 540, 140, 44});
        return buttons;
    }
    add(buttons, ButtonId::Menu, "Menu", {790, 8, 120, 30});
    add(buttons, ButtonId::Save, "Sauvegarder", {920, 8, 160, 30});
    add(buttons, ButtonId::Journal, "Journal", {1090, 8, 170, 30}, 0, true, app.journal_open);
    if (app.curtain || app.journal_open || !human_turn(app) || game.step() != TurnStep::Action) {
        return buttons;
    }

    std::vector<Button> actions;
    if (total(app.selection) > 0) {
        add(actions, ButtonId::Take, "Prendre", {});
        add(actions, ButtonId::Cancel, "Annuler", {});
    } else if (app.target == Target::Visible) {
        add(actions, ButtonId::Buy, "Acheter", {}, 0, game.can_buy(app.target_level, app.target_index + 1));
        if (game.mode() == Mode::Classic) {
            add(actions, ButtonId::Reserve, "Réserver", {}, 0, game.can_reserve());
        }
        add(actions, ButtonId::Cancel, "Annuler", {});
    } else if (app.target == Target::Deck) {
        add(actions, ButtonId::Reserve, "Réserver", {}, 0, game.can_reserve());
        add(actions, ButtonId::Cancel, "Annuler", {});
    } else if (app.target == Target::Reserved) {
        add(actions, ButtonId::Buy, "Acheter", {}, 0, game.can_buy_reserved(app.target_index + 1));
        add(actions, ButtonId::Cancel, "Annuler", {});
    }
    // Deux boutons par ligne dans le panneau d'action
    for (int i = 0; i < static_cast<int>(actions.size()); i++) {
        actions[i].rect = {522.0f + (i % 2) * 122, 672.0f + (i / 2) * 44, 112, 36};
        buttons.push_back(actions[i]);
    }
    return buttons;
}

std::vector<Button> visible_buttons(const App& app)
{
    if (app.screen == Screen::Home) {
        return home_buttons();
    }
    if (app.screen == Screen::Setup) {
        return setup_buttons(app.setup);
    }
    return board_buttons(app);
}

// ---------------------------------------------------------------- Couleurs

static Color gem_color(int gem)
{
    switch (gem) {
    case 0: return {242, 242, 237, 255};  // blanc (diamant)
    case 1: return {41, 97, 209, 255};    // bleu (saphir)
    case 2: return {26, 148, 77, 255};    // vert (émeraude)
    case 3: return {217, 41, 46, 255};    // rouge (rubis)
    case 4: return {31, 31, 33, 255};     // noir (onyx)
    default: return {245, 194, 31, 255};  // or
    }
}

// Couleur du texte lisible sur une gemme
static Color text_on(int gem)
{
    return (gem == 0 || gem == GOLD_INDEX) ? Color{40, 40, 40, 255} : Color{255, 255, 255, 255};
}

static Color mix(Color a, Color b, float t)
{
    return {static_cast<unsigned char>(a.r + (b.r - a.r) * t), static_cast<unsigned char>(a.g + (b.g - a.g) * t),
            static_cast<unsigned char>(a.b + (b.b - a.b) * t), static_cast<unsigned char>(a.a + (b.a - a.a) * t)};
}

static Color alpha(Color color, float value)
{
    color.a = static_cast<unsigned char>(color.a * value);
    return color;
}

const Color GOLD_UI = {255, 209, 64, 255};    // titres, boutons, cartes achetables
const Color WHITE_TEXT = {255, 255, 255, 255};
const Color SOFT_TEXT = {205, 220, 210, 255};
const Color DARK_TEXT = {40, 40, 40, 255};
const Color ERROR_TEXT = {255, 140, 128, 255};

static Color player_color(const App& app, int player)
{
    int choice = player < static_cast<int>(app.player_colors.size()) ? app.player_colors[player] : player;
    return PLAYER_COLORS[choice % PLAYER_COLOR_COUNT].color;
}

// ---------------------------------------------------------------- Outils de dessin

static void rounded(Rectangle rect, float radius, Color color)
{
    float roundness = std::min(1.0f, radius * 2 / std::min(rect.width, rect.height));
    DrawRectangleRounded(rect, roundness, 8, color);
}

static void outline(Rectangle rect, float radius, float thick, Color color, float expand = 2)
{
    Rectangle grown = {rect.x - expand, rect.y - expand, rect.width + 2 * expand, rect.height + 2 * expand};
    float roundness = std::min(1.0f, (radius + expand) * 2 / std::min(grown.width, grown.height));
    DrawRectangleRoundedLinesEx(grown, roundness, 8, thick, color);
}

static void shadow(Rectangle rect, float radius)
{
    rounded({rect.x + 3, rect.y + 4, rect.width, rect.height}, radius, {0, 0, 0, 90});
}

enum class Align { Left, Center, Right };

static Vector2 measure(const App& app, const std::string& text, float size, bool bold)
{
    return MeasureTextEx(bold ? app.bold : app.font, text.c_str(), size, 0);
}

// Écrit `text` ; y est le haut du texte
static void text(const App& app, const std::string& str, float x, float y, float size, bool bold, Color color,
                 Align align = Align::Left)
{
    Vector2 size_px = measure(app, str, size, bold);
    if (align == Align::Center) {
        x -= size_px.x / 2;
    } else if (align == Align::Right) {
        x -= size_px.x;
    }
    DrawTextEx(bold ? app.bold : app.font, str.c_str(), {x, y}, size, 0, color);
}

// Écrit `text` centré sur le point (cx, cy)
static void text_centered(const App& app, const std::string& str, float cx, float cy, float size, bool bold,
                          Color color)
{
    Vector2 size_px = measure(app, str, size, bold);
    DrawTextEx(bold ? app.bold : app.font, str.c_str(), {cx - size_px.x / 2, cy - size_px.y / 2}, size, 0, color);
}

// Texte réduit (puis coupé) pour tenir dans `width`
static std::string fit(const App& app, const std::string& str, float width, float& size, float min_size)
{
    while (size > min_size && measure(app, str, size, false).x > width) {
        size -= 0.5f;
    }
    std::string cut = str;
    while (!cut.empty() && measure(app, cut + "…", size, false).x > width) {
        cut.pop_back();
        while (!cut.empty() && (static_cast<unsigned char>(cut.back()) & 0xC0) == 0x80) {
            cut.pop_back();
        }
        if (!cut.empty() && (static_cast<unsigned char>(cut.back()) & 0xC0) == 0xC0) {
            cut.pop_back();
        }
    }
    return cut.size() == str.size() ? str : cut + "…";
}

// Contour en pointillés d'un rectangle
static void dashed_rect(Rectangle rect, Color color)
{
    const float dash = 6;
    const float gap = 4;
    for (float x = rect.x; x < rect.x + rect.width; x += dash + gap) {
        float end = std::min(x + dash, rect.x + rect.width);
        DrawLineEx({x, rect.y}, {end, rect.y}, 2, color);
        DrawLineEx({x, rect.y + rect.height}, {end, rect.y + rect.height}, 2, color);
    }
    for (float y = rect.y; y < rect.y + rect.height; y += dash + gap) {
        float end = std::min(y + dash, rect.y + rect.height);
        DrawLineEx({rect.x, y}, {rect.x, end}, 2, color);
        DrawLineEx({rect.x + rect.width, y}, {rect.x + rect.width, end}, 2, color);
    }
}

// Symbole propre à chaque gemme (pour les daltoniens) :
// blanc = cercle, bleu = losange, vert = triangle, rouge = carré, noir = hexagone, or = étoile
static void gem_symbol(int gem, Vector2 center, float size, Color color)
{
    switch (gem) {
    case 0: DrawCircleV(center, size * 0.8f, color); break;
    case 1: DrawPoly(center, 4, size, 0, color); break;
    case 2: DrawPoly({center.x, center.y + size * 0.15f}, 3, size, -90, color); break;
    case 3: DrawPoly(center, 4, size, 45, color); break;
    case 4: DrawPoly(center, 6, size, 0, color); break;
    default:
        DrawPoly(center, 3, size, -90, color);
        DrawPoly(center, 3, size, 90, color);
        break;
    }
}

// Une gemme de bonus : le symbole dans sa couleur, sur une pastille sombre
static void draw_gem(int gem, Vector2 center, float size)
{
    DrawCircleV(center, size * 1.3f, {0, 0, 0, static_cast<unsigned char>(gem == 0 ? 120 : 70)});
    gem_symbol(gem, center, size, gem_color(gem));
    Color edge = gem == 0 ? Color{120, 120, 120, 255} : Color{255, 255, 255, 200};
    switch (gem) {
    case 0: DrawCircleLinesV(center, size * 0.8f, edge); break;
    case 1: DrawPolyLinesEx(center, 4, size, 0, 1.5f, edge); break;
    case 2: DrawPolyLinesEx({center.x, center.y + size * 0.15f}, 3, size, -90, 1.5f, edge); break;
    case 3: DrawPolyLinesEx(center, 4, size, 45, 1.5f, edge); break;
    default: DrawPolyLinesEx(center, 6, size, 0, 1.5f, edge); break;
    }
}

// Un jeton : disque coloré à liseré pointillé, symbole et nombre
static void draw_token(const App& app, int gem, Vector2 center, float radius, const std::string& label,
                       bool faded)
{
    float a = faded ? 0.3f : 1.0f;
    Color color = gem_color(gem);
    DrawCircleV({center.x + 2, center.y + 3}, radius, alpha({0, 0, 0, 90}, a));
    DrawCircleV(center, radius, alpha(mix(color, {0, 0, 0, 255}, 0.25f), a));
    DrawCircleV(center, radius * 0.82f, alpha(color, a));
    Color dashes = gem == 0 ? Color{170, 170, 170, 255} : Color{255, 255, 255, 150};
    for (int i = 0; i < 12; i++) {
        float start = i * 30.0f;
        DrawRing(center, radius * 0.86f, radius * 0.94f, start, start + 15, 4, alpha(dashes, a));
    }
    if (radius >= 18) {
        gem_symbol(gem, {center.x, center.y - radius * 0.52f}, radius * 0.16f, alpha(text_on(gem), 0.7f * a));
    }
    if (!label.empty()) {
        text_centered(app, label, center.x, center.y + (radius >= 18 ? radius * 0.1f : 0), radius * 0.9f, true,
                      alpha(text_on(gem), a));
    }
}

// ---------------------------------------------------------------- Éléments du jeu

// Une carte : bandeau de sa couleur avec les points et la gemme, coûts en bas à gauche
static void draw_card(const App& app, const Card& card, Rectangle r)
{
    int gem = index_of(card.provides);
    Color band = gem == 0 ? Color{255, 255, 255, 255} : gem_color(gem);
    Color body = mix(gem_color(gem), {255, 255, 255, 255}, gem == 0 ? 0.2f : 0.7f);
    if (gem == 0) {
        body = {236, 236, 232, 255};
    }
    shadow(r, 8);
    rounded(r, 8, band);
    rounded({r.x, r.y + 36, r.width, r.height - 36}, 8, body);
    DrawRectangleRec({r.x, r.y + 36, r.width, 10}, body);

    if (card.points > 0) {
        text(app, std::to_string(card.points), r.x + 10, r.y + 3, 30, true, text_on(gem));
    }
    draw_gem(gem, {r.x + r.width - 20, r.y + 18}, 11);

    // Coûts empilés depuis le bas
    int line = 0;
    for (int c = COLOR_COUNT - 1; c >= 0; c--) {
        if (card.cost[c] > 0) {
            draw_token(app, c, {r.x + 19, r.y + r.height - 18 - line * 26}, 12, std::to_string(card.cost[c]), false);
            line++;
        }
    }
    outline(r, 8, 1, {0, 0, 0, 100}, 0);
}

// Contour doré d'une carte achetable, avec un halo
static void buyable_outline(Rectangle r, float expand)
{
    outline(r, 8, 6, alpha(GOLD_UI, 0.35f), expand + 2);
    outline(r, 8, 3, GOLD_UI, expand);
}

// Dos de carte (pioche, ou carte qui se retourne)
static void draw_card_back(const App& app, int level, Rectangle r, bool label)
{
    const Color backs[] = {{41, 128, 71, 255}, {204, 153, 31, 255}, {46, 82, 173, 255}};
    const char* roman[] = {"I", "II", "III"};
    rounded(r, 8, backs[level - 1]);
    if (r.width > 20) {
        outline({r.x + 7, r.y + 7, r.width - 14, r.height - 14}, 5, 1.5f, {255, 255, 255, 150}, 0);
    }
    if (label) {
        text_centered(app, roman[level - 1], r.x + r.width / 2, r.y + 50, 34, true, WHITE_TEXT);
    }
}

static void draw_deck(const App& app, int level, int count, bool selected)
{
    Rectangle r = deck_rect(level);
    if (count == 0) {
        dashed_rect(r, {255, 255, 255, 80});
        text_centered(app, "vide", r.x + r.width / 2, r.y + r.height / 2, 15, false, SOFT_TEXT);
        return;
    }
    // Petit décalage pour donner l'impression d'une pile
    shadow(r, 8);
    for (int i = 2; i >= 1; i--) {
        rounded({r.x + i * 2.0f, r.y - i * 2.0f, r.width, r.height}, 8, {0, 0, 0, 60});
    }
    draw_card_back(app, level, r, true);
    text_centered(app, std::to_string(count), r.x + r.width / 2, r.y + 96, 20, true, WHITE_TEXT);
    text_centered(app, "cartes", r.x + r.width / 2, r.y + 117, 12, false, WHITE_TEXT);
    if (selected) {
        outline(r, 8, 4, WHITE_TEXT);
    }
}

// Tuile noble : nom en haut, bonus demandés à gauche, points à droite
static void draw_noble(const App& app, const Noble& noble, Rectangle r, bool highlighted)
{
    const Color ink = {77, 51, 26, 255};
    shadow(r, 6);
    rounded(r, 6, {237, 219, 179, 255});
    outline({r.x + 4, r.y + 4, r.width - 8, r.height - 8}, 4, 1.5f, {158, 122, 71, 255}, 0);

    float size = 12;
    std::string name = fit(app, noble.name, r.width - 10, size, 7);
    text(app, name, r.x + r.width / 2, r.y + 8, size, true, ink, Align::Center);

    int line = 0;
    for (int c = 0; c < COLOR_COUNT; c++) {
        if (noble.cost[c] > 0) {
            Rectangle box = {r.x + 10, r.y + 26 + line * 23.0f, 18, 21};
            rounded(box, 3, gem_color(c));
            outline(box, 3, 1, {0, 0, 0, 100}, 0);
            text_centered(app, std::to_string(noble.cost[c]), box.x + box.width / 2, box.y + box.height / 2, 14,
                          true, text_on(c));
            line++;
        }
    }
    text_centered(app, std::to_string(noble.points), r.x + r.width - 26, r.y + 60, 38, true, ink);
    if (highlighted) {
        outline(r, 6, 4, GOLD_UI);
    }
}

// ---------------------------------------------------------------- Zones du plateau

static void draw_felt()
{
    DrawRectangle(0, 0, static_cast<int>(BOARD_WIDTH), static_cast<int>(BOARD_HEIGHT), {10, 43, 28, 255});
    DrawCircleGradient(static_cast<int>(BOARD_WIDTH / 2), static_cast<int>(BOARD_HEIGHT / 2), 850,
                       {26, 92, 61, 255}, {10, 43, 28, 0});
}

static void draw_header(const App& app)
{
    const Game& game = *app.game;
    text(app, "SPLENDOR", 30, 8, 28, true, GOLD_UI);
    std::string mode = game.mode() == Mode::Classic ? "Partie classique" : "Partie rapide";
    text(app, mode, 30, 44, 13, false, SOFT_TEXT);
    text(app, "Tour " + std::to_string(game.round()), 30, 62, 13, false, SOFT_TEXT);
    text(app, "Nobles", 30, 96, 15, true, WHITE_TEXT);
    if (game.is_last_round() && !game.is_over()) {
        text(app, "Dernier tour !", 30, 120, 14, true, ERROR_TEXT);
    }
}

static bool is_flipping(const App& app, int level, int index, float& progress)
{
    double now = GetTime();
    for (const Flip& flip : app.flips) {
        if (flip.level == level && flip.index == index && now - flip.start < 0.5) {
            progress = static_cast<float>((now - flip.start) / 0.5);
            return true;
        }
    }
    return false;
}

static void draw_cards(const App& app)
{
    const Game& game = *app.game;
    bool can_act = human_turn(app) && game.step() == TurnStep::Action && !app.curtain;
    for (int level = 1; level <= LEVEL_COUNT; level++) {
        draw_deck(app, level, game.deck_size(level), app.target == Target::Deck && app.target_level == level);
        const std::vector<Card>& row = game.visible(level);
        for (int i = 0; i < static_cast<int>(row.size()); i++) {
            Rectangle r = card_rect(level, i);
            float progress = 0;
            if (is_flipping(app, level, i, progress) && progress < 0.5f) {
                // Première moitié : le dos se referme ; ensuite la face apparaît
                float width = r.width * (1 - 2 * progress);
                draw_card_back(app, level, {r.x + (r.width - width) / 2, r.y, width, r.height}, false);
                continue;
            }
            draw_card(app, row[i], r);
            if (app.target == Target::Visible && app.target_level == level && app.target_index == i) {
                outline(r, 8, 4, WHITE_TEXT);
            } else if (can_act && game.can_buy(level, i + 1)) {
                buyable_outline(r, 2);
            }
        }
    }
}

static void draw_nobles(const App& app)
{
    const Game& game = *app.game;
    for (int i = 0; i < static_cast<int>(game.nobles().size()); i++) {
        bool highlighted = false;
        if (game.step() == TurnStep::ChooseNoble && human_turn(app)) {
            for (int choice : game.noble_choices()) {
                highlighted = highlighted || choice == i;
            }
        }
        draw_noble(app, game.nobles()[i], noble_rect(i), highlighted);
    }
}

static void draw_bank(const App& app)
{
    const Game& game = *app.game;
    text(app, "Réserve", 668, 140, 15, true, WHITE_TEXT, Align::Center);
    int count = game.mode() == Mode::Classic ? COLOR_COUNT + 1 : COLOR_COUNT;
    for (int gem = 0; gem < count; gem++) {
        Vector2 center = bank_token_center(gem);
        int amount = gem == GOLD_INDEX ? game.bank_gold() : game.bank()[gem];
        draw_token(app, gem, center, BANK_TOKEN_RADIUS, std::to_string(amount), amount == 0);
        // Pastille « +1 » / « +2 » sur les jetons choisis
        if (gem < COLOR_COUNT && app.selection[gem] > 0) {
            DrawCircleV({center.x + 22, center.y - 20}, 12, GOLD_UI);
            text_centered(app, "+" + std::to_string(app.selection[gem]), center.x + 22, center.y - 20, 13, true,
                          DARK_TEXT);
        }
        if (gem < COLOR_COUNT) {
            text_centered(app, std::to_string(gem + 1), center.x - 40, center.y, 12, false, alpha(SOFT_TEXT, 0.6f));
        }
    }
}

static void draw_players(const App& app)
{
    const Game& game = *app.game;
    bool classic = game.mode() == Mode::Classic;
    for (int p = 0; p < static_cast<int>(game.players().size()); p++) {
        const Player& player = game.players()[p];
        bool current = p == game.current_index() && !game.is_over();
        Rectangle r = player_rect(p);
        Color own = player_color(app, p);

        rounded(r, 10, {0, 0, 0, static_cast<unsigned char>(current ? 115 : 70)});
        // Bande de couleur du joueur ; son tour : tout le panneau est entouré de sa couleur
        rounded({r.x, r.y, 14, r.height}, 7, own);
        DrawRectangleRec({r.x + 7, r.y, 7, r.height}, {0, 0, 0, static_cast<unsigned char>(current ? 115 : 70)});
        if (current) {
            outline(r, 10, 4, own, 0);
        }

        float name_x = r.x + 20;
        if (current) {
            DrawTriangle({r.x + 20, r.y + 13}, {r.x + 20, r.y + 31}, {r.x + 34, r.y + 22}, own);
            name_x += 22;
        }
        text(app, player.name, name_x, r.y + 10, 21, true, own);
        if (player.controller != Controller::Human) {
            float width = measure(app, player.name, 21, true).x;
            std::string kind = player.controller == Controller::AiEasy ? "(ordinateur facile)" : "(ordinateur moyen)";
            text(app, kind, name_x + width + 10, r.y + 16, 12, false, SOFT_TEXT);
        }
        text(app, std::to_string(player.score()), r.x + r.width - 52, r.y + 6, 34, true, WHITE_TEXT, Align::Right);
        text(app, "pts", r.x + r.width - 15, r.y + 22, 14, false, WHITE_TEXT, Align::Right);

        // Bonus : petites cartes de chaque couleur, avec le symbole
        text(app, "Bonus", r.x + 20, r.y + 60, 13, false, SOFT_TEXT);
        Resources bonus = player.bonuses();
        for (int c = 0; c < COLOR_COUNT; c++) {
            Rectangle box = {r.x + 95 + c * 52.0f, r.y + 48, 30, 38};
            rounded(box, 4, alpha(gem_color(c), bonus[c] == 0 ? 0.3f : 1.0f));
            gem_symbol(c, {box.x + 7, box.y + 7}, 3.5f, alpha(text_on(c), bonus[c] == 0 ? 0.3f : 0.7f));
            if (bonus[c] > 0) {
                text_centered(app, std::to_string(bonus[c]), box.x + box.width / 2, box.y + box.height / 2 + 3, 18,
                              true, text_on(c));
            }
        }

        // Jetons en main
        text(app, "Jetons", r.x + 20, r.y + 112, 13, false, SOFT_TEXT);
        int kinds = classic ? COLOR_COUNT + 1 : COLOR_COUNT;
        for (int gem = 0; gem < kinds; gem++) {
            int amount = gem == GOLD_INDEX ? player.gold : player.tokens[gem];
            draw_token(app, gem, player_token_center(p, gem), PLAYER_TOKEN_RADIUS, std::to_string(amount), amount == 0);
        }

        std::string info = std::to_string(player.token_count()) + "/10 jetons   "
                           + std::to_string(player.cards.size()) + " cartes   "
                           + std::to_string(player.nobles.size()) + " noble(s)";
        if (classic) {
            info += "   " + std::to_string(player.reserved.size()) + " réservée(s)";
        }
        text(app, info, r.x + 20, r.y + 150, 13, false, SOFT_TEXT);

        if (current && game.step() == TurnStep::Discard && human_turn(app)) {
            outline({r.x + 85, r.y + 98, 52.0f * kinds, 48}, 8, 2, ERROR_TEXT, 0);
        }
    }
}

static void draw_reserved(const App& app)
{
    const Game& game = *app.game;
    if (game.mode() != Mode::Classic || game.is_over()) {
        return;
    }
    const Player& me = game.current_player();
    Color own = player_color(app, game.current_index());
    text(app, "Réservées", 30, 645, 14, true, WHITE_TEXT);
    text(app, "de", 30, 664, 14, false, WHITE_TEXT);
    float size = 16;
    text(app, fit(app, me.name, 110, size, 10), 30, 684, size, true, own);

    bool can_act = human_turn(app) && game.step() == TurnStep::Action;
    for (int i = 0; i < MAX_RESERVED; i++) {
        Rectangle r = reserved_rect(i);
        if (i < static_cast<int>(me.reserved.size()) && !app.curtain) {
            draw_card(app, me.reserved[i], r);
            outline(r, 8, 4, own);
            if (app.target == Target::Reserved && app.target_index == i) {
                outline(r, 8, 3, WHITE_TEXT, 7);
            } else if (can_act && game.can_buy_reserved(i + 1)) {
                buyable_outline(r, 7);
            }
        } else if (i < static_cast<int>(me.reserved.size())) {
            draw_card_back(app, me.reserved[i].points >= 3 ? 3 : 1, r, false);  // caché derrière le rideau
        } else {
            dashed_rect(r, alpha(own, 0.45f));
        }
    }
}

static void draw_button(const App& app, const Button& button, Vector2 mouse)
{
    bool hover = button.enabled && point_in(mouse, button.rect);
    Color fill;
    Color ink;
    if (!button.enabled) {
        fill = {128, 128, 128, 110};
        ink = {70, 70, 70, 255};
    } else if (button.selected || button.id == ButtonId::Take || button.id == ButtonId::Buy
               || button.id == ButtonId::Reserve || button.id == ButtonId::Cancel || button.id == ButtonId::Start
               || button.id == ButtonId::NewGame || button.id == ButtonId::Resume || button.id == ButtonId::Replay) {
        fill = hover ? mix(GOLD_UI, {255, 255, 255, 255}, 0.3f) : GOLD_UI;
        ink = DARK_TEXT;
    } else {
        fill = hover ? Color{255, 255, 255, 60} : Color{255, 255, 255, 30};
        ink = WHITE_TEXT;
    }
    rounded(button.rect, 8, fill);
    float size = button.rect.height >= 50 ? 19 : 15;
    text_centered(app, button.label, button.rect.x + button.rect.width / 2, button.rect.y + button.rect.height / 2,
                  size, true, ink);
}

// Panneau d'action : ce qui est sélectionné, et les boutons
static void draw_action_panel(const App& app)
{
    const Game& game = *app.game;
    Rectangle r = {510, 615, 250, 145};
    rounded(r, 10, {0, 0, 0, 80});

    std::string title;
    if (!human_turn(app)) {
        title = game.current_player().name + " réfléchit…";
    } else if (game.step() == TurnStep::Discard) {
        title = "Défaussez vos jetons en trop";
    } else if (game.step() == TurnStep::ChooseNoble) {
        title = "Choisissez un noble";
    } else if (total(app.selection) > 0) {
        title = "Jetons choisis :";
    } else if (app.target == Target::Visible || app.target == Target::Reserved) {
        title = "Carte choisie";
    } else if (app.target == Target::Deck) {
        title = "Pioche du niveau " + std::to_string(app.target_level);
    } else {
        title = "Choisissez une action";
    }
    float size = 15;
    text(app, fit(app, title, r.width - 24, size, 11), r.x + 12, r.y + 12, size, true, WHITE_TEXT);

    if (total(app.selection) > 0) {
        int k = 0;
        for (int gem = 0; gem < COLOR_COUNT; gem++) {
            for (int n = 0; n < app.selection[gem]; n++) {
                draw_token(app, gem, {r.x + 145 + k * 26.0f, r.y + 22}, 11, "", false);
                k++;
            }
        }
    } else if (human_turn(app) && game.step() == TurnStep::Action && app.target == Target::None) {
        text(app, "Cliquez des jetons (touches 1 à 5),", r.x + 12, r.y + 40, 13, false, SOFT_TEXT);
        text(app, "une carte ou une pioche.", r.x + 12, r.y + 58, 13, false, SOFT_TEXT);
        text(app, "Entrée valide, Échap annule.", r.x + 12, r.y + 76, 13, false, SOFT_TEXT);
        text(app, "Contour doré = carte achetable", r.x + 12, r.y + 104, 13, false, GOLD_UI);
    }
}

static void draw_status(const App& app)
{
    if (app.message.empty()) {
        return;
    }
    float size = 15;
    std::string line = fit(app, app.message, 740, size, 11);
    text(app, line, 30, 774, size, app.message_is_error, app.message_is_error ? ERROR_TEXT : SOFT_TEXT);
}

// Infobulle au survol : ce qu'il manque pour une carte, un noble...
static void draw_tooltip(const App& app, Vector2 mouse)
{
    const Game& game = *app.game;
    if (!human_turn(app) || app.curtain || app.journal_open) {
        return;
    }
    std::string tip;
    for (int level = 1; level <= LEVEL_COUNT && tip.empty(); level++) {
        for (int i = 0; i < static_cast<int>(game.visible(level).size()) && tip.empty(); i++) {
            if (point_in(mouse, card_rect(level, i))) {
                tip = missing_text(game, game.visible(level)[i]);
                if (tip.empty()) {
                    tip = "Vous pouvez acheter cette carte.";
                }
            }
        }
        if (tip.empty() && point_in(mouse, deck_rect(level))) {
            tip = "Pioche du niveau " + std::to_string(level) + " : " + std::to_string(game.deck_size(level))
                  + " cartes" + (game.mode() == Mode::Classic ? " (cliquez pour réserver le dessus)" : "");
        }
    }
    if (game.mode() == Mode::Classic) {
        const std::vector<Card>& reserved = game.current_player().reserved;
        for (int i = 0; i < static_cast<int>(reserved.size()) && tip.empty(); i++) {
            if (point_in(mouse, reserved_rect(i))) {
                tip = missing_text(game, reserved[i]);
                if (tip.empty()) {
                    tip = "Vous pouvez acheter cette carte.";
                }
            }
        }
    }
    Resources bonus = game.current_player().bonuses();
    for (int i = 0; i < static_cast<int>(game.nobles().size()) && tip.empty(); i++) {
        if (point_in(mouse, noble_rect(i))) {
            std::string missing;
            for (int c = 0; c < COLOR_COUNT; c++) {
                int gap = game.nobles()[i].cost[c] - bonus[c];
                if (gap > 0) {
                    missing += (missing.empty() ? "" : ", ") + std::to_string(gap) + " " + color_name(color_at(c));
                }
            }
            tip = missing.empty() ? "Ce noble vous rendra visite." : "Bonus manquants : " + missing;
        }
    }
    if (tip.empty()) {
        return;
    }
    Vector2 size = measure(app, tip, 14, false);
    Rectangle box = {mouse.x + 16, mouse.y + 18, size.x + 20, 30};
    if (box.x + box.width > BOARD_WIDTH - 10) {
        box.x = BOARD_WIDTH - 10 - box.width;
    }
    if (box.y + box.height > BOARD_HEIGHT - 10) {
        box.y = mouse.y - 40;
    }
    rounded(box, 6, {20, 30, 25, 235});
    outline(box, 6, 1, GOLD_UI, 0);
    text(app, tip, box.x + 10, box.y + 7, 14, false, WHITE_TEXT);
}

static void draw_animations(const App& app)
{
    double now = GetTime();
    for (const Animation& anim : app.animations) {
        float t = static_cast<float>((now - anim.start) / anim.duration);
        if (t < 0 || t > 1) {
            continue;
        }
        float ease = 1 - (1 - t) * (1 - t);  // ralentit à l'arrivée
        Vector2 pos = {anim.from.x + (anim.to.x - anim.from.x) * ease, anim.from.y + (anim.to.y - anim.from.y) * ease};
        if (anim.gem >= 0) {
            draw_token(app, anim.gem, pos, 16, "", false);
        } else {
            float scale = 1 - 0.6f * ease;
            draw_card(app, anim.card, {pos.x - 50 * scale, pos.y - 70 * scale, 100 * scale, 140 * scale});
        }
    }
}

static void draw_journal(const App& app)
{
    const Game& game = *app.game;
    Rectangle r = {300, 110, 680, 580};
    DrawRectangle(0, 0, static_cast<int>(BOARD_WIDTH), static_cast<int>(BOARD_HEIGHT), {0, 0, 0, 120});
    rounded(r, 14, {18, 48, 34, 245});
    outline(r, 14, 2, GOLD_UI, 0);
    text(app, "Journal de la partie", r.x + 24, r.y + 18, 24, true, GOLD_UI);
    text(app, "J ou clic pour fermer", r.x + r.width - 24, r.y + 26, 13, false, SOFT_TEXT, Align::Right);

    const std::vector<Event>& events = game.events();
    const int lines = 21;
    int first = std::max(0, static_cast<int>(events.size()) - lines);
    if (events.empty()) {
        text(app, "Rien pour l'instant.", r.x + 24, r.y + 70, 15, false, SOFT_TEXT);
    }
    for (int i = first; i < static_cast<int>(events.size()); i++) {
        float y = r.y + 64 + (i - first) * 23.0f;
        DrawCircleV({r.x + 30, y + 9}, 5, player_color(app, events[i].player));
        float size = 15;
        text(app, fit(app, describe_event(game, events[i]), r.width - 70, size, 11), r.x + 44, y, size, false,
             WHITE_TEXT);
    }
}

// Écran de passage : cache les cartes réservées entre deux joueurs humains
static void draw_curtain(const App& app)
{
    const Game& game = *app.game;
    DrawRectangle(0, 0, static_cast<int>(BOARD_WIDTH), static_cast<int>(BOARD_HEIGHT), {6, 26, 17, 235});
    text_centered(app, "Au tour de", 640, 330, 26, false, SOFT_TEXT);
    text_centered(app, game.current_player().name, 640, 385, 56, true, player_color(app, game.current_index()));
    text_centered(app, "Cliquez ou appuyez sur Entrée pour afficher le plateau", 640, 460, 18, false, SOFT_TEXT);
}

static void draw_results(const App& app, Vector2 mouse)
{
    const Game& game = *app.game;
    DrawRectangle(0, 0, static_cast<int>(BOARD_WIDTH), static_cast<int>(BOARD_HEIGHT), {0, 0, 0, 165});
    Rectangle r = {390, 190, 500, 420};
    rounded(r, 16, {31, 77, 54, 255});
    outline(r, 16, 3, GOLD_UI, 0);
    text_centered(app, "Fin de partie", 640, 235, 34, true, GOLD_UI);
    if (game.is_blocked()) {
        text_centered(app, "(plus aucun joueur ne pouvait jouer)", 640, 268, 13, false, WHITE_TEXT);
    }
    std::vector<int> winners = game.winners();
    std::string headline = winners.size() == 1 ? "Victoire de " + game.players()[winners[0]].name + " !"
                                               : std::string("Égalité parfaite !");
    text_centered(app, headline, 640, 305, 24, true, WHITE_TEXT);
    for (int i = 0; i < static_cast<int>(game.players().size()); i++) {
        const Player& player = game.players()[i];
        float y = 345 + i * 36.0f;
        text(app, player.name, 440, y, 19, true, player_color(app, i));
        text(app, std::to_string(player.score()) + " pts", 720, y, 19, true, GOLD_UI, Align::Right);
        text(app, std::to_string(player.cards.size()) + " cartes", 840, y + 2, 15, false, WHITE_TEXT, Align::Right);
    }
    for (const Button& button : board_buttons(app)) {
        draw_button(app, button, mouse);
    }
}

static void draw_board(const App& app, Vector2 mouse)
{
    draw_felt();
    draw_header(app);
    draw_nobles(app);
    draw_cards(app);
    draw_bank(app);
    draw_players(app);
    draw_reserved(app);
    if (app.game->is_over()) {
        draw_results(app, mouse);
        return;
    }
    draw_action_panel(app);
    draw_status(app);
    for (const Button& button : visible_buttons(app)) {
        draw_button(app, button, mouse);
    }
    draw_animations(app);
    if (app.journal_open) {
        draw_journal(app);
    } else if (app.curtain) {
        draw_curtain(app);
    } else {
        draw_tooltip(app, mouse);
    }
}

// ---------------------------------------------------------------- Accueil et nouvelle partie

static void draw_home(const App& app, Vector2 mouse)
{
    draw_felt();
    // Une rangée de jetons décoratifs
    for (int gem = 0; gem <= GOLD_INDEX; gem++) {
        draw_token(app, gem, {640 + (gem - 2.5f) * 78, 175}, 30, "", false);
    }
    text_centered(app, "SPLENDOR", 640, 270, 96, true, GOLD_UI);
    text_centered(app, std::string("Version ") + SPLENDOR_VERSION, 640, 335, 16, false, SOFT_TEXT);
    for (const Button& button : home_buttons()) {
        draw_button(app, button, mouse);
    }
    if (!FileExists(SAVE_FILE)) {
        text_centered(app, "(aucune sauvegarde trouvée)", 640, 612, 13, false, SOFT_TEXT);
    }
    if (!app.message.empty()) {
        text_centered(app, app.message, 640, 650, 15, false, app.message_is_error ? ERROR_TEXT : SOFT_TEXT);
    }
}

static void draw_setup(const App& app, Vector2 mouse)
{
    const Setup& setup = app.setup;
    draw_felt();
    text(app, "Nouvelle partie", 200, 40, 40, true, GOLD_UI);
    text(app, "Mode", 200, 128, 19, true, WHITE_TEXT);
    text(app, setup.mode == Mode::Classic ? "Avec or et réservation (règles complètes)"
                                          : "Sans or ni réservation (plus court)",
         800, 130, 14, false, SOFT_TEXT);
    text(app, "Joueurs", 200, 198, 19, true, WHITE_TEXT);
    text(app, "Nom", 200, 252, 14, false, SOFT_TEXT);
    text(app, "Couleur", 530, 252, 14, false, SOFT_TEXT);
    text(app, "Joué par", 790, 252, 14, false, SOFT_TEXT);

    for (const Button& button : setup_buttons(setup)) {
        if (button.id == ButtonId::NameBox) {
            // Champ de saisie du nom
            rounded(button.rect, 6, {255, 255, 255, 235});
            std::string name = button.label;
            bool caret = button.selected && static_cast<int>(GetTime() * 2) % 2 == 0;
            text(app, name + (caret ? "|" : ""), button.rect.x + 12, button.rect.y + 9, 19, false, DARK_TEXT);
            if (button.selected) {
                outline(button.rect, 6, 3, GOLD_UI);
            }
        } else if (button.id == ButtonId::Swatch) {
            // Pastille de couleur du joueur
            Vector2 center = {button.rect.x + 18, button.rect.y + 18};
            Color color = PLAYER_COLORS[button.value % 10].color;
            DrawCircleV(center, 16, alpha(color, button.enabled ? 1.0f : 0.25f));
            if (button.selected) {
                DrawRing(center, 18, 21, 0, 360, 32, WHITE_TEXT);
            } else if (button.enabled && point_in(mouse, button.rect)) {
                DrawRing(center, 18, 20, 0, 360, 32, alpha(WHITE_TEXT, 0.5f));
            }
        } else {
            draw_button(app, button, mouse);
        }
    }
    if (setup.focused_name >= 0) {
        text(app, "Tapez le nom, puis Entrée.", 200, 650, 14, false, SOFT_TEXT);
    }
}

void draw_app(const App& app, Vector2 mouse)
{
    if (app.screen == Screen::Home) {
        draw_home(app, mouse);
    } else if (app.screen == Screen::Setup) {
        draw_setup(app, mouse);
    } else if (app.game) {
        draw_board(app, mouse);
    }
}

std::string missing_text(const Game& game, const Card& card)
{
    const Player& me = game.current_player();
    if (missing_tokens(me.bonuses(), me.tokens, card) <= me.gold) {
        return "";
    }
    Resources to_pay = tokens_to_pay(me.bonuses(), card);
    std::string text_out;
    for (int c = 0; c < COLOR_COUNT; c++) {
        int gap = to_pay[c] - me.tokens[c];
        if (gap > 0) {
            text_out += (text_out.empty() ? "" : ", ") + std::to_string(gap) + " " + color_name(color_at(c));
        }
    }
    text_out = "Il manque " + text_out;
    if (me.gold > 0) {
        text_out += " (vous avez " + std::to_string(me.gold) + " or)";
    }
    return text_out;
}
