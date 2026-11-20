// Interface graphique de Splendor (GTK 3 + Cairo).
// Ce fichier gère la fenêtre, la boîte de dialogue de nouvelle partie, les
// clics et l'enchaînement des tours. Le dessin est dans gui_draw.cpp.
#include <gtk/gtk.h>

#include <cmath>
#include <random>
#include <string>
#include <vector>

#include "display.hpp"
#include "gui.hpp"
#include "rules.hpp"
#include "version.hpp"

// Widgets de la boîte de dialogue « Nouvelle partie »
struct SetupWidgets {
    GtkWidget* spin;
    GtkWidget* entries[4];
};

// Active seulement les champs de nom utiles
static void on_player_count_changed(GtkSpinButton* spin, gpointer data)
{
    SetupWidgets* widgets = static_cast<SetupWidgets*>(data);
    int count = gtk_spin_button_get_value_as_int(spin);
    for (int i = 0; i < 4; i++) {
        gtk_widget_set_sensitive(widgets->entries[i], i < count);
    }
}

// Demande le mode, le nombre de joueurs et les noms, puis crée la partie.
// Retourne false si le joueur annule.
static bool run_setup_dialog(App& app, GtkWindow* parent)
{
    GtkWidget* dialog = gtk_dialog_new_with_buttons("Nouvelle partie de Splendor", parent, GTK_DIALOG_MODAL,
                                                    "_Quitter", GTK_RESPONSE_CANCEL,
                                                    "_Jouer", GTK_RESPONSE_OK, nullptr);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 16);
    gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), grid);

    GtkWidget* quick = gtk_radio_button_new_with_label(nullptr, "Partie rapide (sans or ni réservation)");
    GtkWidget* classic = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(quick),
                                                                     "Partie classique (avec or et réservation)");
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Mode :"), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), quick, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), classic, 1, 1, 1, 1);

    SetupWidgets widgets;
    widgets.spin = gtk_spin_button_new_with_range(2, 4, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Joueurs :"), 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widgets.spin, 1, 2, 1, 1);
    for (int i = 0; i < 4; i++) {
        widgets.entries[i] = gtk_entry_new();
        gtk_entry_set_text(GTK_ENTRY(widgets.entries[i]), ("Joueur " + std::to_string(i + 1)).c_str());
        gtk_entry_set_activates_default(GTK_ENTRY(widgets.entries[i]), TRUE);
        std::string label = "Nom " + std::to_string(i + 1) + " :";
        gtk_grid_attach(GTK_GRID(grid), gtk_label_new(label.c_str()), 0, 3 + i, 1, 1);
        gtk_grid_attach(GTK_GRID(grid), widgets.entries[i], 1, 3 + i, 1, 1);
    }
    g_signal_connect(widgets.spin, "value-changed", G_CALLBACK(on_player_count_changed), &widgets);
    on_player_count_changed(GTK_SPIN_BUTTON(widgets.spin), &widgets);

    gtk_widget_show_all(dialog);
    bool accepted = gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK;

    if (accepted) {
        Mode mode = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(classic)) ? Mode::Classic : Mode::Quick;
        int count = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widgets.spin));
        std::vector<std::string> names;
        for (int i = 0; i < count; i++) {
            std::string name = gtk_entry_get_text(GTK_ENTRY(widgets.entries[i]));
            names.push_back(name.empty() ? "Joueur " + std::to_string(i + 1) : name);
        }
        app.game.reset(new Game(names, std::random_device{}(), mode));
        app.phase = Phase::Action;
        app.selection = {0, 0, 0, 0, 0};
        app.target = Target::None;
        app.noble_choices.clear();
        app.message = "À " + app.game->current_player().name + " de jouer.";
        app.message_is_error = false;
    }
    gtk_widget_destroy(dialog);
    return accepted;
}

// ---------------------------------------------------------------- Déroulement du tour

static void clear_selection(App& app)
{
    app.selection = {0, 0, 0, 0, 0};
    app.target = Target::None;
}

static void set_message(App& app, const std::string& message, bool is_error)
{
    app.message = message;
    app.message_is_error = is_error;
}

// Passe au joueur suivant ; ceux qui ne peuvent rien faire passent leur tour
static void next_turn(App& app)
{
    Game& game = *app.game;
    game.end_turn();
    while (!game.is_over() && !game.can_play()) {
        app.message += " " + game.current_player().name + " ne peut rien faire et passe.";
        game.pass_turn();
        game.end_turn();
    }
    if (game.is_over()) {
        app.phase = Phase::Over;
    } else {
        app.phase = Phase::Action;
        app.message += " À " + game.current_player().name + " de jouer.";
    }
}

static void receive_noble(App& app, int noble_index)
{
    Game& game = *app.game;
    app.message += " " + game.nobles()[noble_index].name + " rend visite à "
                   + game.current_player().name + " (+3 pts) !";
    game.receive_noble(noble_index);
    app.noble_choices.clear();
    next_turn(app);
}

// Après la défausse : visite éventuelle d'un noble, puis fin du tour
static void after_discard(App& app)
{
    std::vector<int> candidates = app.game->visiting_nobles();
    if (candidates.size() == 1) {
        receive_noble(app, candidates[0]);
    } else if (candidates.size() > 1) {
        app.phase = Phase::ChooseNoble;
        app.noble_choices = candidates;
        app.message += " Plusieurs nobles veulent venir : cliquez sur celui de votre choix.";
    } else {
        next_turn(app);
    }
}

// L'action du joueur est faite : défausse si nécessaire, puis la suite
static void finish_action(App& app, const std::string& description)
{
    clear_selection(app);
    set_message(app, description, false);
    if (app.game->must_discard()) {
        app.phase = Phase::Discard;
        int extra = app.game->current_player().token_count() - MAX_TOKENS_IN_HAND;
        app.message += " Plus de 10 jetons : cliquez sur vos jetons pour en défausser "
                       + std::to_string(extra) + ".";
    } else {
        after_discard(app);
    }
}

static std::string card_text(const Card& card)
{
    std::string text = "une carte " + color_name(card.provides);
    if (card.points > 0) {
        text += " (" + std::to_string(card.points) + " pts)";
    }
    return text;
}

// ---------------------------------------------------------------- Boutons

static void press_button(App& app, ButtonId id, GtkWidget* window)
{
    Game& game = *app.game;
    std::string name = game.current_player().name;
    std::string error;

    switch (id) {
    case ButtonId::Take:
        error = game.take_tokens(app.selection);
        if (error.empty()) {
            finish_action(app, name + " prend " + std::to_string(total(app.selection)) + " jeton(s).");
        }
        break;
    case ButtonId::Buy:
        if (app.target == Target::Visible) {
            Card card = game.visible(app.target_level)[app.target_index];
            error = game.buy_card(app.target_level, app.target_index + 1);
            if (error.empty()) {
                finish_action(app, name + " achète " + card_text(card) + ".");
            }
        } else if (app.target == Target::Reserved) {
            Card card = game.current_player().reserved[app.target_index];
            error = game.buy_reserved(app.target_index + 1);
            if (error.empty()) {
                finish_action(app, name + " achète sa carte réservée, " + card_text(card) + ".");
            }
        }
        break;
    case ButtonId::Reserve: {
        int position = app.target == Target::Deck ? 0 : app.target_index + 1;
        int gold_before = game.current_player().gold;
        error = game.reserve_card(app.target_level, position);
        if (error.empty()) {
            std::string text = name + " réserve une carte";
            if (game.current_player().gold > gold_before) {
                text += " et prend un jeton or";
            }
            finish_action(app, text + ".");
        }
        break;
    }
    case ButtonId::Cancel:
        clear_selection(app);
        set_message(app, "", false);
        break;
    case ButtonId::Replay:
        run_setup_dialog(app, GTK_WINDOW(window));
        break;
    case ButtonId::Quit:
        gtk_widget_destroy(window);
        break;
    }
    if (!error.empty()) {
        set_message(app, error, true);
    }
}

// ---------------------------------------------------------------- Clics sur le plateau

static void click_action_phase(App& app, double x, double y)
{
    Game& game = *app.game;

    // Jetons de la réserve : chaque clic ajoute un jeton (0 -> 1 -> 2 -> 0)
    for (int c = 0; c <= GOLD_INDEX; c++) {
        double cx = 0;
        double cy = 0;
        bank_token_center(c, cx, cy);
        if (std::hypot(x - cx, y - cy) > BANK_TOKEN_RADIUS) {
            continue;
        }
        if (c == GOLD_INDEX) {
            if (game.mode() == Mode::Classic) {
                set_message(app, "L'or ne se prend pas : on le reçoit en réservant une carte.", true);
            }
            return;
        }
        if (game.bank()[c] == 0) {
            set_message(app, "Il n'y a plus de jetons " + color_name(color_at(c)) + ".", true);
            return;
        }
        app.target = Target::None;
        app.selection[c] = (app.selection[c] + 1) % 3;
        set_message(app, "", false);
        return;
    }

    // Cartes visibles et pioches
    for (int level = 1; level <= LEVEL_COUNT; level++) {
        for (int i = 0; i < static_cast<int>(game.visible(level).size()); i++) {
            if (card_rect(level, i).contains(x, y)) {
                clear_selection(app);
                app.target = Target::Visible;
                app.target_level = level;
                app.target_index = i;
                set_message(app, "", false);
                return;
            }
        }
        if (deck_rect(level).contains(x, y)) {
            if (game.mode() != Mode::Classic) {
                set_message(app, "La pioche ne sert qu'à réserver (partie classique).", true);
            } else if (game.deck_size(level) > 0) {
                clear_selection(app);
                app.target = Target::Deck;
                app.target_level = level;
                set_message(app, "", false);
            }
            return;
        }
    }

    // Cartes réservées du joueur courant
    for (int i = 0; i < static_cast<int>(game.current_player().reserved.size()); i++) {
        if (reserved_rect(i).contains(x, y)) {
            clear_selection(app);
            app.target = Target::Reserved;
            app.target_index = i;
            set_message(app, "", false);
            return;
        }
    }
}

static void click_discard_phase(App& app, double x, double y)
{
    Game& game = *app.game;
    for (int c = 0; c <= GOLD_INDEX; c++) {
        double cx = 0;
        double cy = 0;
        player_token_center(game.current_index(), c, cx, cy);
        if (std::hypot(x - cx, y - cy) > PLAYER_TOKEN_RADIUS) {
            continue;
        }
        std::string error = c == GOLD_INDEX ? game.discard_gold() : game.discard(color_at(c));
        if (!error.empty()) {
            set_message(app, error, true);
        } else if (!game.must_discard()) {
            set_message(app, "Défausse terminée.", false);
            after_discard(app);
        } else {
            int extra = game.current_player().token_count() - MAX_TOKENS_IN_HAND;
            set_message(app, "Encore " + std::to_string(extra) + " jeton(s) à défausser.", false);
        }
        return;
    }
}

static void click_noble_phase(App& app, double x, double y)
{
    for (int choice : app.noble_choices) {
        if (noble_rect(choice).contains(x, y)) {
            set_message(app, "", false);
            receive_noble(app, choice);
            return;
        }
    }
}

static gboolean on_click(GtkWidget* area, GdkEventButton* event, gpointer data)
{
    App& app = *static_cast<App*>(data);
    if (!app.game || event->button != 1) {
        return FALSE;
    }
    // Coordonnées de la fenêtre -> repère virtuel du plateau
    double x = (event->x - app.offset_x) / app.scale;
    double y = (event->y - app.offset_y) / app.scale;

    bool button_pressed = false;
    for (const Button& button : visible_buttons(app)) {
        if (button.rect.contains(x, y)) {
            button_pressed = true;
            if (button.enabled) {
                press_button(app, button.id, gtk_widget_get_toplevel(area));
            }
            break;
        }
    }
    if (!button_pressed) {
        if (app.phase == Phase::Action) {
            click_action_phase(app, x, y);
        } else if (app.phase == Phase::Discard) {
            click_discard_phase(app, x, y);
        } else if (app.phase == Phase::ChooseNoble) {
            click_noble_phase(app, x, y);
        }
    }
    if (app.game) {
        gtk_widget_queue_draw(area);
    }
    return TRUE;
}

static gboolean on_key(GtkWidget* window, GdkEventKey* event, gpointer data)
{
    App& app = *static_cast<App*>(data);
    if (event->keyval == GDK_KEY_Escape && app.phase == Phase::Action) {
        clear_selection(app);
        gtk_widget_queue_draw(window);
        return TRUE;
    }
    return FALSE;
}

static gboolean on_draw(GtkWidget* area, cairo_t* cr, gpointer data)
{
    App& app = *static_cast<App*>(data);
    draw_board(cr, app, gtk_widget_get_allocated_width(area), gtk_widget_get_allocated_height(area));
    return FALSE;
}

int main(int argc, char* argv[])
{
    gtk_init(&argc, &argv);

    App app;
    if (!run_setup_dialog(app, nullptr)) {
        return 0;
    }

    GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    std::string title = std::string("Splendor ") + SPLENDOR_VERSION;
    gtk_window_set_title(GTK_WINDOW(window), title.c_str());
    gtk_window_set_default_size(GTK_WINDOW(window), 1280, 800);

    GtkWidget* area = gtk_drawing_area_new();
    gtk_widget_add_events(area, GDK_BUTTON_PRESS_MASK);
    gtk_container_add(GTK_CONTAINER(window), area);

    g_signal_connect(area, "draw", G_CALLBACK(on_draw), &app);
    g_signal_connect(area, "button-press-event", G_CALLBACK(on_click), &app);
    g_signal_connect(window, "key-press-event", G_CALLBACK(on_key), &app);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), nullptr);

    gtk_widget_show_all(window);
    gtk_main();
    return 0;
}
