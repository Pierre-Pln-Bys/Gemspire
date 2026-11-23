#include "rules.hpp"

int tokens_per_color(int player_count)
{
    if (player_count <= 2) {
        return 4;
    }
    if (player_count == 3) {
        return 5;
    }
    return 7;
}

int total(const Resources& resources)
{
    int sum = 0;
    for (int value : resources) {
        sum += value;
    }
    return sum;
}

int score(const std::vector<Card>& cards)
{
    int sum = 0;
    for (const Card& card : cards) {
        sum += card.points;
    }
    return sum;
}

Resources bonuses(const std::vector<Card>& cards)
{
    Resources bonus = {0, 0, 0, 0, 0};
    for (const Card& card : cards) {
        bonus[index_of(card.provides)]++;
    }
    return bonus;
}

bool noble_can_visit(const Resources& bonus, const Noble& noble)
{
    for (int c = 0; c < COLOR_COUNT; c++) {
        if (bonus[c] < noble.cost[c]) {
            return false;
        }
    }
    return true;
}

Resources tokens_to_pay(const Resources& bonus, const Card& card)
{
    Resources to_pay = {0, 0, 0, 0, 0};
    for (int c = 0; c < COLOR_COUNT; c++) {
        if (card.cost[c] > bonus[c]) {
            to_pay[c] = card.cost[c] - bonus[c];
        }
    }
    return to_pay;
}

int missing_tokens(const Resources& bonus, const Resources& wallet, const Card& card)
{
    Resources to_pay = tokens_to_pay(bonus, card);
    int missing = 0;
    for (int c = 0; c < COLOR_COUNT; c++) {
        if (to_pay[c] > wallet[c]) {
            missing += to_pay[c] - wallet[c];
        }
    }
    return missing;
}

std::string check_pick(const Resources& wanted, const Resources& bank)
{
    int double_color = -1;    // couleur demandée en 2 exemplaires (-1 : aucune)
    int different_colors = 0; // nombre de couleurs demandées
    for (int c = 0; c < COLOR_COUNT; c++) {
        if (wanted[c] < 0 || wanted[c] > 2) {
            return "Invalid token amount.";
        }
        if (wanted[c] > bank[c]) {
            return "There aren't enough tokens of this colour in the bank.";
        }
        if (wanted[c] == 2) {
            double_color = c;
        }
        if (wanted[c] > 0) {
            different_colors++;
        }
    }

    if (different_colors == 0) {
        return "No token requested.";
    }

    // Cas b) : 2 jetons de la même couleur, et rien d'autre
    if (double_color != -1) {
        if (different_colors > 1) {
            return "You can't take 2 identical tokens and other tokens as well.";
        }
        if (bank[double_color] < MIN_STACK_FOR_DOUBLE) {
            return "There must be at least 4 tokens in the stack to take 2 identical ones.";
        }
        return "";
    }

    // Cas a) : des jetons de couleurs différentes, 3 si possible
    int available_colors = 0;
    for (int c = 0; c < COLOR_COUNT; c++) {
        if (bank[c] > 0) {
            available_colors++;
        }
    }
    int expected = available_colors < 3 ? available_colors : 3;
    if (different_colors != expected) {
        return expected == 1 ? "You must take 1 token."
                             : "You must take " + std::to_string(expected) + " tokens of different colours.";
    }
    return "";
}
