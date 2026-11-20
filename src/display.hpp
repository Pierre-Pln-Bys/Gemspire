#ifndef SPLENDOR_DISPLAY_HPP
#define SPLENDOR_DISPLAY_HPP

#include <string>

#include "game.hpp"
#include "types.hpp"

// Tout l'affichage du jeu (sortie standard, couleurs ANSI, cadres Unicode).

// Nom français de la couleur ("Blanc", "Bleu", ...)
std::string color_name(Color color);

// `text` entouré du code couleur ANSI de `color` (sauf si NO_COLOR est défini)
std::string colored(Color color, const std::string& text);

// "Bleu: 2, Rouge: 1" (les couleurs à 0 sont omises), "aucun" si tout est à 0
std::string format_resources(const Resources& resources);

// Jetons d'un joueur, or compris
std::string format_tokens(const Player& player);

// Le plateau dessiné comme la vraie table : nobles, 3 rangées de cartes avec
// leur pioche, réserve de jetons, puis les joueurs.
void show_board(const Game& game);
void show_help(Mode mode);
void show_results(const Game& game);

#endif
