#ifndef SPLENDOR_SAVE_HPP
#define SPLENDOR_SAVE_HPP

#include <memory>
#include <string>

#include "game.hpp"

// Sauvegarde d'une partie dans un format texte lisible (fichiers .splendor).
// Tout l'état est enregistré, y compris l'ordre des pioches : une partie
// rechargée continue exactement comme l'originale. L'historique des
// événements n'est pas enregistré.

// Retourne le texte de la sauvegarde
std::string save_game(const Game& game);

// Recrée une partie depuis le texte d'une sauvegarde. Retourne une chaîne
// vide si tout va bien (la partie est alors dans `game`), sinon l'erreur.
std::string load_game(const std::string& text, std::unique_ptr<Game>& game);

// Mêmes opérations avec un fichier. Retournent "" ou un message d'erreur.
std::string save_game_to_file(const Game& game, const std::string& path);
std::string load_game_from_file(const std::string& path, std::unique_ptr<Game>& game);

#endif
