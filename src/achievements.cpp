#include "achievements.hpp"

const AchievementInfo ACHIEVEMENTS[ACHIEVEMENT_COUNT] = {
    {"first_game", "First steps", "Finish an online game."},
    {"first_win", "First victory", "Win an online game."},
    {"noble", "Royal visit", "Receive a noble."},
    {"three_nobles", "High society", "Receive 3 nobles in one game."},
    {"no_reserve_win", "No safety net", "Win a classic game without reserving a card."},
    {"fast_win", "Lightning", "Win in 20 rounds or fewer."},
    {"twenty_points", "Overflowing", "Finish a game with 20 points or more."},
    {"giant_slayer", "Giant slayer", "Finish ahead of a player rated 100 Elo higher."},
    {"ten_games", "Regular", "Play 10 online games."},
    {"fifty_games", "Veteran", "Play 50 online games."},
    {"elo_1200", "Rising star", "Reach 1200 Elo."},
};

const AchievementInfo* find_achievement(const std::string& code)
{
    for (const AchievementInfo& info : ACHIEVEMENTS) {
        if (code == info.code) {
            return &info;
        }
    }
    return nullptr;
}

std::vector<std::string> earned_achievements(const Game& game, const AchievementContext& context)
{
    std::vector<std::string> earned;
    const Player& me = game.players()[context.seat];
    if (me.forfeited) {
        return earned;
    }
    bool won = context.places[context.seat] == 0;
    bool reserved = false;
    for (const Event& event : game.events()) {
        reserved = reserved || (event.type == EventType::Reserve && event.player == context.seat);
    }

    earned.push_back("first_game");
    if (won) {
        earned.push_back("first_win");
    }
    if (!me.nobles.empty()) {
        earned.push_back("noble");
    }
    if (me.nobles.size() >= 3) {
        earned.push_back("three_nobles");
    }
    if (won && game.mode() == Mode::Classic && !reserved) {
        earned.push_back("no_reserve_win");
    }
    if (won && game.round() <= 20) {
        earned.push_back("fast_win");
    }
    if (me.score() >= 20) {
        earned.push_back("twenty_points");
    }
    for (int other = 0; other < static_cast<int>(context.places.size()); other++) {
        if (context.places[context.seat] < context.places[other]
            && context.ratings[other] >= context.ratings[context.seat] + 100) {
            earned.push_back("giant_slayer");
            break;
        }
    }
    if (context.games_after >= 10) {
        earned.push_back("ten_games");
    }
    if (context.games_after >= 50) {
        earned.push_back("fifty_games");
    }
    if (context.elo_after >= 1200) {
        earned.push_back("elo_1200");
    }
    return earned;
}
