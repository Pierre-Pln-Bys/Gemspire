#ifndef SPLENDOR_AI_HPP
#define SPLENDOR_AI_HPP

#include <string>

#include "game.hpp"
#include "move.hpp"

// Adversaires ordinateur. Logique pure : aucune entrée/sortie.
//
// - Facile : achète la carte qui rapporte le plus de points, sinon prend les
//   jetons qui le rapprochent de la carte visible la moins chère.
// - Moyen : évalue les cartes (points, aide pour attirer un noble), vise la
//   meilleure carte par rapport à ce qu'il lui manque, et en partie classique
//   réserve une carte qu'un adversaire peut acheter ou une cible lointaine.

// Joue UNE étape pour le joueur courant (action, défausse ou choix du noble),
// selon game.step(). Ne fait rien si c'est le tour d'un humain.
// Retourne une chaîne vide si un coup a été joué, sinon un message d'erreur.
std::string ai_play(Game& game);

// Le coup que choisirait l'ordinateur (moyen ou facile) pour le joueur
// courant, sans le jouer. Sert aussi pour les tests du serveur.
Move ai_choose(const Game& game, bool medium);

// Vrai si c'est à l'ordinateur de jouer
bool ai_to_play(const Game& game);

#endif
