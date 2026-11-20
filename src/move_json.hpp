#ifndef SPLENDOR_MOVE_JSON_HPP
#define SPLENDOR_MOVE_JSON_HPP

#include <string>

#include "json.hpp"

#include "move.hpp"

// Conversion d'un coup en JSON (échangé entre le jeu et le serveur) :
//   {"type": "take", "tokens": [1, 1, 1, 0, 0]}
//   {"type": "buy", "level": 2, "position": 3}
//   {"type": "buy_reserved", "index": 1}
//   {"type": "reserve", "level": 3, "position": 0}   (0 = dessus de la pioche)
//   {"type": "discard", "gem": 5}                    (5 = or)
//   {"type": "noble", "noble": 1}

nlohmann::json move_to_json(const Move& move);

// Retourne "" si le JSON décrit un coup bien formé (rempli dans `move`),
// sinon le message d'erreur. Les règles sont vérifiées ensuite par Game.
std::string move_from_json(const nlohmann::json& json, Move& move);

#endif
