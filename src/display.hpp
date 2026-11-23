#ifndef SPLENDOR_DISPLAY_HPP
#define SPLENDOR_DISPLAY_HPP

#include <string>

#include "game.hpp"
#include "i18n.hpp"
#include "types.hpp"

// Tout l'affichage du jeu (sortie standard, couleurs ANSI, cadres Unicode).

// Nom de la couleur ("White", "Blue"... ou "Blanc", "Bleu"...)
std::string color_name(Gem color, Lang lang);

// `text` entouré du code couleur ANSI de `color` (sauf si NO_COLOR est défini)
std::string colored(Gem color, const std::string& text);

// "Blue: 2, Red: 1" (les couleurs à 0 sont omises), "none" si tout est à 0
std::string format_resources(const Resources& resources, Lang lang);

// Jetons d'un joueur, or compris
std::string format_tokens(const Player& player, Lang lang);

// `text` dans la couleur du joueur `player` (console)
std::string player_colored(int player, const std::string& text);

// Phrase décrivant un événement ("Alice takes 1 White, 1 Green, 1 Red."),
// sans codes couleur : utilisée par la console et par la fenêtre.
std::string describe_event(const Game& game, const Event& event, Lang lang);

// Le plateau dessiné comme la vraie table : nobles, 3 rangées de cartes avec
// leur pioche, réserve de jetons, puis les joueurs.
void show_board(const Game& game, Lang lang);
void show_help(Mode mode, Lang lang);
void show_results(const Game& game, Lang lang);

#endif
