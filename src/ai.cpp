#include "ai.hpp"

#include <vector>

#include "rules.hpp"

// Une carte que le joueur pourrait viser : visible (level, position) ou
// réservée (level = 0, position = index dans sa main, à partir de 1)
struct Target {
    Card card;
    int level;
    int position;
};

// ---------------------------------------------------------------- Outils

// Jetons de couleur qui manquent pour payer la carte (sans compter l'or)
static Resources missing_colors(const Player& player, const Card& card)
{
    Resources to_pay = tokens_to_pay(player.bonuses(), card);
    Resources missing = {0, 0, 0, 0, 0};
    for (int c = 0; c < COLOR_COUNT; c++) {
        if (to_pay[c] > player.tokens[c]) {
            missing[c] = to_pay[c] - player.tokens[c];
        }
    }
    return missing;
}

// Toutes les cartes que le joueur courant peut viser
static std::vector<Target> all_targets(const Game& game)
{
    std::vector<Target> targets;
    for (int level = 1; level <= LEVEL_COUNT; level++) {
        const std::vector<Card>& row = game.visible(level);
        for (int i = 0; i < static_cast<int>(row.size()); i++) {
            targets.push_back({row[i], level, i + 1});
        }
    }
    const std::vector<Card>& reserved = game.current_player().reserved;
    for (int i = 0; i < static_cast<int>(reserved.size()); i++) {
        targets.push_back({reserved[i], 0, i + 1});
    }
    return targets;
}

static bool can_buy_target(const Game& game, const Target& target)
{
    if (target.level == 0) {
        return game.can_buy_reserved(target.position);
    }
    return game.can_buy(target.level, target.position);
}

static Move buy_target(const Target& target)
{
    if (target.level == 0) {
        return buy_reserved_move(target.position);
    }
    return buy_move(target.level, target.position);
}

// Combien la carte aide à attirer un noble (bonus de la bonne couleur)
static double noble_help(const Game& game, const Player& player, const Card& card)
{
    Resources bonus = player.bonuses();
    int c = index_of(card.provides);
    double help = 0;
    for (const Noble& noble : game.nobles()) {
        if (noble.cost[c] > bonus[c]) {
            int gap = 0;
            for (int k = 0; k < COLOR_COUNT; k++) {
                if (noble.cost[k] > bonus[k]) {
                    gap += noble.cost[k] - bonus[k];
                }
            }
            help += 3.0 / gap;  // plus le noble est proche, plus la carte aide
        }
    }
    return help;
}

// Combien de cartes visibles à points demandent la couleur que fournit la carte
static int bonus_usefulness(const Game& game, const Card& card)
{
    int c = index_of(card.provides);
    int count = 0;
    for (int level = 1; level <= LEVEL_COUNT; level++) {
        for (const Card& other : game.visible(level)) {
            if (other.points > 0 && other.cost[c] > 0) {
                count++;
            }
        }
    }
    return count;
}

// Valeur d'une carte pour l'IA moyenne : ses points, l'aide pour les nobles,
// et l'intérêt de son bonus pour acheter d'autres cartes ensuite
static double card_value(const Game& game, const Player& player, const Card& card)
{
    return card.points * 3.0 + noble_help(game, player, card) + 0.4 * bonus_usefulness(game, card);
}

// Construit une prise de jetons valide en privilégiant les couleurs `need`.
// Retourne false si aucune prise n'est possible (réserve vide).
static bool choose_pick(const Game& game, const Resources& need, Resources& pick)
{
    const Resources& bank = game.bank();
    pick = {0, 0, 0, 0, 0};

    int available = 0;
    int needed_colors = 0;
    int only_color = -1;
    for (int c = 0; c < COLOR_COUNT; c++) {
        available += bank[c] > 0;
        if (need[c] > 0 && bank[c] > 0) {
            needed_colors++;
            only_color = c;
        }
    }
    if (available == 0) {
        return false;
    }

    // Une seule couleur utile, dont il faut au moins 2 : on en prend 2
    if (needed_colors == 1 && need[only_color] >= 2 && bank[only_color] >= MIN_STACK_FOR_DOUBLE) {
        pick[only_color] = 2;
        return true;
    }

    // Sinon des couleurs différentes : d'abord les plus utiles, puis les plus abondantes
    int wanted = available < 3 ? available : 3;
    for (int k = 0; k < wanted; k++) {
        int best = -1;
        for (int c = 0; c < COLOR_COUNT; c++) {
            if (bank[c] == 0 || pick[c] > 0) {
                continue;
            }
            if (best == -1 || need[c] > need[best] || (need[c] == need[best] && bank[c] > bank[best])) {
                best = c;
            }
        }
        pick[best] = 1;
    }
    return true;
}

// ---------------------------------------------------------------- Choix d'une cible

// Facile : la carte la moins chère à compléter (puis la plus rentable)
static int easy_target(const Game& game, const std::vector<Target>& targets)
{
    const Player& me = game.current_player();
    int best = -1;
    int best_missing = 0;
    for (int i = 0; i < static_cast<int>(targets.size()); i++) {
        int missing = total(missing_colors(me, targets[i].card));
        if (best == -1 || missing < best_missing
            || (missing == best_missing && targets[i].card.points > targets[best].card.points)) {
            best = i;
            best_missing = missing;
        }
    }
    return best;
}

// Moyen : le meilleur rapport valeur / (jetons manquants)²
static int medium_target(const Game& game, const std::vector<Target>& targets)
{
    const Player& me = game.current_player();
    int best = -1;
    double best_score = 0;
    for (int i = 0; i < static_cast<int>(targets.size()); i++) {
        int missing = total(missing_colors(me, targets[i].card)) - me.gold;
        if (missing < 0) {
            missing = 0;
        }
        // Les cartes proches comptent beaucoup plus que les cartes lointaines
        double score = card_value(game, me, targets[i].card) / ((missing + 1.0) * (missing + 1.0));
        if (best == -1 || score > best_score) {
            best = i;
            best_score = score;
        }
    }
    return best;
}

// ---------------------------------------------------------------- Étapes

static Move choose_action(const Game& game, bool medium)
{
    const Player& me = game.current_player();
    std::vector<Target> targets = all_targets(game);

    // 1. Acheter si possible : facile = le plus de points, moyen = la meilleure valeur
    int best_buy = -1;
    double best_buy_value = 0;
    for (int i = 0; i < static_cast<int>(targets.size()); i++) {
        if (!can_buy_target(game, targets[i])) {
            continue;
        }
        double value = medium ? card_value(game, me, targets[i].card) : targets[i].card.points;
        if (best_buy == -1 || value > best_buy_value) {
            best_buy = i;
            best_buy_value = value;
        }
    }
    if (best_buy != -1) {
        return buy_target(targets[best_buy]);
    }

    int target = medium ? medium_target(game, targets) : easy_target(game, targets);

    // 2. Moyen, partie classique : bloquer la carte qui ferait gagner un
    //    adversaire proche des 15 points
    if (medium && game.can_reserve()) {
        for (int p = 0; p < static_cast<int>(game.players().size()); p++) {
            const Player& other = game.players()[p];
            if (p == game.current_index() || other.score() < WINNING_SCORE - 5) {
                continue;
            }
            for (const Target& t : targets) {
                if (t.level > 0 && other.score() + t.card.points >= WINNING_SCORE
                    && missing_tokens(other.bonuses(), other.tokens, t.card) <= other.gold) {
                    return reserve_move(t.level, t.position);
                }
            }
        }
    }

    // 3. Prendre les jetons qui rapprochent de la cible
    Resources need = {0, 0, 0, 0, 0};
    if (target != -1) {
        need = missing_colors(me, targets[target].card);
    }
    // Moyen : les couleurs de la 2e meilleure cible départagent les jetons
    if (medium && target != -1) {
        std::vector<Target> others = targets;
        others.erase(others.begin() + target);
        int second = medium_target(game, others);
        if (second != -1) {
            Resources more = missing_colors(me, others[second].card);
            for (int c = 0; c < COLOR_COUNT; c++) {
                need[c] = need[c] * 2 + (more[c] > 0 ? 1 : 0);
            }
        }
    }
    Resources pick;
    if (choose_pick(game, need, pick) && check_pick(pick, game.bank()).empty()) {
        return take_move(pick);
    }

    // 4. Plus de jetons : réserver (classique) la cible ou le dessus d'une pioche
    if (game.can_reserve()) {
        if (target != -1 && targets[target].level > 0) {
            return reserve_move(targets[target].level, targets[target].position);
        }
        for (int level = 1; level <= LEVEL_COUNT; level++) {
            if (game.deck_size(level) > 0) {
                return reserve_move(level, 0);
            }
        }
        for (int level = 1; level <= LEVEL_COUNT; level++) {
            if (!game.visible(level).empty()) {
                return reserve_move(level, 1);
            }
        }
    }
    // Aucun coup possible (ne devrait pas arriver : Game fait passer ces joueurs)
    return take_move({0, 0, 0, 0, 0});
}

// Défausse le jeton le moins utile : la couleur dont il a le plus et qui ne
// sert pas à sa cible ; l'or en dernier.
static Move choose_discard(const Game& game, bool medium)
{
    const Player& me = game.current_player();
    std::vector<Target> targets = all_targets(game);
    int target = medium ? medium_target(game, targets) : easy_target(game, targets);
    Resources useful = {0, 0, 0, 0, 0};
    if (target != -1) {
        useful = tokens_to_pay(me.bonuses(), targets[target].card);
    }
    int best = -1;
    int best_extra = 0;
    for (int c = 0; c < COLOR_COUNT; c++) {
        int extra = me.tokens[c] - useful[c];
        if (me.tokens[c] > 0 && (best == -1 || extra > best_extra)) {
            best = c;
            best_extra = extra;
        }
    }
    return discard_move(best == -1 ? GOLD_GEM : best);
}

bool ai_to_play(const Game& game)
{
    return !game.is_over() && game.current_player().controller != Controller::Human;
}

Move ai_choose(const Game& game, bool medium)
{
    switch (game.step()) {
    case TurnStep::Discard:
        return choose_discard(game, medium);
    case TurnStep::ChooseNoble:
        return noble_move(game.noble_choices()[0]);
    default:
        return choose_action(game, medium);
    }
}

std::string ai_play(Game& game)
{
    if (!ai_to_play(game)) {
        return "Ce n'est pas à l'ordinateur de jouer.";
    }
    if (game.is_over()) {
        return "La partie est terminée.";
    }
    bool medium = game.current_player().controller == Controller::AiMedium;
    return apply_move(game, ai_choose(game, medium));
}
