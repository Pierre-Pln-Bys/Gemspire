#ifndef SPLENDOR_GUI_HPP
#define SPLENDOR_GUI_HPP

#include <memory>
#include <string>
#include <vector>

#include <cairo.h>

#include "game.hpp"
#include "types.hpp"

// Interface graphique (GTK 3 + Cairo).
// Le plateau est dessiné dans un repère virtuel de BOARD_WIDTH x BOARD_HEIGHT,
// mis à l'échelle de la fenêtre. Les mêmes fonctions de placement (rectangles)
// servent au dessin et à la détection des clics.

const double BOARD_WIDTH = 1280;
const double BOARD_HEIGHT = 800;
const int GOLD_INDEX = COLOR_COUNT;  // indice de l'or à côté des 5 couleurs

// Étape du tour en cours
enum class Phase {
    Action,       // le joueur choisit son action
    Discard,      // il doit défausser des jetons (plus de 10)
    ChooseNoble,  // plusieurs nobles peuvent venir : il en choisit un
    Over          // partie terminée
};

// Ce que le joueur a sélectionné à la souris
enum class Target { None, Visible, Deck, Reserved };

enum class ButtonId { Take, Buy, Reserve, Cancel, Replay, Quit };

struct Rect {
    double x, y, w, h;
    bool contains(double px, double py) const
    {
        return px >= x && px <= x + w && py >= y && py <= y + h;
    }
};

struct Button {
    ButtonId id;
    std::string label;
    bool enabled;
    Rect rect;
};

// Tout l'état de l'interface (passé aux fonctions de rappel GTK par pointeur)
struct App {
    std::unique_ptr<Game> game;
    Phase phase = Phase::Action;
    Resources selection = {0, 0, 0, 0, 0};  // jetons choisis dans la réserve
    Target target = Target::None;
    int target_level = 0;                   // niveau de la carte / pioche choisie
    int target_index = 0;                   // position (à partir de 0)
    std::vector<int> noble_choices;         // nobles possibles (phase ChooseNoble)
    std::string message;                    // ligne d'état en bas de l'écran
    bool message_is_error = false;

    // Mise à l'échelle calculée au dernier dessin (pour convertir les clics)
    double scale = 1;
    double offset_x = 0;
    double offset_y = 0;
};

// --- Placement des éléments (repère virtuel) ---
Rect noble_rect(int index);
Rect deck_rect(int level);
Rect card_rect(int level, int index);
Rect reserved_rect(int index);
Rect player_rect(int player);
void bank_token_center(int color_index, double& x, double& y);
void player_token_center(int player, int color_index, double& x, double& y);
const double BANK_TOKEN_RADIUS = 28;
const double PLAYER_TOKEN_RADIUS = 18;

// Boutons visibles selon l'état
std::vector<Button> visible_buttons(const App& app);

// Dessine tout le plateau sur une surface de `width` x `height` pixels
// (met à jour app.scale / offset_x / offset_y).
void draw_board(cairo_t* cr, App& app, double width, double height);

#endif
