// Interface graphique de Splendor (GTK 3 + Cairo).
// Ce fichier gère la fenêtre, la boîte de dialogue de nouvelle partie, les
// clics et l'enchaînement des tours. Le dessin est dans gui_draw.cpp.
#include <gtk/gtk.h>

#include <cmath>
#include <random>
#include <string>
#include <vector>

#include "ai.hpp"
#include "display.hpp"
#include "gui.hpp"
#include "rules.hpp"
#include "save.hpp"
#include "version.hpp"

// Widgets de la boîte de dialogue « Nouvelle partie »
struct SetupWidgets {
    GtkWidget* spin;
    GtkWidget* entries[4];
    GtkWidget* colors[4];   // choix de la couleur de chaque joueur
    GtkWidget* players[4];  // humain ou ordinateur
};

// Active seulement les champs des joueurs présents
static void on_player_count_changed(GtkSpinButton* spin, gpointer data)
{
    SetupWidgets* widgets = static_cast<SetupWidgets*>(data);
    int count = gtk_spin_button_get_value_as_int(spin);
    for (int i = 0; i < 4; i++) {
        gtk_widget_set_sensitive(widgets->entries[i], i < count);
        gtk_widget_set_sensitive(widgets->colors[i], i < count);
        gtk_widget_set_sensitive(widgets->players[i], i < count);
    }
}

// Demande le mode, le nombre de joueurs, leurs noms et leurs couleurs, puis
// crée la partie. Retourne false si le joueur annule.
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
    gtk_grid_attach(GTK_GRID(grid), quick, 1, 0, 3, 1);
    gtk_grid_attach(GTK_GRID(grid), classic, 1, 1, 3, 1);

    SetupWidgets widgets;
    widgets.spin = gtk_spin_button_new_with_range(2, 4, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Joueurs :"), 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widgets.spin, 1, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Couleur"), 2, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Joué par"), 3, 2, 1, 1);
    for (int i = 0; i < 4; i++) {
        widgets.entries[i] = gtk_entry_new();
        gtk_entry_set_text(GTK_ENTRY(widgets.entries[i]), ("Joueur " + std::to_string(i + 1)).c_str());
        gtk_entry_set_activates_default(GTK_ENTRY(widgets.entries[i]), TRUE);
        std::string label = "Nom " + std::to_string(i + 1) + " :";
        gtk_grid_attach(GTK_GRID(grid), gtk_label_new(label.c_str()), 0, 3 + i, 1, 1);
        gtk_grid_attach(GTK_GRID(grid), widgets.entries[i], 1, 3 + i, 1, 1);

        // Par défaut, chaque joueur a une couleur différente
        widgets.colors[i] = gtk_combo_box_text_new();
        for (int c = 0; c < PLAYER_COLOR_COUNT; c++) {
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widgets.colors[i]), PLAYER_COLORS[c].name);
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(widgets.colors[i]), i);
        gtk_grid_attach(GTK_GRID(grid), widgets.colors[i], 2, 3 + i, 1, 1);

        // Joueur 1 humain, les autres aussi par défaut
        widgets.players[i] = gtk_combo_box_text_new();
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widgets.players[i]), "Humain");
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widgets.players[i]), "Ordinateur facile");
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widgets.players[i]), "Ordinateur moyen");
        gtk_combo_box_set_active(GTK_COMBO_BOX(widgets.players[i]), 0);
        gtk_grid_attach(GTK_GRID(grid), widgets.players[i], 3, 3 + i, 1, 1);
    }
    GtkWidget* error_label = gtk_label_new("");
    gtk_grid_attach(GTK_GRID(grid), error_label, 0, 7, 4, 1);
    g_signal_connect(widgets.spin, "value-changed", G_CALLBACK(on_player_count_changed), &widgets);
    on_player_count_changed(GTK_SPIN_BUTTON(widgets.spin), &widgets);

    gtk_widget_show_all(dialog);
    bool accepted = false;
    while (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
        int count = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widgets.spin));
        std::vector<int> colors;
        bool duplicate = false;
        for (int i = 0; i < count; i++) {
            int color = gtk_combo_box_get_active(GTK_COMBO_BOX(widgets.colors[i]));
            for (int other : colors) {
                if (other == color) {
                    duplicate = true;
                }
            }
            colors.push_back(color);
        }
        if (duplicate) {
            gtk_label_set_markup(GTK_LABEL(error_label),
                                 "<span foreground=\"#d03030\">Chaque joueur doit avoir une couleur différente.</span>");
            continue;
        }

        Mode mode = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(classic)) ? Mode::Classic : Mode::Quick;
        std::vector<std::string> names;
        for (int i = 0; i < count; i++) {
            std::string name = gtk_entry_get_text(GTK_ENTRY(widgets.entries[i]));
            names.push_back(name.empty() ? "Joueur " + std::to_string(i + 1) : name);
        }
        app.game.reset(new Game(names, std::random_device{}(), mode));
        app.player_colors = colors;
        const Controller kinds[] = {Controller::Human, Controller::AiEasy, Controller::AiMedium};
        for (int i = 0; i < count; i++) {
            app.game->set_controller(i, kinds[gtk_combo_box_get_active(GTK_COMBO_BOX(widgets.players[i]))]);
        }
        app.shown_events = 0;
        app.selection = {0, 0, 0, 0, 0};
        app.target = Target::None;
        app.message = "À " + app.game->current_player().name + " de jouer.";
        app.message_is_error = false;
        accepted = true;
        break;
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

// Met dans la ligne d'état ce qui vient de se passer (événements de Game),
// puis ce que le joueur doit faire maintenant.
static void show_new_events(App& app)
{
    const Game& game = *app.game;
    std::string text;
    for (; app.shown_events < static_cast<int>(game.events().size()); app.shown_events++) {
        text += describe_event(game, game.events()[app.shown_events]) + " ";
    }
    if (game.step() == TurnStep::Discard) {
        text += "Plus de 10 jetons : cliquez sur vos jetons pour en défausser "
                + std::to_string(game.tokens_to_discard()) + ".";
    } else if (game.step() == TurnStep::ChooseNoble) {
        text += "Plusieurs nobles veulent venir : cliquez sur celui de votre choix.";
    } else if (game.step() == TurnStep::Action) {
        text += "À " + game.current_player().name + " de jouer.";
    }
    set_message(app, text, false);
}

// ---------------------------------------------------------------- Ordinateur

// Un coup de l'ordinateur ; GTK rappelle cette fonction tant qu'il doit jouer
static gboolean on_ai_timer(gpointer data)
{
    App& app = *static_cast<App*>(data);
    if (!app.game || !ai_to_play(*app.game)) {
        app.ai_timer_pending = false;
        return G_SOURCE_REMOVE;
    }
    std::string error = ai_play(*app.game);
    if (error.empty()) {
        show_new_events(app);
    } else {
        set_message(app, "Erreur de l'ordinateur : " + error, true);
    }
    if (app.area != nullptr) {
        gtk_widget_queue_draw(app.area);
    }
    if (!error.empty() || !ai_to_play(*app.game)) {
        app.ai_timer_pending = false;
        return G_SOURCE_REMOVE;
    }
    return G_SOURCE_CONTINUE;
}

// Si c'est à l'ordinateur de jouer, programme ses coups (un toutes les 0,7 s)
static void schedule_ai(App& app)
{
    if (app.game && ai_to_play(*app.game) && !app.ai_timer_pending) {
        app.ai_timer_pending = true;
        g_timeout_add(700, on_ai_timer, &app);
    }
}

// ---------------------------------------------------------------- Boutons

static void press_button(App& app, ButtonId id, GtkWidget* window)
{
    Game& game = *app.game;
    std::string error;

    switch (id) {
    case ButtonId::Take:
        error = game.take_tokens(app.selection);
        break;
    case ButtonId::Buy:
        if (app.target == Target::Visible) {
            error = game.buy_card(app.target_level, app.target_index + 1);
        } else if (app.target == Target::Reserved) {
            error = game.buy_reserved(app.target_index + 1);
        }
        break;
    case ButtonId::Reserve:
        error = game.reserve_card(app.target_level, app.target == Target::Deck ? 0 : app.target_index + 1);
        break;
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
    } else if (id == ButtonId::Take || id == ButtonId::Buy || id == ButtonId::Reserve) {
        clear_selection(app);
        show_new_events(app);
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
        } else {
            show_new_events(app);
        }
        return;
    }
}

static void click_noble_phase(App& app, double x, double y)
{
    for (int choice : app.game->noble_choices()) {
        if (noble_rect(choice).contains(x, y)) {
            app.game->choose_noble(choice);
            show_new_events(app);
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
    // Pendant le tour de l'ordinateur, on ne peut pas cliquer sur le plateau
    if (ai_to_play(*app.game)) {
        return TRUE;
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
        TurnStep step = app.game->step();
        if (step == TurnStep::Action) {
            click_action_phase(app, x, y);
        } else if (step == TurnStep::Discard) {
            click_discard_phase(app, x, y);
        } else if (step == TurnStep::ChooseNoble) {
            click_noble_phase(app, x, y);
        }
    }
    if (app.game) {
        gtk_widget_queue_draw(area);
        schedule_ai(app);
    }
    return TRUE;
}

// ---------------------------------------------------------------- Sauvegarde

static void on_save_clicked(GtkWidget* widget, gpointer data)
{
    App& app = *static_cast<App*>(data);
    if (!app.game) {
        return;
    }
    GtkWidget* dialog = gtk_file_chooser_dialog_new("Sauvegarder la partie",
                                                    GTK_WINDOW(gtk_widget_get_toplevel(widget)),
                                                    GTK_FILE_CHOOSER_ACTION_SAVE,
                                                    "_Annuler", GTK_RESPONSE_CANCEL,
                                                    "_Sauvegarder", GTK_RESPONSE_ACCEPT, nullptr);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), "partie.splendor");
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char* path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        std::string error = save_game_to_file(*app.game, path);
        if (error.empty()) {
            set_message(app, std::string("Partie sauvegardée dans ") + path + ".", false);
        } else {
            set_message(app, error, true);
        }
        g_free(path);
    }
    gtk_widget_destroy(dialog);
    gtk_widget_queue_draw(app.area);
}

static void on_open_clicked(GtkWidget* widget, gpointer data)
{
    App& app = *static_cast<App*>(data);
    GtkWidget* dialog = gtk_file_chooser_dialog_new("Ouvrir une partie",
                                                    GTK_WINDOW(gtk_widget_get_toplevel(widget)),
                                                    GTK_FILE_CHOOSER_ACTION_OPEN,
                                                    "_Annuler", GTK_RESPONSE_CANCEL,
                                                    "_Ouvrir", GTK_RESPONSE_ACCEPT, nullptr);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char* path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        std::unique_ptr<Game> loaded;
        std::string error = load_game_from_file(path, loaded);
        if (error.empty()) {
            app.game = std::move(loaded);
            // Les couleurs des joueurs ne sont pas dans la sauvegarde : couleurs par défaut
            app.player_colors.clear();
            for (int i = 0; i < static_cast<int>(app.game->players().size()); i++) {
                app.player_colors.push_back(i);
            }
            app.shown_events = 0;
            clear_selection(app);
            set_message(app, std::string("Partie chargée. À ") + app.game->current_player().name + " de jouer.", false);
            schedule_ai(app);
        } else {
            set_message(app, error, true);
        }
        g_free(path);
    }
    gtk_widget_destroy(dialog);
    gtk_widget_queue_draw(app.area);
}

static gboolean on_key(GtkWidget* window, GdkEventKey* event, gpointer data)
{
    App& app = *static_cast<App*>(data);
    if ((event->state & GDK_CONTROL_MASK) && event->keyval == GDK_KEY_s) {
        on_save_clicked(window, data);
        return TRUE;
    }
    if ((event->state & GDK_CONTROL_MASK) && event->keyval == GDK_KEY_o) {
        on_open_clicked(window, data);
        return TRUE;
    }
    if (event->keyval == GDK_KEY_Escape && app.game && app.game->step() == TurnStep::Action) {
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

    // Barre de titre avec les boutons de sauvegarde
    GtkWidget* bar = gtk_header_bar_new();
    gtk_header_bar_set_title(GTK_HEADER_BAR(bar), title.c_str());
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(bar), TRUE);
    GtkWidget* open_button = gtk_button_new_with_label("Ouvrir…");
    GtkWidget* save_button = gtk_button_new_with_label("Sauvegarder");
    gtk_widget_set_tooltip_text(open_button, "Ouvrir une partie sauvegardée (Ctrl+O)");
    gtk_widget_set_tooltip_text(save_button, "Sauvegarder la partie (Ctrl+S)");
    gtk_header_bar_pack_start(GTK_HEADER_BAR(bar), open_button);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(bar), save_button);
    gtk_window_set_titlebar(GTK_WINDOW(window), bar);
    g_signal_connect(open_button, "clicked", G_CALLBACK(on_open_clicked), &app);
    g_signal_connect(save_button, "clicked", G_CALLBACK(on_save_clicked), &app);

    GtkWidget* area = gtk_drawing_area_new();
    gtk_widget_add_events(area, GDK_BUTTON_PRESS_MASK);
    gtk_container_add(GTK_CONTAINER(window), area);

    g_signal_connect(area, "draw", G_CALLBACK(on_draw), &app);
    g_signal_connect(area, "button-press-event", G_CALLBACK(on_click), &app);
    g_signal_connect(window, "key-press-event", G_CALLBACK(on_key), &app);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), nullptr);

    app.area = area;
    gtk_widget_show_all(window);
    schedule_ai(app);
    gtk_main();
    return 0;
}
