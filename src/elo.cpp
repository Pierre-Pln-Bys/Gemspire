#include "elo.hpp"

#include <cmath>

std::vector<int> final_places(const Game& game)
{
    const std::vector<Player>& players = game.players();
    std::vector<int> places;
    for (const Player& player : players) {
        // Place = nombre de joueurs strictement meilleurs
        int better = 0;
        for (const Player& other : players) {
            bool more_points = other.score() > player.score();
            bool fewer_cards = other.score() == player.score() && other.cards.size() < player.cards.size();
            if (more_points || fewer_cards) {
                better++;
            }
        }
        places.push_back(better);
    }
    return places;
}

std::vector<int> elo_changes(const std::vector<int>& ratings, const std::vector<int>& places)
{
    int count = static_cast<int>(ratings.size());
    std::vector<int> changes(count, 0);
    if (count < 2) {
        return changes;
    }
    for (int i = 0; i < count; i++) {
        double total = 0;
        for (int j = 0; j < count; j++) {
            if (i == j) {
                continue;
            }
            double result = places[i] < places[j] ? 1.0 : (places[i] == places[j] ? 0.5 : 0.0);
            double expected = 1.0 / (1.0 + std::pow(10.0, (ratings[j] - ratings[i]) / 400.0));
            total += result - expected;
        }
        changes[i] = static_cast<int>(std::lround(ELO_K * total / (count - 1)));
    }
    return changes;
}
