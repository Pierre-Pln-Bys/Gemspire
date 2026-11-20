#include "gui.hpp"

#include <cmath>

#include "display.hpp"
#include "rules.hpp"

// ---------------------------------------------------------------- Placement

// Ordonnée de la rangée d'un niveau (niveau 3 en haut, comme sur la table)
static double row_y(int level)
{
    return 150 + (LEVEL_COUNT - level) * 150;
}

Rect noble_rect(int index) { return {150.0 + index * 105, 40, 90, 100}; }
Rect deck_rect(int level) { return {30, row_y(level), 100, 140}; }
Rect card_rect(int level, int index) { return {150.0 + index * 115, row_y(level), 100, 140}; }
Rect reserved_rect(int index) { return {150.0 + index * 115, 615, 100, 140}; }
Rect player_rect(int player) { return {790, 40.0 + player * 188, 470, 180}; }

void bank_token_center(int color_index, double& x, double& y)
{
    x = 668;
    y = 192 + color_index * 70;
}

void player_token_center(int player, int color_index, double& x, double& y)
{
    Rect panel = player_rect(player);
    x = panel.x + 110 + color_index * 52;
    y = panel.y + 122;
}

std::vector<Button> visible_buttons(const App& app)
{
    std::vector<Button> buttons;
    if (!app.game) {
        return buttons;
    }
    const Game& game = *app.game;

    if (app.phase == Phase::Over) {
        buttons.push_back({ButtonId::Replay, "Rejouer", true, {490, 520, 140, 44}});
        buttons.push_back({ButtonId::Quit, "Quitter", true, {650, 520, 140, 44}});
        return buttons;
    }
    if (app.phase != Phase::Action) {
        return buttons;
    }

    std::vector<Button> list;
    if (total(app.selection) > 0) {
        list.push_back({ButtonId::Take, "Prendre", true, {0, 0, 0, 0}});
        list.push_back({ButtonId::Cancel, "Annuler", true, {0, 0, 0, 0}});
    } else if (app.target == Target::Visible) {
        list.push_back({ButtonId::Buy, "Acheter", game.can_buy(app.target_level, app.target_index + 1), {0, 0, 0, 0}});
        if (game.mode() == Mode::Classic) {
            list.push_back({ButtonId::Reserve, "Réserver", game.can_reserve(), {0, 0, 0, 0}});
        }
        list.push_back({ButtonId::Cancel, "Annuler", true, {0, 0, 0, 0}});
    } else if (app.target == Target::Deck) {
        list.push_back({ButtonId::Reserve, "Réserver", game.can_reserve(), {0, 0, 0, 0}});
        list.push_back({ButtonId::Cancel, "Annuler", true, {0, 0, 0, 0}});
    } else if (app.target == Target::Reserved) {
        list.push_back({ButtonId::Buy, "Acheter", game.can_buy_reserved(app.target_index + 1), {0, 0, 0, 0}});
        list.push_back({ButtonId::Cancel, "Annuler", true, {0, 0, 0, 0}});
    }
    // Deux boutons par ligne dans le panneau d'action
    for (int i = 0; i < static_cast<int>(list.size()); i++) {
        list[i].rect = {522.0 + (i % 2) * 122, 672.0 + (i / 2) * 44, 112, 36};
        buttons.push_back(list[i]);
    }
    return buttons;
}

// ---------------------------------------------------------------- Couleurs

struct Rgb {
    double r, g, b;
};

static Rgb gem_color(int color_index)
{
    switch (color_index) {
    case 0: return {0.95, 0.95, 0.93};  // blanc (diamant)
    case 1: return {0.16, 0.38, 0.82};  // bleu (saphir)
    case 2: return {0.10, 0.58, 0.30};  // vert (émeraude)
    case 3: return {0.85, 0.16, 0.18};  // rouge (rubis)
    case 4: return {0.12, 0.12, 0.13};  // noir (onyx)
    default: return {0.96, 0.76, 0.12}; // or
    }
}

// Couleur du texte lisible sur cette gemme
static Rgb text_on(int color_index)
{
    if (color_index == 0 || color_index == GOLD_INDEX) {
        return {0.15, 0.15, 0.15};
    }
    return {1, 1, 1};
}

static Rgb mix(Rgb a, Rgb b, double t)
{
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}

static void set_color(cairo_t* cr, Rgb c, double alpha = 1)
{
    cairo_set_source_rgba(cr, c.r, c.g, c.b, alpha);
}

const Rgb WHITE_TEXT = {1, 1, 1};
const Rgb DARK_TEXT = {0.15, 0.15, 0.15};
const Rgb HIGHLIGHT = {1.0, 0.82, 0.25};   // doré : titres, boutons, cartes achetables
const Rgb BUYABLE = HIGHLIGHT;             // carte achetable
const Rgb SELECTED = {1, 1, 1};            // carte ou pioche sélectionnée (blanc)

// Couleur choisie par le joueur `player`
static Rgb player_rgb(const App& app, int player)
{
    int choice = player < static_cast<int>(app.player_colors.size()) ? app.player_colors[player] : player;
    const PlayerColor& color = PLAYER_COLORS[choice % PLAYER_COLOR_COUNT];
    return {color.r, color.g, color.b};
}

// ---------------------------------------------------------------- Outils de dessin

static void rounded_rect(cairo_t* cr, Rect r, double radius)
{
    cairo_new_sub_path(cr);
    cairo_arc(cr, r.x + r.w - radius, r.y + radius, radius, -M_PI / 2, 0);
    cairo_arc(cr, r.x + r.w - radius, r.y + r.h - radius, radius, 0, M_PI / 2);
    cairo_arc(cr, r.x + radius, r.y + r.h - radius, radius, M_PI / 2, M_PI);
    cairo_arc(cr, r.x + radius, r.y + radius, radius, M_PI, 3 * M_PI / 2);
    cairo_close_path(cr);
}

enum class Align { Left, Center, Right };

// Écrit `text` avec la ligne de base en y
static void draw_text(cairo_t* cr, const std::string& text, double x, double y, double size,
                      bool bold, Rgb color, Align align = Align::Left)
{
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                           bold ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, size);
    cairo_text_extents_t extents;
    cairo_text_extents(cr, text.c_str(), &extents);
    if (align == Align::Center) {
        x -= extents.x_advance / 2;
    } else if (align == Align::Right) {
        x -= extents.x_advance;
    }
    set_color(cr, color);
    cairo_move_to(cr, x, y);
    cairo_show_text(cr, text.c_str());
}

// Écrit `text` centré (horizontalement et verticalement) sur le point (cx, cy)
static void draw_text_centered(cairo_t* cr, const std::string& text, double cx, double cy,
                               double size, bool bold, Rgb color)
{
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                           bold ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, size);
    cairo_text_extents_t extents;
    cairo_text_extents(cr, text.c_str(), &extents);
    set_color(cr, color);
    cairo_move_to(cr, cx - extents.x_advance / 2, cy - extents.y_bearing - extents.height / 2);
    cairo_show_text(cr, text.c_str());
}

// Ombre portée sous un rectangle arrondi
static void draw_shadow(cairo_t* cr, Rect r, double radius)
{
    rounded_rect(cr, {r.x + 3, r.y + 4, r.w, r.h}, radius);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.35);
    cairo_fill(cr);
}

// Contour épais (sélection, carte achetable...)
static void draw_outline(cairo_t* cr, Rect r, double radius, Rgb color, double width, double expand = 2)
{
    rounded_rect(cr, {r.x - expand, r.y - expand, r.w + 2 * expand, r.h + 2 * expand}, radius + expand);
    set_color(cr, color);
    cairo_set_line_width(cr, width);
    cairo_stroke(cr);
}

// Un jeton : disque coloré avec un liseré, le nombre au centre
static void draw_token(cairo_t* cr, int color_index, double cx, double cy, double radius,
                       const std::string& label, bool faded)
{
    double alpha = faded ? 0.3 : 1;
    Rgb color = gem_color(color_index);
    cairo_arc(cr, cx + 2, cy + 3, radius, 0, 2 * M_PI);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.35 * alpha);
    cairo_fill(cr);

    cairo_arc(cr, cx, cy, radius, 0, 2 * M_PI);
    set_color(cr, mix(color, {0, 0, 0}, 0.25), alpha);
    cairo_fill(cr);
    cairo_arc(cr, cx, cy, radius * 0.82, 0, 2 * M_PI);
    set_color(cr, color, alpha);
    cairo_fill(cr);

    // Liseré pointillé, comme sur les vrais jetons
    double dashes[] = {4, 4};
    cairo_set_dash(cr, dashes, 2, 0);
    cairo_arc(cr, cx, cy, radius * 0.9, 0, 2 * M_PI);
    set_color(cr, color_index == 0 ? Rgb{0.7, 0.7, 0.7} : Rgb{1, 1, 1}, 0.6 * alpha);
    cairo_set_line_width(cr, 2);
    cairo_stroke(cr);
    cairo_set_dash(cr, nullptr, 0, 0);

    if (!label.empty()) {
        draw_text_centered(cr, label, cx, cy, radius * 0.85, true, text_on(color_index));
    }
}

// Une gemme en forme de losange
static void draw_gem(cairo_t* cr, int color_index, double cx, double cy, double size)
{
    cairo_arc(cr, cx, cy, size * 1.25, 0, 2 * M_PI);
    cairo_set_source_rgba(cr, 0, 0, 0, color_index == 0 ? 0.55 : 0.3);
    cairo_fill(cr);
    cairo_move_to(cr, cx, cy - size);
    cairo_line_to(cr, cx + size * 0.8, cy);
    cairo_line_to(cr, cx, cy + size);
    cairo_line_to(cr, cx - size * 0.8, cy);
    cairo_close_path(cr);
    set_color(cr, gem_color(color_index));
    cairo_fill_preserve(cr);
    set_color(cr, color_index == 0 ? Rgb{0.5, 0.5, 0.5} : Rgb{1, 1, 1}, 0.8);
    cairo_set_line_width(cr, 1.5);
    cairo_stroke(cr);
}

// ---------------------------------------------------------------- Éléments du jeu

// Contour doré d'une carte achetable, avec un halo pour bien le voir
static void draw_buyable_outline(cairo_t* cr, Rect r, double expand)
{
    rounded_rect(cr, {r.x - expand - 2, r.y - expand - 2, r.w + 2 * expand + 4, r.h + 2 * expand + 4}, 10 + expand);
    set_color(cr, BUYABLE, 0.35);
    cairo_set_line_width(cr, 6);
    cairo_stroke(cr);
    draw_outline(cr, r, 8, BUYABLE, 3, expand);
}

// Une carte : bandeau de sa couleur avec les points et la gemme, coûts en bas à gauche
static void draw_card(cairo_t* cr, const Card& card, Rect r, bool selected, bool buyable)
{
    int color = index_of(card.provides);
    draw_shadow(cr, r, 8);

    rounded_rect(cr, r, 8);
    set_color(cr, mix(gem_color(color), {1, 1, 1}, color == 0 ? 0.2 : 0.7));
    cairo_fill(cr);

    // Bandeau du haut
    cairo_save(cr);
    rounded_rect(cr, r, 8);
    cairo_clip(cr);
    cairo_rectangle(cr, r.x, r.y, r.w, 36);
    set_color(cr, color == 0 ? Rgb{1, 1, 1} : gem_color(color), 0.95);
    cairo_fill(cr);
    cairo_restore(cr);

    if (card.points > 0) {
        draw_text(cr, std::to_string(card.points), r.x + 10, r.y + 28, 26, true, text_on(color));
    }
    draw_gem(cr, color, r.x + r.w - 20, r.y + 18, 12);

    // Coûts empilés depuis le bas
    int line = 0;
    for (int c = COLOR_COUNT - 1; c >= 0; c--) {
        if (card.cost[c] > 0) {
            double cy = r.y + r.h - 18 - line * 26;
            draw_token(cr, c, r.x + 19, cy, 12, std::to_string(card.cost[c]), false);
            line++;
        }
    }

    rounded_rect(cr, r, 8);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.4);
    cairo_set_line_width(cr, 1);
    cairo_stroke(cr);

    if (selected) {
        draw_outline(cr, r, 8, SELECTED, 4);
    } else if (buyable) {
        draw_buyable_outline(cr, r, 2);
    }
}

// Dos de carte : la pioche d'un niveau
static void draw_deck(cairo_t* cr, int level, int count, bool selected)
{
    Rect r = deck_rect(level);
    const Rgb backs[] = {{0.16, 0.50, 0.28}, {0.80, 0.60, 0.12}, {0.18, 0.32, 0.68}};
    const char* roman[] = {"I", "II", "III"};

    if (count == 0) {
        double dashes[] = {6, 4};
        cairo_set_dash(cr, dashes, 2, 0);
        rounded_rect(cr, r, 8);
        cairo_set_source_rgba(cr, 1, 1, 1, 0.3);
        cairo_set_line_width(cr, 2);
        cairo_stroke(cr);
        cairo_set_dash(cr, nullptr, 0, 0);
        draw_text_centered(cr, "vide", r.x + r.w / 2, r.y + r.h / 2, 14, false, {0.8, 0.8, 0.8});
        return;
    }

    // Petit décalage pour donner l'impression d'une pile
    for (int i = 2; i >= 0; i--) {
        Rect layer = {r.x + i * 2, r.y - i * 2, r.w, r.h};
        if (i == 0) {
            draw_shadow(cr, layer, 8);
        }
        rounded_rect(cr, layer, 8);
        set_color(cr, mix(backs[level - 1], {0, 0, 0}, 0.15 * i));
        cairo_fill(cr);
    }
    rounded_rect(cr, {r.x + 7, r.y + 7, r.w - 14, r.h - 14}, 5);
    cairo_set_source_rgba(cr, 1, 1, 1, 0.6);
    cairo_set_line_width(cr, 1.5);
    cairo_stroke(cr);

    draw_text_centered(cr, roman[level - 1], r.x + r.w / 2, r.y + 58, 30, true, WHITE_TEXT);
    draw_text_centered(cr, std::to_string(count), r.x + r.w / 2, r.y + 98, 18, true, WHITE_TEXT);
    draw_text_centered(cr, "cartes", r.x + r.w / 2, r.y + 117, 11, false, WHITE_TEXT);

    if (selected) {
        draw_outline(cr, r, 8, SELECTED, 4);
    }
}

// Tuile noble : nom en haut, bonus demandés à gauche, points à droite
static void draw_noble(cairo_t* cr, const Noble& noble, Rect r, bool highlighted)
{
    draw_shadow(cr, r, 6);
    rounded_rect(cr, r, 6);
    set_color(cr, {0.93, 0.86, 0.70});
    cairo_fill(cr);
    rounded_rect(cr, {r.x + 4, r.y + 4, r.w - 8, r.h - 8}, 4);
    set_color(cr, {0.62, 0.48, 0.28});
    cairo_set_line_width(cr, 1.5);
    cairo_stroke(cr);

    // Le nom est réduit jusqu'à tenir dans la tuile
    double size = 11;
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_text_extents_t extents;
    do {
        size -= 0.5;
        cairo_set_font_size(cr, size);
        cairo_text_extents(cr, noble.name.c_str(), &extents);
    } while (extents.x_advance > r.w - 12 && size > 6);
    draw_text(cr, noble.name, r.x + r.w / 2, r.y + 18, size, true, {0.30, 0.20, 0.10}, Align::Center);

    int line = 0;
    for (int c = 0; c < COLOR_COUNT; c++) {
        if (noble.cost[c] > 0) {
            Rect box = {r.x + 10, r.y + 26 + line * 23, 18, 21};
            rounded_rect(cr, box, 3);
            set_color(cr, gem_color(c));
            cairo_fill_preserve(cr);
            cairo_set_source_rgba(cr, 0, 0, 0, 0.4);
            cairo_set_line_width(cr, 1);
            cairo_stroke(cr);
            draw_text_centered(cr, std::to_string(noble.cost[c]), box.x + box.w / 2, box.y + box.h / 2,
                               13, true, text_on(c));
            line++;
        }
    }
    draw_text_centered(cr, std::to_string(noble.points), r.x + r.w - 26, r.y + 60, 34, true,
                       {0.30, 0.20, 0.10});

    if (highlighted) {
        draw_outline(cr, r, 6, HIGHLIGHT, 4);
    }
}

// ---------------------------------------------------------------- Zones du plateau

static void draw_background(cairo_t* cr)
{
    // Tapis de jeu vert avec un léger dégradé
    cairo_pattern_t* felt = cairo_pattern_create_radial(BOARD_WIDTH / 2, BOARD_HEIGHT / 2, 100,
                                                        BOARD_WIDTH / 2, BOARD_HEIGHT / 2, 900);
    cairo_pattern_add_color_stop_rgb(felt, 0, 0.10, 0.36, 0.24);
    cairo_pattern_add_color_stop_rgb(felt, 1, 0.04, 0.17, 0.11);
    cairo_rectangle(cr, 0, 0, BOARD_WIDTH, BOARD_HEIGHT);
    cairo_set_source(cr, felt);
    cairo_fill(cr);
    cairo_pattern_destroy(felt);
}

static void draw_header(cairo_t* cr, const Game& game)
{
    std::string mode = game.mode() == Mode::Classic ? "classique" : "rapide";
    draw_text(cr, "SPLENDOR", 30, 28, 22, true, HIGHLIGHT);
    draw_text(cr, "Partie " + mode, 30, 50, 12, false, {0.8, 0.9, 0.85});
    draw_text(cr, "Tour " + std::to_string(game.round()), 30, 68, 12, false, {0.8, 0.9, 0.85});
    if (game.is_last_round()) {
        draw_text(cr, "Dernier tour !", 30, 120, 14, true, {1, 0.5, 0.4});
    }
}

static void draw_nobles(cairo_t* cr, const App& app)
{
    const std::vector<Noble>& nobles = app.game->nobles();
    for (int i = 0; i < static_cast<int>(nobles.size()); i++) {
        bool highlighted = false;
        for (int choice : app.noble_choices) {
            if (choice == i && app.phase == Phase::ChooseNoble) {
                highlighted = true;
            }
        }
        draw_noble(cr, nobles[i], noble_rect(i), highlighted);
    }
}

static void draw_cards(cairo_t* cr, const App& app)
{
    const Game& game = *app.game;
    bool my_turn = app.phase == Phase::Action;
    for (int level = 1; level <= LEVEL_COUNT; level++) {
        bool deck_selected = app.target == Target::Deck && app.target_level == level;
        draw_deck(cr, level, game.deck_size(level), deck_selected);
        const std::vector<Card>& row = game.visible(level);
        for (int i = 0; i < static_cast<int>(row.size()); i++) {
            bool selected = app.target == Target::Visible && app.target_level == level && app.target_index == i;
            draw_card(cr, row[i], card_rect(level, i), selected, my_turn && game.can_buy(level, i + 1));
        }
    }
}

static void draw_bank(cairo_t* cr, const App& app)
{
    const Game& game = *app.game;
    draw_text(cr, "Réserve", 668, 150, 14, true, WHITE_TEXT, Align::Center);
    int count = game.mode() == Mode::Classic ? COLOR_COUNT + 1 : COLOR_COUNT;
    for (int c = 0; c < count; c++) {
        double x = 0;
        double y = 0;
        bank_token_center(c, x, y);
        int amount = c == GOLD_INDEX ? game.bank_gold() : game.bank()[c];
        draw_token(cr, c, x, y, BANK_TOKEN_RADIUS, std::to_string(amount), amount == 0);

        // Pastille "+1" / "+2" sur les jetons choisis
        if (c < COLOR_COUNT && app.selection[c] > 0) {
            cairo_arc(cr, x + 22, y - 20, 12, 0, 2 * M_PI);
            set_color(cr, HIGHLIGHT);
            cairo_fill(cr);
            draw_text_centered(cr, "+" + std::to_string(app.selection[c]), x + 22, y - 20, 12, true, DARK_TEXT);
        }
    }
}

static void draw_players(cairo_t* cr, const App& app)
{
    const Game& game = *app.game;
    bool classic = game.mode() == Mode::Classic;
    for (int p = 0; p < static_cast<int>(game.players().size()); p++) {
        const Player& player = game.players()[p];
        bool current = p == game.current_index() && app.phase != Phase::Over;
        Rect r = player_rect(p);

        Rgb own = player_rgb(app, p);

        rounded_rect(cr, r, 10);
        cairo_set_source_rgba(cr, 0, 0, 0, current ? 0.45 : 0.28);
        cairo_fill(cr);

        // Bande de couleur du joueur, toujours visible sur la gauche du panneau
        cairo_save(cr);
        rounded_rect(cr, r, 10);
        cairo_clip(cr);
        cairo_rectangle(cr, r.x, r.y, 7, r.h);
        set_color(cr, own);
        cairo_fill(cr);
        cairo_restore(cr);

        // Son tour : toute sa main est entourée de sa couleur
        if (current) {
            rounded_rect(cr, r, 10);
            set_color(cr, own);
            cairo_set_line_width(cr, 4);
            cairo_stroke(cr);
        }

        double name_x = r.x + 17;
        if (current) {
            cairo_move_to(cr, r.x + 17, r.y + 14);
            cairo_line_to(cr, r.x + 31, r.y + 23);
            cairo_line_to(cr, r.x + 17, r.y + 32);
            cairo_close_path(cr);
            set_color(cr, own);
            cairo_fill(cr);
            name_x += 22;
        }
        draw_text(cr, player.name, name_x, r.y + 30, 19, true, own);
        draw_text(cr, std::to_string(player.score()), r.x + r.w - 52, r.y + 34, 30, true, WHITE_TEXT, Align::Right);
        draw_text(cr, "pts", r.x + r.w - 15, r.y + 34, 13, false, WHITE_TEXT, Align::Right);

        // Bonus : petites cartes de chaque couleur
        draw_text(cr, "Bonus", r.x + 17, r.y + 72, 12, false, {0.8, 0.85, 0.8});
        Resources bonus = player.bonuses();
        for (int c = 0; c < COLOR_COUNT; c++) {
            Rect box = {r.x + 95 + c * 52, r.y + 48, 30, 38};
            rounded_rect(cr, box, 4);
            set_color(cr, gem_color(c), bonus[c] == 0 ? 0.3 : 1);
            cairo_fill(cr);
            if (bonus[c] > 0) {
                draw_text_centered(cr, std::to_string(bonus[c]), box.x + box.w / 2, box.y + box.h / 2,
                                   16, true, text_on(c));
            }
        }

        // Jetons en main
        draw_text(cr, "Jetons", r.x + 17, r.y + 127, 12, false, {0.8, 0.85, 0.8});
        int token_kinds = classic ? COLOR_COUNT + 1 : COLOR_COUNT;
        for (int c = 0; c < token_kinds; c++) {
            double x = 0;
            double y = 0;
            player_token_center(p, c, x, y);
            int amount = c == GOLD_INDEX ? player.gold : player.tokens[c];
            draw_token(cr, c, x, y, PLAYER_TOKEN_RADIUS, std::to_string(amount), amount == 0);
        }

        std::string info = std::to_string(player.token_count()) + "/10 jetons   "
                           + std::to_string(player.cards.size()) + " cartes   "
                           + std::to_string(player.nobles.size()) + " noble(s)";
        if (classic) {
            info += "   " + std::to_string(player.reserved.size()) + " réservée(s)";
        }
        draw_text(cr, info, r.x + 17, r.y + 167, 12, false, {0.85, 0.9, 0.85});

        if (current && app.phase == Phase::Discard) {
            draw_outline(cr, {r.x + 85, r.y + 98, 52.0 * token_kinds, 48}, 8, {1, 0.45, 0.4}, 2);
        }
    }
}

static void draw_reserved(cairo_t* cr, const App& app)
{
    const Game& game = *app.game;
    if (game.mode() != Mode::Classic || app.phase == Phase::Over) {
        return;
    }
    // Les cartes réservées prennent la couleur du joueur dont c'est le tour
    const Player& me = game.current_player();
    Rgb own = player_rgb(app, game.current_index());
    draw_text(cr, "Réservées", 30, 655, 13, true, WHITE_TEXT);
    draw_text(cr, "de", 30, 673, 13, false, WHITE_TEXT);
    draw_text(cr, me.name, 30, 693, 15, true, own);
    for (int i = 0; i < MAX_RESERVED; i++) {
        Rect r = reserved_rect(i);
        if (i < static_cast<int>(me.reserved.size())) {
            draw_card(cr, me.reserved[i], r, false, false);
            draw_outline(cr, r, 8, own, 4);
            // Sélection ou carte achetable : un second contour, plus large
            if (app.target == Target::Reserved && app.target_index == i) {
                draw_outline(cr, r, 8, SELECTED, 3, 7);
            } else if (app.phase == Phase::Action && game.can_buy_reserved(i + 1)) {
                draw_buyable_outline(cr, r, 7);
            }
        } else {
            double dashes[] = {6, 4};
            cairo_set_dash(cr, dashes, 2, 0);
            rounded_rect(cr, r, 8);
            set_color(cr, own, 0.45);
            cairo_set_line_width(cr, 2);
            cairo_stroke(cr);
            cairo_set_dash(cr, nullptr, 0, 0);
        }
    }
}

static void draw_buttons(cairo_t* cr, const App& app)
{
    for (const Button& button : visible_buttons(app)) {
        rounded_rect(cr, button.rect, 8);
        if (button.enabled) {
            set_color(cr, HIGHLIGHT);
        } else {
            cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 0.6);
        }
        cairo_fill(cr);
        draw_text_centered(cr, button.label, button.rect.x + button.rect.w / 2, button.rect.y + button.rect.h / 2,
                           15, true, button.enabled ? DARK_TEXT : Rgb{0.25, 0.25, 0.25});
    }
}

// Panneau d'action : rappelle ce qui est sélectionné et propose les boutons
static void draw_action_panel(cairo_t* cr, const App& app)
{
    Rect r = {510, 615, 250, 145};
    rounded_rect(cr, r, 10);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.3);
    cairo_fill(cr);

    std::string title;
    if (app.phase == Phase::Discard) {
        title = "Défaussez vos jetons en trop";
    } else if (app.phase == Phase::ChooseNoble) {
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
    draw_text(cr, title, r.x + 12, r.y + 24, 14, true, WHITE_TEXT);

    if (total(app.selection) > 0) {
        int k = 0;
        for (int c = 0; c < COLOR_COUNT; c++) {
            for (int n = 0; n < app.selection[c]; n++) {
                draw_token(cr, c, r.x + 140 + k * 26, r.y + 18, 11, "", false);
                k++;
            }
        }
    } else if (app.phase == Phase::Action && app.target == Target::None) {
        draw_text(cr, "Cliquez des jetons de la réserve,", r.x + 12, r.y + 50, 12, false, {0.85, 0.9, 0.85});
        draw_text(cr, "une carte ou une pioche.", r.x + 12, r.y + 68, 12, false, {0.85, 0.9, 0.85});
        draw_text(cr, "Contour doré = carte achetable", r.x + 12, r.y + 96, 12, false, BUYABLE);
    }
    draw_buttons(cr, app);
}

static void draw_status(cairo_t* cr, const App& app)
{
    if (app.message.empty()) {
        return;
    }
    Rgb color = app.message_is_error ? Rgb{1, 0.55, 0.5} : Rgb{0.9, 0.95, 0.9};
    draw_text(cr, app.message, 30, 785, 14, app.message_is_error, color);
}

static void draw_results(cairo_t* cr, const App& app)
{
    const Game& game = *app.game;
    cairo_rectangle(cr, 0, 0, BOARD_WIDTH, BOARD_HEIGHT);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.65);
    cairo_fill(cr);

    Rect r = {390, 200, 500, 390};
    rounded_rect(cr, r, 16);
    set_color(cr, {0.12, 0.30, 0.21});
    cairo_fill_preserve(cr);
    set_color(cr, HIGHLIGHT);
    cairo_set_line_width(cr, 3);
    cairo_stroke(cr);

    draw_text(cr, "Fin de partie", 640, 250, 30, true, HIGHLIGHT, Align::Center);
    std::vector<int> winners = game.winners();
    std::string headline;
    if (game.is_blocked()) {
        draw_text(cr, "(plus aucun joueur ne pouvait jouer)", 640, 275, 12, false, WHITE_TEXT, Align::Center);
    }
    if (winners.size() == 1) {
        headline = "Victoire de " + game.players()[winners[0]].name + " !";
    } else {
        headline = "Égalité parfaite !";
    }
    draw_text(cr, headline, 640, 310, 22, true, WHITE_TEXT, Align::Center);

    for (int i = 0; i < static_cast<int>(game.players().size()); i++) {
        const Player& player = game.players()[i];
        double y = 355 + i * 34;
        draw_text(cr, player.name, 440, y, 17, true, player_rgb(app, i));
        draw_text(cr, std::to_string(player.score()) + " pts", 720, y, 17, true, HIGHLIGHT, Align::Right);
        draw_text(cr, std::to_string(player.cards.size()) + " cartes", 840, y, 14, false, WHITE_TEXT, Align::Right);
    }
    draw_buttons(cr, app);
}

// ---------------------------------------------------------------- Dessin complet

void draw_board(cairo_t* cr, App& app, double width, double height)
{
    // Mise à l'échelle : le plateau garde ses proportions et reste centré
    app.scale = std::fmin(width / BOARD_WIDTH, height / BOARD_HEIGHT);
    app.offset_x = (width - BOARD_WIDTH * app.scale) / 2;
    app.offset_y = (height - BOARD_HEIGHT * app.scale) / 2;

    cairo_set_source_rgb(cr, 0.03, 0.12, 0.08);
    cairo_paint(cr);
    cairo_save(cr);
    cairo_translate(cr, app.offset_x, app.offset_y);
    cairo_scale(cr, app.scale, app.scale);

    draw_background(cr);
    if (app.game) {
        draw_header(cr, *app.game);
        draw_text(cr, "Nobles", 30, 100, 14, true, WHITE_TEXT);
        draw_nobles(cr, app);
        draw_cards(cr, app);
        draw_bank(cr, app);
        draw_players(cr, app);
        draw_reserved(cr, app);
        if (app.phase == Phase::Over) {
            draw_results(cr, app);
        } else {
            draw_action_panel(cr, app);
            draw_status(cr, app);
        }
    }
    cairo_restore(cr);
}
