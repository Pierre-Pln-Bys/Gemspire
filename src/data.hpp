#ifndef SPLENDOR_DATA_HPP
#define SPLENDOR_DATA_HPP

#include <vector>

#include "types.hpp"

// Retourne toutes les cartes du niveau `level` (1, 2 ou 3)
const std::vector<Card>& cards_of_level(int level);

// Retourne les 10 nobles du jeu
const std::vector<Noble>& all_nobles();

#endif
