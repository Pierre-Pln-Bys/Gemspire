#ifndef SPLENDOR_RULES_HPP
#define SPLENDOR_RULES_HPP

#include <string>
#include <vector>

#include "types.hpp"

// Fonctions pures décrivant les règles. Aucune entrée/sortie ici.

const int MAX_TOKENS_IN_HAND = 10;
const int WINNING_SCORE = 15;
const int MIN_STACK_FOR_DOUBLE = 4;  // pile minimale pour prendre 2 jetons identiques

// Nombre de jetons par couleur au début de la partie (2, 3 ou 4 joueurs)
int tokens_per_color(int player_count);

// Somme de toutes les ressources
int total(const Resources& resources);

// Somme des points des cartes
int score(const std::vector<Card>& cards);

// Bonus fournis par un ensemble de cartes (nombre de cartes de chaque couleur)
Resources bonuses(const std::vector<Card>& cards);

// Vrai si les bonus suffisent pour recevoir la visite du noble
bool noble_can_visit(const Resources& bonus, const Noble& noble);

// Jetons à payer pour la carte : coût moins les bonus (jamais négatif)
Resources tokens_to_pay(const Resources& bonus, const Card& card);

// Nombre de jetons manquants pour acheter la carte (0 = achat possible)
int missing_tokens(const Resources& bonus, const Resources& wallet, const Card& card);

// Vérifie une prise de jetons `wanted` dans la réserve `bank`.
// Retourne une chaîne vide si la prise est valide, sinon le message d'erreur.
std::string check_pick(const Resources& wanted, const Resources& bank);

#endif
