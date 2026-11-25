#ifndef SPLENDOR_SEASON_PASS_HPP
#define SPLENDOR_SEASON_PASS_HPP

#include <string>
#include <vector>

#include "game.hpp"
#include "i18n.hpp"

// Passe de saison (v4.2), logique pure : partagée par le serveur (XP,
// récompenses) et le jeu (affichage). Gratuit pour tous, cosmétique seulement.
//
// - 50 paliers par saison, PASS_XP_PER_TIER XP chacun ; une récompense tous
//   les 2 paliers (25 objets par saison).
// - Les objets sont générés à partir du numéro de saison (couleurs, noms) :
//   ceux d'une saison passée restent exclusifs, rien à dessiner à la main.
// - Code d'un objet : « s<saison>.<sorte>.<n> », ex. « s1.theme.0 ».

const int PASS_TIERS = 50;
const int PASS_XP_PER_TIER = 700;
const int PASS_DAILY_BONUS_XP = 150;    // première partie en ligne du jour
const int PASS_FRIENDLY_PER_DAY = 5;    // parties amicales qui rapportent de l'XP par jour
const int PASS_CHALLENGES_PER_WEEK = 3;

enum class CosmeticKind { Theme, CardBack, Tokens, Frame, Title, Avatar, Reaction };
const int COSMETIC_KIND_COUNT = 7;

struct Rgb {
    unsigned char r, g, b;
};

// Un objet du passe
struct Cosmetic {
    std::string code;
    CosmeticKind kind = CosmeticKind::Title;
    int season = 0;
    int number = 0;      // rang parmi les objets de cette sorte dans la saison
    int gem = 0;         // gemme qui donne sa couleur et son nom (0 à 11)
    Rgb color = {0, 0, 0};
    int pattern = 0;     // motif (dos de cartes, cadres, avatars) : 0 à 3
    int symbol = 0;      // avatar : symbole de gemme (0 cercle ... 5 étoile)
};

std::string kind_code(CosmeticKind kind);                   // "theme", "cardback"...
bool parse_kind_code(const std::string& text, CosmeticKind& kind);
// Objet de ce code (false si le code n'existe pas : saison, sorte ou numéro invalide)
bool find_cosmetic(const std::string& code, Cosmetic& cosmetic);
// Récompense d'un palier (paliers pairs seulement ; false sinon)
bool pass_reward(int season, int tier, Cosmetic& cosmetic);
// Toutes les récompenses d'une saison, du palier 2 au palier 50
std::vector<std::pair<int, Cosmetic>> season_rewards(int season);
// Nom affiché (« Tapis rubis », « Ruby felt ») ; pour un titre ou une
// réaction : le texte lui-même
std::string cosmetic_name(const Cosmetic& cosmetic, Lang lang);
std::string kind_name(CosmeticKind kind, Lang lang);        // « Thème », « Theme »...

// Palier atteint avec cette XP (0 à PASS_TIERS)
int pass_tier(int xp);
// XP d'une partie en ligne terminée (`kind` : "friendly", "ranked", "season")
int game_xp(const std::string& kind, bool won, bool forfeited);

// --- Défis de la semaine (semaine commençant le lundi à 0 h UTC)
struct ChallengeInfo {
    const char* code;
    const char* english;    // {0} = objectif
    const char* french;
    int target;
    int xp;
};

int week_number(long long now);                            // semaines depuis 1970
long long week_end(int week);                              // fin de la semaine (secondes)
std::vector<const ChallengeInfo*> weekly_challenges(int week);
const ChallengeInfo* find_challenge(const std::string& code);
std::string challenge_text(const ChallengeInfo& challenge, Lang lang);
// Avancée d'un défi apportée par une partie en ligne terminée
int challenge_progress(const ChallengeInfo& challenge, const Game& game, int seat, const std::string& kind,
                       bool won);

#endif
