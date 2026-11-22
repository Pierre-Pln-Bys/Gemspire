// Tests de la logique du jeu (rules et game). Lancer avec : make test
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <iostream>
#include <random>

#include "../src/ai.hpp"
#include "../src/data.hpp"
#include "../src/elo.hpp"
#include "../src/game.hpp"
#include "../src/rules.hpp"
#include "../src/save.hpp"

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
    Card card = {0, Gem::Red, {0, 2, 2, 0, 0}};
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

// Accès aux données privées de Game pour préparer des situations précises
struct GameTester {
    static Player& player(Game& game, int index) { return game.players_[index]; }
    static Resources& bank(Game& game) { return game.bank_; }
    static int& bank_gold(Game& game) { return game.bank_gold_; }
    static std::vector<Noble>& nobles(Game& game) { return game.nobles_; }
    static std::vector<Card>& visible(Game& game, int level) { return game.visible_[level - 1]; }
    static std::vector<Card>& deck(Game& game, int level) { return game.decks_[level - 1]; }
};

// Nombre total de jetons (réserve + mains), qui ne doit jamais changer
static int all_tokens(const Game& game)
{
    int sum = total(game.bank()) + game.bank_gold();
    for (const Player& player : game.players()) {
        sum += player.token_count();
    }
    return sum;
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
    assert(game.step() == TurnStep::Action);

    // Une action fait passer au joueur suivant, et le tour de table compte
    assert(game.take_tokens({1, 1, 1, 0, 0}).empty());
    assert(game.players()[0].tokens[0] == 1 && game.bank()[0] == 3);
    assert(game.current_index() == 1 && game.round() == 1);
    assert(!game.take_tokens({2, 0, 0, 0, 0}).empty());  // plus que 3 blancs
    assert(game.current_index() == 1);                   // erreur : on ne change pas de joueur
    assert(game.take_tokens({0, 0, 0, 2, 0}).empty());
    assert(game.current_index() == 0 && game.round() == 2);
    assert(game.events().size() == 2 && game.events()[1].type == EventType::TakeTokens);

    // Achat : coût payé, carte remplacée, jetons conservés
    Game shop({"A", "B"}, 1, Mode::Quick);
    GameTester::player(shop, 0).tokens = {4, 4, 4, 4, 4};
    GameTester::bank(shop) = {0, 0, 0, 0, 0};
    int before = all_tokens(shop);
    assert(shop.can_buy(1, 1));
    assert(shop.buy_card(1, 1).empty());
    assert(shop.players()[0].cards.size() == 1);
    assert(shop.visible(1).size() == 4 && shop.deck_size(1) == 35);
    assert(all_tokens(shop) == before);
    assert(!shop.buy_card(4, 1).empty());

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
    const Player& a = game.players()[0];
    assert(a.reserved.size() == 1 && a.reserved[0].cost == wanted.cost);
    assert(a.gold == 1 && game.bank_gold() == GOLD_TOKENS - 1);
    assert(game.visible(2).size() == 4 && game.deck_size(2) == 25);
    assert(game.events().back().type == EventType::Reserve && game.events().back().gold == 1);

    // B réserve le dessus d'une pioche, puis A une 2e et une 3e carte : refus de la 4e
    assert(game.reserve_card(3, 0).empty());
    assert(game.deck_size(3) == 15 && game.events().back().from_deck);
    assert(game.reserve_card(1, 4).empty());
    assert(game.take_tokens({1, 1, 1, 0, 0}).empty());
    assert(game.reserve_card(1, 1).empty());
    assert(game.players()[0].reserved.size() == 3 && game.players()[0].gold == 3);
    assert(game.take_tokens({1, 1, 1, 0, 0}).empty());
    assert(!game.can_reserve());
    assert(!game.reserve_card(1, 1).empty());
    assert(!game.reserve_card(9, 1).empty());

    // L'or sert de joker : 2 jetons manquants payés avec 2 or
    Game joker({"A", "B"}, 5, Mode::Classic);
    Card card = {1, Gem::Red, {2, 2, 0, 0, 0}};
    GameTester::visible(joker, 1)[0] = card;
    GameTester::player(joker, 0).tokens = {1, 1, 0, 0, 0};
    GameTester::player(joker, 0).gold = 2;
    GameTester::bank_gold(joker) = 3;
    assert(joker.can_buy(1, 1));
    assert(joker.buy_card(1, 1).empty());
    assert(joker.players()[0].gold == 0 && joker.bank_gold() == 5);
    assert(joker.players()[0].tokens[0] == 0 && joker.players()[0].tokens[1] == 0);

    // Une réservée peut être achetée par son propriétaire
    Game own({"A", "B"}, 6, Mode::Classic);
    GameTester::player(own, 0).reserved.push_back({2, Gem::Blue, {1, 0, 0, 0, 0}});
    GameTester::player(own, 0).tokens = {1, 0, 0, 0, 0};
    assert(own.can_buy_reserved(1));
    assert(own.buy_reserved(1).empty());
    assert(own.players()[0].cards.size() == 1 && own.players()[0].reserved.empty());
    assert(own.events().back().type == EventType::BuyReserved);
}

static void test_turn_flow()
{
    // Défausse obligatoire au-delà de 10 jetons, or compris
    Game game({"A", "B"}, 7, Mode::Classic);
    GameTester::player(game, 0).tokens = {2, 2, 2, 2, 0};
    GameTester::player(game, 0).gold = 1;
    assert(game.take_tokens({0, 0, 1, 1, 1}).empty());
    assert(game.step() == TurnStep::Discard && game.current_index() == 0);
    assert(game.tokens_to_discard() == 2);
    assert(!game.take_tokens({1, 1, 1, 0, 0}).empty());   // pas d'action pendant la défausse
    assert(game.discard_gold().empty());
    assert(game.step() == TurnStep::Discard);
    assert(!game.discard_gold().empty());                 // plus d'or
    assert(game.discard(Gem::Red).empty());
    assert(game.step() == TurnStep::Action && game.current_index() == 1);
    assert(game.players()[0].token_count() == 10);

    // Un seul noble possible : il vient tout seul
    Game single({"A", "B"}, 8, Mode::Quick);
    std::vector<Noble>& nobles = GameTester::nobles(single);
    nobles[0].cost = {1, 0, 0, 0, 0};
    nobles[1].cost = {4, 4, 0, 0, 0};
    nobles[2].cost = {4, 4, 0, 0, 0};
    GameTester::player(single, 0).cards.push_back({0, Gem::White, {0, 0, 0, 0, 0}});
    assert(single.take_tokens({1, 1, 1, 0, 0}).empty());
    assert(single.players()[0].nobles.size() == 1 && single.players()[0].score() == 3);
    assert(single.nobles().size() == 2 && single.current_index() == 1);

    // Deux nobles possibles : le joueur choisit, un seul par tour
    Game two({"A", "B"}, 9, Mode::Quick);
    GameTester::nobles(two)[0].cost = {1, 0, 0, 0, 0};
    GameTester::nobles(two)[1].cost = {0, 1, 0, 0, 0};
    GameTester::nobles(two)[2].cost = {4, 4, 4, 0, 0};
    GameTester::player(two, 0).cards.push_back({0, Gem::White, {0, 0, 0, 0, 0}});
    GameTester::player(two, 0).cards.push_back({0, Gem::Blue, {0, 0, 0, 0, 0}});
    std::string second = two.nobles()[1].name;
    assert(two.take_tokens({0, 0, 1, 1, 1}).empty());
    assert(two.step() == TurnStep::ChooseNoble && two.noble_choices().size() == 2);
    assert(!two.choose_noble(2).empty());                 // pas dans les choix
    assert(two.choose_noble(1).empty());
    assert(two.players()[0].nobles.size() == 1 && two.players()[0].nobles[0].name == second);
    assert(two.step() == TurnStep::Action && two.current_index() == 1);

    // Dernier tour : B termine le tour de table après les 15 points de A
    Game last({"A", "B"}, 10, Mode::Quick);
    for (int i = 0; i < 5; i++) {
        GameTester::player(last, 0).cards.push_back({3, Gem::Red, {0, 0, 0, 0, 0}});
    }
    assert(last.take_tokens({1, 1, 1, 0, 0}).empty());
    assert(last.is_last_round() && !last.is_over());
    assert(last.take_tokens({1, 1, 1, 0, 0}).empty());
    assert(last.is_over() && last.step() == TurnStep::Over);
    assert(last.winners().size() == 1 && last.winners()[0] == 0);

    // Passes en chaîne et partie bloquée : réserve vide, personne ne peut acheter
    Game blocked({"A", "B", "C"}, 11, Mode::Quick);
    GameTester::bank(blocked) = {0, 0, 1, 0, 0};
    assert(blocked.take_tokens({0, 0, 1, 0, 0}).empty());
    // B et C ne peuvent rien faire, puis A non plus : partie bloquée
    assert(blocked.is_over() && blocked.is_blocked());
    int passes = 0;
    for (const Event& event : blocked.events()) {
        passes += event.type == EventType::Pass;
    }
    assert(passes == 3);
}

// Parties complètes jouées au hasard : elles doivent toutes se terminer,
// sans perdre de jeton et sans dépasser les limites.
static void test_random_games()
{
    std::mt19937 random(2026);
    for (int g = 0; g < 300; g++) {
        int count = 2 + g % 3;
        std::vector<std::string> names;
        for (int i = 0; i < count; i++) {
            names.push_back("J" + std::to_string(i));
        }
        Game game(names, g, g % 2 ? Mode::Classic : Mode::Quick);
        int start = all_tokens(game);
        int moves = 0;
        while (!game.is_over() && moves < 20000) {
            moves++;
            int choice = static_cast<int>(random() % 6);
            if (game.step() == TurnStep::Discard) {
                if (game.discard(color_at(choice % COLOR_COUNT)).empty() || game.discard_gold().empty()) {
                    continue;
                }
            } else if (game.step() == TurnStep::ChooseNoble) {
                assert(game.choose_noble(game.noble_choices()[0]).empty());
            } else {
                // Achète la première carte possible (réservées d'abord), sinon
                // réserve parfois, sinon prend des jetons de couleurs différentes
                bool done = false;
                for (int r = 1; r <= 3 && !done; r++) {
                    done = game.can_buy_reserved(r) && game.buy_reserved(r).empty();
                }
                for (int level = 3; level >= 1 && !done; level--) {
                    for (int position = 1; position <= 4 && !done; position++) {
                        done = game.can_buy(level, position) && game.buy_card(level, position).empty();
                    }
                }
                if (!done && choice == 0 && game.can_reserve()) {
                    done = game.reserve_card(1 + static_cast<int>(random() % 3), static_cast<int>(random() % 5)).empty();
                }
                if (!done) {
                    Resources wanted = {0, 0, 0, 0, 0};
                    int available = 0;
                    for (int c = 0; c < COLOR_COUNT; c++) {
                        available += game.bank()[c] > 0;
                    }
                    int needed = available < 3 ? available : 3;
                    for (int c = choice % COLOR_COUNT, k = 0; k < COLOR_COUNT && needed > 0; k++, c = (c + 1) % COLOR_COUNT) {
                        if (game.bank()[c] > 0) {
                            wanted[c] = 1;
                            needed--;
                        }
                    }
                    done = game.take_tokens(wanted).empty();
                }
                if (!done && game.can_reserve()) {
                    game.reserve_card(1, 0);
                }
            }
            assert(all_tokens(game) == start);
            for (const Player& player : game.players()) {
                assert(player.reserved.size() <= 3 && player.gold >= 0);
                for (int c = 0; c < COLOR_COUNT; c++) {
                    assert(player.tokens[c] >= 0 && game.bank()[c] >= 0);
                }
            }
        }
        assert(game.is_over());
    }
}

// Tournoi : 1 000 parties entre ordinateurs. Aucune ne doit planter ni
// perdre de jeton, et l'IA moyenne doit battre nettement l'IA facile.
static void test_ai_tournament()
{
    int medium_wins = 0;
    int easy_wins = 0;
    for (int g = 0; g < 1000; g++) {
        int count = 2 + g % 3;
        std::vector<std::string> names;
        for (int i = 0; i < count; i++) {
            names.push_back("IA" + std::to_string(i));
        }
        Game game(names, 1000 + g, g % 2 ? Mode::Classic : Mode::Quick);
        // Moyenne et facile en alternance, la première place change à chaque partie
        for (int i = 0; i < count; i++) {
            game.set_controller(i, (i + g) % 2 ? Controller::AiMedium : Controller::AiEasy);
        }
        int start = all_tokens(game);
        int moves = 0;
        while (!game.is_over()) {
            assert(ai_to_play(game));
            assert(ai_play(game).empty());
            assert(all_tokens(game) == start);
            assert(++moves < 5000);
        }
        if (game.is_blocked()) {
            continue;
        }
        std::vector<int> winners = game.winners();
        if (winners.size() == 1) {
            if (game.players()[winners[0]].controller == Controller::AiMedium) {
                medium_wins++;
            } else {
                easy_wins++;
            }
        }
    }
    std::cout << "Tournoi : IA moyenne " << medium_wins << " victoires, IA facile " << easy_wins << std::endl;
    assert(medium_wins > easy_wins * 3 / 2);
}

// Sauvegarde : aller-retour identique, et une partie rechargée se termine
// exactement comme l'originale
static void test_save()
{
    for (int g = 0; g < 50; g++) {
        Game game({"Alice Martin", "Bob", "Chloé"}, 500 + g, g % 2 ? Mode::Classic : Mode::Quick);
        for (int i = 0; i < 3; i++) {
            game.set_controller(i, i % 2 ? Controller::AiMedium : Controller::AiEasy);
        }
        // On avance un peu la partie, puis on sauvegarde
        for (int step = 0; step < 10 + g && !game.is_over(); step++) {
            assert(ai_play(game).empty());
        }
        std::string text = save_game(game);
        std::unique_ptr<Game> copy;
        assert(load_game(text, copy).empty());
        assert(save_game(*copy) == text);
        assert(copy->players()[0].name == "Alice Martin");

        // Les deux parties continuent avec les mêmes coups
        while (!game.is_over()) {
            assert(ai_play(game).empty());
            assert(ai_play(*copy).empty());
        }
        assert(copy->is_over());
        assert(save_game(*copy) == save_game(game));
    }

    // Fichiers invalides : un message d'erreur, pas de plantage
    std::unique_ptr<Game> broken;
    assert(!load_game("", broken).empty());
    assert(!load_game("GEMSPIRE 2\n", broken).empty());
    Game game({"A", "B"}, 1, Mode::Quick);
    std::string text = save_game(game);
    assert(!load_game(text.substr(0, text.size() / 2), broken).empty());
    assert(!broken);

    // Fichier sur le disque
    assert(save_game_to_file(game, "tests/test.splendor").empty());
    std::unique_ptr<Game> from_file;
    assert(load_game_from_file("tests/test.splendor", from_file).empty());
    assert(save_game(*from_file) == text);
    std::remove("tests/test.splendor");
    assert(!load_game_from_file("tests/absent.splendor", from_file).empty());
}

// Temps écoulé et abandon (parties en ligne)
static void test_timeout_and_forfeit()
{
    // Temps écoulé : le joueur passe, on change de joueur
    Game game({"A", "B", "C"}, 5, Mode::Quick);
    assert(game.skip_turn().empty());
    assert(game.current_index() == 1);
    assert(game.events().back().type == EventType::Timeout);

    // B abandonne pendant son tour : C joue ensuite, A puis C, B est sauté
    assert(game.forfeit(1).empty());
    assert(game.players()[1].forfeited);
    assert(game.current_index() == 2);
    assert(game.active_players() == 2);
    assert(game.take_tokens({1, 1, 1, 0, 0}).empty());
    assert(game.current_index() == 0);
    assert(game.take_tokens({1, 1, 1, 0, 0}).empty());
    assert(game.current_index() == 2);

    // B a beaucoup de points mais il est classé dernier, et ne gagne pas
    GameTester::player(game, 1).nobles = {all_nobles()[0], all_nobles()[1], all_nobles()[2]};
    std::vector<int> places = final_places(game);
    assert(places[1] == 2);
    std::vector<int> win = game.winners();
    assert(std::find(win.begin(), win.end(), 1) == win.end());

    // La sauvegarde garde l'abandon
    std::unique_ptr<Game> copy;
    assert(load_game(save_game(game), copy).empty());
    assert(copy->players()[1].forfeited && !copy->players()[0].forfeited);

    // Il ne reste qu'un joueur : la partie est finie, il gagne
    assert(game.forfeit(0).empty());
    assert(game.is_over());
    assert(game.winners() == std::vector<int>{2});

    // Deux joueurs : un abandon termine la partie tout de suite
    Game duel({"A", "B"}, 6, Mode::Classic);
    assert(duel.forfeit(0).empty());
    assert(duel.is_over() && duel.winners() == std::vector<int>{1});
    assert(final_places(duel)[0] == 1 && final_places(duel)[1] == 0);
}

int main()
{
    test_data();
    test_tokens_to_pay();
    test_check_pick();
    test_noble();
    test_game();
    test_classic();
    test_turn_flow();
    test_random_games();
    test_ai_tournament();
    test_save();
    test_timeout_and_forfeit();
    std::cout << "Tous les tests passent.\n";
    return 0;
}
