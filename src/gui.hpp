#ifndef SPLENDOR_GUI_HPP
#define SPLENDOR_GUI_HPP

#include <memory>
#include <string>
#include <vector>

#include "raylib.h"

#include "game.hpp"
#include "types.hpp"

// Interface graphique (raylib).
// Tout est dessiné dans un repère virtuel de BOARD_WIDTH x BOARD_HEIGHT, mis
// à l'échelle de la fenêtre. Les mêmes fonctions de placement (rectangles)
// servent au dessin et à la détection des clics.

const float BOARD_WIDTH = 1280;
const float BOARD_HEIGHT = 800;
const int GOLD_INDEX = COLOR_COUNT;  // indice de l'or à côté des 5 gemmes
const char* const SAVE_FILE = "partie.splendor";

enum class Screen { Home, Setup, Board };

// Ce que le joueur a sélectionné à la souris
enum class Target { None, Visible, Deck, Reserved };

// Tous les éléments cliquables. `value` précise lequel (joueur, couleur...).
enum class ButtonId {
    // Accueil
    NewGame, Resume, Quit,
    // Nouvelle partie
    Mode, Count, NameBox, Swatch, Kind, Start, Back,
    // Plateau
    Take, Buy, Reserve, Cancel, Save, Journal, Menu,
    // Fin de partie
    Replay
};

struct Button {
    ButtonId id;
    std::string label;
    bool enabled;
    Rectangle rect;
    int value;
    bool selected;   // bouton « allumé » (choix courant)
};

// Couleurs que les joueurs peuvent choisir (différentes de celles des gemmes)
struct PlayerColor {
    const char* name;
    Color color;
};
const int PLAYER_COLOR_COUNT = 5;
const PlayerColor PLAYER_COLORS[PLAYER_COLOR_COUNT] = {
    {"Violet", {168, 97, 235, 255}},
    {"Orange", {255, 140, 26, 255}},
    {"Turquoise", {51, 217, 204, 255}},
    {"Rose", {255, 115, 184, 255}},
    {"Marron", {158, 102, 56, 255}},
};

// Réglages de l'écran « Nouvelle partie »
struct Setup {
    Mode mode = Mode::Classic;
    int count = 2;
    std::string names[4] = {"Joueur 1", "Joueur 2", "Joueur 3", "Joueur 4"};
    int colors[4] = {0, 1, 2, 3};
    Controller kinds[4] = {Controller::Human, Controller::AiMedium, Controller::AiMedium, Controller::AiMedium};
    int focused_name = -1;   // champ de nom en cours de saisie (-1 : aucun)
};

// Une petite animation : jeton qui glisse, ou carte qui part vers un joueur
struct Animation {
    double start;
    double duration;
    Vector2 from;
    Vector2 to;
    int gem;         // jeton : indice de gemme (GOLD_INDEX pour l'or) ; -1 pour une carte
    Card card;
};

// Une carte qui vient d'apparaître sur le plateau (elle se retourne)
struct Flip {
    double start;
    int level;
    int index;
};

// Tout l'état de l'interface
struct App {
    Screen screen = Screen::Home;
    Setup setup;
    std::unique_ptr<Game> game;
    std::vector<int> player_colors;         // couleur de chaque joueur (indice dans PLAYER_COLORS)
    Resources selection = {0, 0, 0, 0, 0};  // jetons choisis dans la réserve
    Target target = Target::None;
    int target_level = 0;
    int target_index = 0;                   // position, à partir de 0
    int shown_events = 0;                   // événements de Game déjà affichés
    std::string message;                    // ligne d'état
    bool message_is_error = false;
    bool journal_open = false;
    bool curtain = false;                   // écran de passage entre deux joueurs
    int last_player = 0;                    // pour détecter le changement de joueur
    double ai_next_time = 0;                // heure du prochain coup de l'ordinateur
    std::vector<Animation> animations;
    std::vector<Flip> flips;
    bool quit = false;

    Font font = {};
    Font bold = {};
};

// --- Placement des éléments du plateau (repère virtuel) ---
Rectangle noble_rect(int index);
Rectangle deck_rect(int level);
Rectangle card_rect(int level, int index);
Rectangle reserved_rect(int index);
Rectangle player_rect(int player);
Vector2 bank_token_center(int gem);
Vector2 player_token_center(int player, int gem);
const float BANK_TOKEN_RADIUS = 28;
const float PLAYER_TOKEN_RADIUS = 18;

bool point_in(Vector2 point, Rectangle rect);

// Boutons visibles selon l'écran et l'état
std::vector<Button> visible_buttons(const App& app);

// Dessine l'écran courant, dans le repère virtuel
void draw_app(const App& app, Vector2 mouse);

// « Il manque 2 Rouge, 1 Bleu » pour une carte, ou "" si elle est achetable
std::string missing_text(const Game& game, const Card& card);

#endif
