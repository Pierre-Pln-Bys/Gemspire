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
            return "Quantité de jetons invalide.";
        }
        if (wanted[c] > bank[c]) {
            return "Il n'y a pas assez de jetons de cette couleur dans la réserve.";
        }
        if (wanted[c] == 2) {
            double_color = c;
        }
        if (wanted[c] > 0) {
            different_colors++;
        }
    }

    if (different_colors == 0) {
        return "Aucun jeton demandé.";
    }

    // Cas b) : 2 jetons de la même couleur, et rien d'autre
    if (double_color != -1) {
        if (different_colors > 1) {
            return "On ne peut pas prendre 2 jetons identiques et d'autres jetons en plus.";
        }
        if (bank[double_color] < MIN_STACK_FOR_DOUBLE) {
            return "Il faut au moins 4 jetons dans la pile pour en prendre 2 identiques.";
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
        return "Il faut prendre " + std::to_string(expected)
               + " jeton(s) de couleurs différentes.";
    }
    return "";
}
