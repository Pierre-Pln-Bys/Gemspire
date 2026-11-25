#include "season_pass.hpp"

#include <cstdlib>

// ---------------------------------------------------------------- Catalogue

// Gemmes qui donnent leur couleur et leur nom aux objets d'une saison
struct PassGem {
    const char* english;
    const char* french;
    Rgb color;
};

const int PASS_GEM_COUNT = 12;
static const PassGem PASS_GEMS[PASS_GEM_COUNT] = {
    {"Ruby", "rubis", {200, 30, 60}},          {"Sapphire", "saphir", {40, 90, 200}},
    {"Emerald", "émeraude", {20, 150, 90}},    {"Amber", "ambre", {230, 150, 30}},
    {"Amethyst", "améthyste", {140, 70, 190}}, {"Onyx", "onyx", {45, 45, 55}},
    {"Pearl", "perle", {225, 220, 205}},       {"Jade", "jade", {80, 170, 130}},
    {"Topaz", "topaze", {240, 190, 60}},       {"Garnet", "grenat", {130, 20, 40}},
    {"Opal", "opale", {150, 200, 220}},        {"Quartz", "quartz", {230, 170, 190}},
};

// Sorte de la récompense de chaque palier pair (2, 4, ... 50)
static const CosmeticKind REWARD_KINDS[PASS_TIERS / 2] = {
    CosmeticKind::Title,    CosmeticKind::Reaction, CosmeticKind::Frame,    CosmeticKind::Avatar,
    CosmeticKind::Theme,    CosmeticKind::Title,    CosmeticKind::Reaction, CosmeticKind::CardBack,
    CosmeticKind::Avatar,   CosmeticKind::Frame,    CosmeticKind::Title,    CosmeticKind::Tokens,
    CosmeticKind::Reaction, CosmeticKind::Avatar,   CosmeticKind::Theme,    CosmeticKind::Title,
    CosmeticKind::Frame,    CosmeticKind::Reaction, CosmeticKind::Avatar,   CosmeticKind::CardBack,
    CosmeticKind::Title,    CosmeticKind::Reaction, CosmeticKind::Frame,    CosmeticKind::Tokens,
    CosmeticKind::Theme,
};

struct PassText {
    const char* english;
    const char* french;
};

static const PassText TITLES[] = {
    {"Gem Collector", "Collectionneur de gemmes"}, {"Noble Hunter", "Chasseur de nobles"},
    {"Merchant", "Marchand"},                      {"Strategist", "Stratège"},
    {"Gem Master", "Maître des gemmes"},           {"Tycoon", "Magnat"},
    {"Jeweller", "Joaillier"},                     {"Prospector", "Prospecteur"},
    {"Patron of the Arts", "Mécène"},              {"Treasurer", "Trésorier"},
    {"Gold Digger", "Chercheur d'or"},             {"Court Favourite", "Favori de la cour"},
    {"Card Shark", "As des cartes"},               {"Mine Owner", "Propriétaire de mines"},
    {"Gem Cutter", "Tailleur de gemmes"},          {"Royal Guest", "Invité royal"},
    {"Explorer", "Explorateur"},                   {"Baron", "Baron"},
    {"Diamond Hand", "Main de diamant"},           {"Legend", "Légende"},
};
static const int TITLE_COUNT = sizeof(TITLES) / sizeof(TITLES[0]);

static const PassText REACTIONS[] = {
    {"Well played!", "Bien joué !"},      {"Wow!", "Waouh !"},
    {"Close one!", "C'était serré !"},    {"Rematch?", "Revanche ?"},
    {"Nice combo!", "Joli combo !"},      {"Thinking...", "Je réfléchis..."},
    {"Lucky!", "Quelle chance !"},        {"Your turn!", "À toi !"},
    {"Gem rush!", "Ruée vers les gemmes !"}, {"That noble is mine!", "Ce noble est à moi !"},
    {"Oops", "Oups"},                     {"Gold!", "De l'or !"},
    {"Not that card!", "Pas cette carte !"}, {"Respect", "Respect"},
    {"So close...", "Si près..."},        {"Let's go!", "C'est parti !"},
    {"Tactical!", "Tactique !"},          {"Hello!", "Salut !"},
};
static const int REACTION_TEXT_COUNT = sizeof(REACTIONS) / sizeof(REACTIONS[0]);

static const char* const KIND_CODES[COSMETIC_KIND_COUNT] = {"theme",  "cardback", "tokens",  "frame",
                                                            "title", "avatar",   "reaction"};

std::string kind_code(CosmeticKind kind)
{
    return KIND_CODES[static_cast<int>(kind)];
}

bool parse_kind_code(const std::string& text, CosmeticKind& kind)
{
    for (int i = 0; i < COSMETIC_KIND_COUNT; i++) {
        if (text == KIND_CODES[i]) {
            kind = static_cast<CosmeticKind>(i);
            return true;
        }
    }
    return false;
}

// Nombre d'objets de cette sorte dans une saison
static int kind_count(CosmeticKind kind)
{
    int count = 0;
    for (CosmeticKind reward : REWARD_KINDS) {
        count += reward == kind ? 1 : 0;
    }
    return count;
}

// Les caractéristiques d'un objet ne dépendent que de (saison, sorte, numéro)
static Cosmetic make_cosmetic(int season, CosmeticKind kind, int number)
{
    Cosmetic cosmetic;
    cosmetic.kind = kind;
    cosmetic.season = season;
    cosmetic.number = number;
    cosmetic.code = "s" + std::to_string(season) + "." + kind_code(kind) + "." + std::to_string(number);
    cosmetic.gem = (season * 5 + static_cast<int>(kind) * 3 + number * 7) % PASS_GEM_COUNT;
    cosmetic.color = PASS_GEMS[cosmetic.gem].color;
    cosmetic.pattern = (season + number + static_cast<int>(kind)) % 4;
    cosmetic.symbol = (season * 3 + number) % 6;
    return cosmetic;
}

bool find_cosmetic(const std::string& code, Cosmetic& cosmetic)
{
    // « s<saison>.<sorte>.<n> »
    size_t first = code.find('.');
    size_t second = first == std::string::npos ? std::string::npos : code.find('.', first + 1);
    if (code.size() < 6 || code.size() > 40 || code[0] != 's' || second == std::string::npos) {
        return false;
    }
    std::string season_text = code.substr(1, first - 1);
    std::string number_text = code.substr(second + 1);
    CosmeticKind kind;
    if (!parse_kind_code(code.substr(first + 1, second - first - 1), kind) || season_text.empty()
        || season_text.size() > 4 || number_text.empty() || number_text.size() > 2) {
        return false;
    }
    for (char c : season_text + number_text) {
        if (c < '0' || c > '9') {
            return false;
        }
    }
    int season = std::atoi(season_text.c_str());
    int number = std::atoi(number_text.c_str());
    if (season < 1 || number >= kind_count(kind)) {
        return false;
    }
    cosmetic = make_cosmetic(season, kind, number);
    return cosmetic.code == code;  // refuse « s01... » ou « ...00 »
}

bool pass_reward(int season, int tier, Cosmetic& cosmetic)
{
    if (season < 1 || tier < 2 || tier > PASS_TIERS || tier % 2 != 0) {
        return false;
    }
    int index = tier / 2 - 1;
    CosmeticKind kind = REWARD_KINDS[index];
    int number = 0;
    for (int i = 0; i < index; i++) {
        number += REWARD_KINDS[i] == kind ? 1 : 0;
    }
    cosmetic = make_cosmetic(season, kind, number);
    return true;
}

std::vector<std::pair<int, Cosmetic>> season_rewards(int season)
{
    std::vector<std::pair<int, Cosmetic>> rewards;
    for (int tier = 2; tier <= PASS_TIERS; tier += 2) {
        Cosmetic cosmetic;
        if (pass_reward(season, tier, cosmetic)) {
            rewards.push_back({tier, cosmetic});
        }
    }
    return rewards;
}

std::string kind_name(CosmeticKind kind, Lang lang)
{
    bool fr = lang == Lang::French;
    switch (kind) {
    case CosmeticKind::Theme: return fr ? "Thème" : "Theme";
    case CosmeticKind::CardBack: return fr ? "Dos de cartes" : "Card back";
    case CosmeticKind::Tokens: return fr ? "Jetons" : "Tokens";
    case CosmeticKind::Frame: return fr ? "Cadre" : "Frame";
    case CosmeticKind::Title: return fr ? "Titre" : "Title";
    case CosmeticKind::Avatar: return "Avatar";
    case CosmeticKind::Reaction: return fr ? "Réaction" : "Reaction";
    }
    return "";
}

std::string cosmetic_name(const Cosmetic& cosmetic, Lang lang)
{
    bool fr = lang == Lang::French;
    int seed = cosmetic.season * 5 + cosmetic.number * 3;
    if (cosmetic.kind == CosmeticKind::Title) {
        const PassText& title = TITLES[seed % TITLE_COUNT];
        return fr ? title.french : title.english;
    }
    if (cosmetic.kind == CosmeticKind::Reaction) {
        const PassText& reaction = REACTIONS[(seed + cosmetic.season) % REACTION_TEXT_COUNT];
        return fr ? reaction.french : reaction.english;
    }
    // Les autres : nom de la gemme de leur couleur
    std::string english = PASS_GEMS[cosmetic.gem].english;
    std::string french = PASS_GEMS[cosmetic.gem].french;
    switch (cosmetic.kind) {
    case CosmeticKind::Theme: return fr ? "Tapis " + french : english + " felt";
    case CosmeticKind::CardBack: return fr ? "Dos " + french : english + " card back";
    case CosmeticKind::Tokens: return fr ? "Jetons " + french : english + " tokens";
    case CosmeticKind::Frame: return fr ? "Cadre " + french : english + " frame";
    case CosmeticKind::Avatar: return fr ? "Avatar " + french : english + " avatar";
    default: return english;
    }
}

// ---------------------------------------------------------------- XP

int pass_tier(int xp)
{
    int tier = xp / PASS_XP_PER_TIER;
    return tier < 0 ? 0 : (tier > PASS_TIERS ? PASS_TIERS : tier);
}

int game_xp(const std::string& kind, bool won, bool forfeited)
{
    if (forfeited) {
        return 0;
    }
    if (kind == "friendly") {
        return 50;
    }
    return 100 + (won ? 50 : 0);
}

// ---------------------------------------------------------------- Défis

static const ChallengeInfo CHALLENGES[] = {
    {"win_season", "Win {0} season games", "Gagner {0} parties de saison", 2, 500},
    {"win_ranked", "Win {0} ranked games", "Gagner {0} parties classées", 2, 500},
    {"play_online", "Finish {0} online games", "Terminer {0} parties en ligne", 5, 400},
    {"nobles", "Receive {0} nobles", "Recevoir {0} nobles", 3, 500},
    {"cards_white", "Buy {0} white cards", "Acheter {0} cartes blanches", 8, 400},
    {"cards_blue", "Buy {0} blue cards", "Acheter {0} cartes bleues", 8, 400},
    {"cards_green", "Buy {0} green cards", "Acheter {0} cartes vertes", 8, 400},
    {"cards_red", "Buy {0} red cards", "Acheter {0} cartes rouges", 8, 400},
    {"cards_black", "Buy {0} black cards", "Acheter {0} cartes noires", 8, 400},
    {"score_18", "Finish a game with {0} points or more", "Finir une partie avec {0} points ou plus", 18, 500},
    {"friendly", "Play {0} friendly game(s)", "Jouer {0} partie(s) amicale(s)", 1, 400},
    {"two_nobles_win", "Win a game with {0} nobles or more", "Gagner une partie avec {0} nobles ou plus", 2, 600},
};
static const int CHALLENGE_COUNT = sizeof(CHALLENGES) / sizeof(CHALLENGES[0]);

int week_number(long long now)
{
    // Le 1er janvier 1970 était un jeudi : +3 jours pour commencer le lundi
    return static_cast<int>((now / 86400 + 3) / 7);
}

long long week_end(int week)
{
    return (static_cast<long long>(week + 1) * 7 - 3) * 86400;
}

std::vector<const ChallengeInfo*> weekly_challenges(int week)
{
    // Trois défis différents, tirés à partir du numéro de semaine (les mêmes pour tous)
    std::vector<const ChallengeInfo*> chosen;
    unsigned value = static_cast<unsigned>(week) * 2654435761u;
    while (static_cast<int>(chosen.size()) < PASS_CHALLENGES_PER_WEEK) {
        value = value * 1103515245u + 12345u;
        const ChallengeInfo* candidate = &CHALLENGES[(value >> 8) % CHALLENGE_COUNT];
        bool already = false;
        for (const ChallengeInfo* challenge : chosen) {
            already = already || challenge == candidate;
        }
        if (!already) {
            chosen.push_back(candidate);
        }
    }
    return chosen;
}

const ChallengeInfo* find_challenge(const std::string& code)
{
    for (const ChallengeInfo& challenge : CHALLENGES) {
        if (code == challenge.code) {
            return &challenge;
        }
    }
    return nullptr;
}

std::string challenge_text(const ChallengeInfo& challenge, Lang lang)
{
    std::string text = lang == Lang::French ? challenge.french : challenge.english;
    size_t at = text.find("{0}");
    if (at != std::string::npos) {
        text.replace(at, 3, std::to_string(challenge.target));
    }
    return text;
}

int challenge_progress(const ChallengeInfo& challenge, const Game& game, int seat, const std::string& kind,
                       bool won)
{
    const Player& me = game.players()[seat];
    if (me.forfeited) {
        return 0;
    }
    std::string code = challenge.code;
    if (code == "win_season") {
        return won && kind == "season" ? 1 : 0;
    }
    if (code == "win_ranked") {
        return won && kind == "ranked" ? 1 : 0;
    }
    if (code == "play_online") {
        return 1;
    }
    if (code == "nobles") {
        return static_cast<int>(me.nobles.size());
    }
    const char* colors[5] = {"cards_white", "cards_blue", "cards_green", "cards_red", "cards_black"};
    for (int color = 0; color < 5; color++) {
        if (code == colors[color]) {
            return me.bonuses()[color];
        }
    }
    if (code == "score_18") {
        return me.score() >= challenge.target ? challenge.target : 0;  // fait d'un coup
    }
    if (code == "friendly") {
        return kind == "friendly" ? 1 : 0;
    }
    if (code == "two_nobles_win") {
        return won && static_cast<int>(me.nobles.size()) >= challenge.target ? challenge.target : 0;
    }
    return 0;
}
