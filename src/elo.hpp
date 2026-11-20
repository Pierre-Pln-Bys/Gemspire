#ifndef SPLENDOR_ELO_HPP
#define SPLENDOR_ELO_HPP

#include <vector>

#include "game.hpp"

// Classement Elo des parties en ligne (2 à 4 joueurs).
// Une partie à plusieurs compte comme des duels deux à deux : chaque joueur
// gagne contre ceux qu'il devance, perd contre ceux qui le devancent, fait
// match nul en cas d'égalité. Le gain total est divisé par (joueurs - 1).

const int ELO_START = 1000;
const int ELO_K = 32;

// Place de chaque joueur à la fin (0 = premier). Mêmes points et même nombre
// de cartes : même place.
std::vector<int> final_places(const Game& game);

// Variation d'Elo de chaque joueur, à partir de son Elo avant la partie et de
// sa place. La somme des variations est (presque) nulle.
std::vector<int> elo_changes(const std::vector<int>& ratings, const std::vector<int>& places);

#endif
