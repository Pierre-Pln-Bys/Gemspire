#ifndef SPLENDOR_MOVE_HPP
#define SPLENDOR_MOVE_HPP

#include <string>

#include "game.hpp"

// Un coup, sous une forme commune à toutes les interfaces : fenêtre, console,
// ordinateur et serveur en ligne appliquent les coups avec apply_move.

const int GOLD_GEM = COLOR_COUNT;  // indice de l'or pour la défausse

enum class MoveType { TakeTokens, BuyCard, BuyReserved, Reserve, Discard, ChooseNoble };

struct Move {
    MoveType type = MoveType::TakeTokens;
    Resources tokens = {0, 0, 0, 0, 0};  // TakeTokens : jetons voulus
    int level = 0;                       // BuyCard, Reserve : niveau (1 à 3)
    int position = 0;                    // BuyCard : 1 à 4 ; Reserve : 1 à 4, 0 = dessus de la pioche ;
                                         // BuyReserved : carte réservée 1 à 3
    int gem = 0;                         // Discard : 0 à 4, ou GOLD_GEM
    int noble = 0;                       // ChooseNoble : indice dans game.nobles()
};

// Applique le coup au joueur courant. Retourne "" ou le message d'erreur.
std::string apply_move(Game& game, const Move& move);

// Constructeurs pratiques
Move take_move(const Resources& tokens);
Move buy_move(int level, int position);
Move buy_reserved_move(int index);
Move reserve_move(int level, int position);
Move discard_move(int gem);
Move noble_move(int noble);

#endif
