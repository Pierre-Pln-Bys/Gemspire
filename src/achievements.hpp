#ifndef SPLENDOR_ACHIEVEMENTS_HPP
#define SPLENDOR_ACHIEVEMENTS_HPP

#include <string>
#include <vector>

#include "game.hpp"

// Succès des parties en ligne (logique pure : le serveur les calcule à la
// fin d'une partie, le jeu affiche leur nom et leur description).

struct AchievementInfo {
    const char* code;          // identifiant stocké en base
    const char* name;          // nom affiché (anglais, traduit par tr)
    const char* description;
};

const int ACHIEVEMENT_COUNT = 11;
extern const AchievementInfo ACHIEVEMENTS[ACHIEVEMENT_COUNT];

// Nom et description d'un succès (nullptr si le code est inconnu)
const AchievementInfo* find_achievement(const std::string& code);

// Ce qu'il faut savoir d'un joueur à la fin d'une partie en ligne
struct AchievementContext {
    int seat;                    // place du joueur dans la partie
    std::vector<int> places;     // place finale de chaque joueur (0 = premier)
    std::vector<int> ratings;    // Elo de chaque joueur avant la partie
    int games_after;             // parties jouées par ce joueur, celle-ci comprise
    int elo_after;               // son Elo après la partie
};

// Succès mérités dans cette partie (les codes ; le serveur ne garde que les
// nouveaux). Un joueur qui a abandonné n'en reçoit aucun.
std::vector<std::string> earned_achievements(const Game& game, const AchievementContext& context);

#endif
