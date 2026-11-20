// Tests de la logique du jeu (rules et game). Lancer avec : make test
#include <cassert>
#include <iostream>

#include "../src/data.hpp"
#include "../src/game.hpp"
#include "../src/rules.hpp"

static void test_data()
{
    assert(cards_of_level(1).size() == 40);
    assert(cards_of_level(2).size() == 30);
    assert(cards_of_level(3).size() == 20);
    assert(all_nobles().size() == 10);
}

static void test_tokens_to_pay()
{
    // Exemple du sujet : carte 2 bleu 2 vert, bonus 1 bleu 1 vert
    Card card = {0, Color::Red, {0, 2, 2, 0, 0}};
    Resources bonus = {0, 1, 1, 0, 0};
    Resources wallet_ok = {0, 1, 1, 1, 0};
    Resources wallet_ko = {1, 0, 0, 1, 1};
    assert(missing_tokens(bonus, wallet_ok, card) == 0);
    assert(missing_tokens(bonus, wallet_ko, card) == 2);
    Resources to_pay = tokens_to_pay(bonus, card);
    assert(to_pay[1] == 1 && to_pay[2] == 1 && total(to_pay) == 2);
    // Les bonus en trop ne donnent pas de coût négatif
    Resources big_bonus = {5, 5, 5, 5, 5};
    assert(total(tokens_to_pay(big_bonus, card)) == 0);
}

static void test_check_pick()
{
    Resources bank = {4, 4, 4, 4, 4};
    assert(check_pick({1, 1, 1, 0, 0}, bank).empty());
    assert(check_pick({2, 0, 0, 0, 0}, bank).empty());
    assert(!check_pick({1, 1, 0, 0, 0}, bank).empty());   // 3 couleurs disponibles
    assert(!check_pick({2, 1, 0, 0, 0}, bank).empty());
    assert(!check_pick({0, 0, 0, 0, 0}, bank).empty());
    assert(!check_pick({1, 1, 1, 1, 0}, bank).empty());

    Resources small = {3, 4, 0, 0, 0};
    assert(!check_pick({2, 0, 0, 0, 0}, small).empty());  // pile de 3 < 4
    assert(check_pick({0, 2, 0, 0, 0}, small).empty());
    assert(check_pick({1, 1, 0, 0, 0}, small).empty());   // seulement 2 couleurs restantes
    assert(!check_pick({1, 0, 0, 0, 0}, small).empty());
    assert(!check_pick({0, 0, 1, 0, 0}, small).empty());  // couleur vide
}

static void test_noble()
{
    Noble noble = {3, {4, 4, 0, 0, 0}, "Machiavel"};
    assert(noble_can_visit({4, 4, 0, 0, 0}, noble));
    assert(!noble_can_visit({4, 3, 9, 9, 9}, noble));
}

static void test_game()
{
    Game game({"A", "B"}, 42, Mode::Quick);
    assert(game.nobles().size() == 3);
    assert(game.bank()[0] == 4);
    for (int level = 1; level <= 3; level++) {
        assert(game.visible(level).size() == 4);
    }
    assert(game.deck_size(1) == 36);

    // Prise de jetons
    assert(game.take_tokens({1, 1, 1, 0, 0}).empty());
    assert(game.current_player().tokens[0] == 1 && game.bank()[0] == 3);
    game.end_turn();
    assert(game.current_index() == 1);
    assert(!game.take_tokens({2, 0, 0, 0, 0}).empty());  // plus que 3 blancs
    game.end_turn();
    assert(game.current_index() == 0 && game.round() == 2);

    // Défausse au-delà de 10 jetons
    Game rich({"A", "B", "C", "D"}, 1, Mode::Quick);
    for (int i = 0; i < 3; i++) {
        assert(rich.take_tokens({2, 0, 0, 0, 0}).empty() || rich.take_tokens({0, 2, 0, 0, 0}).empty());
    }
    assert(rich.take_tokens({0, 0, 2, 0, 0}).empty());
    assert(rich.take_tokens({0, 0, 0, 2, 0}).empty());
    assert(total(rich.current_player().tokens) == 10);
    assert(!rich.must_discard());
    assert(rich.take_tokens({0, 0, 0, 0, 2}).empty());
    assert(rich.must_discard());
    assert(rich.discard(Color::Black).empty());
    assert(rich.discard(Color::Black).empty());
    assert(!rich.must_discard());
    assert(!rich.discard(Color::Black).empty());

    // Achat : avec tous ces jetons, on peut acheter une carte de niveau 1
    int bought_level = 0;
    int bought_position = 0;
    for (int p = 1; p <= 4 && bought_level == 0; p++) {
        if (rich.can_buy(1, p)) {
            bought_level = 1;
            bought_position = p;
        }
    }
    assert(bought_level == 1);
    int tokens_before = total(rich.current_player().tokens) + total(rich.bank());
    assert(rich.buy_card(bought_level, bought_position).empty());
    assert(rich.current_player().cards.size() == 1);
    assert(rich.visible(1).size() == 4);
    assert(rich.deck_size(1) == 35);
    assert(total(rich.current_player().tokens) + total(rich.bank()) == tokens_before);
    assert(!rich.buy_card(4, 1).empty());

    // Pas de réservation ni d'or en partie rapide
    assert(game.bank_gold() == 0);
    assert(!game.can_reserve());
    assert(!game.reserve_card(1, 1).empty());
}

static void test_classic()
{
    Game game({"A", "B"}, 3, Mode::Classic);
    assert(game.bank_gold() == GOLD_TOKENS);
    assert(game.can_reserve());

    // Réserver une carte visible : elle est remplacée, on gagne 1 or
    Card wanted = game.visible(2)[0];
    assert(game.reserve_card(2, 1).empty());
    assert(game.current_player().reserved.size() == 1);
    assert(game.current_player().reserved[0].cost == wanted.cost);
    assert(game.current_player().gold == 1 && game.bank_gold() == GOLD_TOKENS - 1);
    assert(game.visible(2).size() == 4 && game.deck_size(2) == 25);

    // Réserver le dessus de la pioche, puis une 3e carte, puis refus de la 4e
    assert(game.reserve_card(3, 0).empty());
    assert(game.deck_size(3) == 15);
    assert(game.reserve_card(1, 4).empty());
    assert(game.current_player().gold == 3);
    assert(!game.can_reserve());
    assert(!game.reserve_card(1, 1).empty());
    assert(!game.reserve_card(9, 1).empty());

    // L'or sert de joker : une carte de niveau 1 coûte au plus 5 jetons,
    // avec 3 or et 2 jetons de couleurs adaptées on peut payer.
    Game joker({"A", "B"}, 5, Mode::Classic);
    for (int i = 0; i < 3; i++) {
        assert(joker.reserve_card(3, 0).empty());
    }
    Card card = joker.visible(1)[0];
    int missing = missing_tokens({0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, card);
    assert(joker.can_buy(1, 1) == (missing <= 3));
    if (missing <= 3) {
        assert(joker.buy_card(1, 1).empty());
        assert(joker.current_player().gold == 3 - missing);
        assert(joker.bank_gold() == GOLD_TOKENS - 3 + missing);
    }

    // Défausse de l'or
    assert(joker.discard_gold().empty() || joker.current_player().gold == 0);
}

int main()
{
    test_data();
    test_tokens_to_pay();
    test_check_pick();
    test_noble();
    test_game();
    test_classic();
    std::cout << "Tous les tests passent.\n";
    return 0;
}
