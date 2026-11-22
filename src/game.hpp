#ifndef SPLENDOR_GAME_HPP
#define SPLENDOR_GAME_HPP

#include <array>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "types.hpp"

const int LEVEL_COUNT = 3;
const int VISIBLE_PER_LEVEL = 4;
const int MAX_RESERVED = 3;   // cartes réservées au maximum (mode classique)
const int GOLD_TOKENS = 5;    // jetons or dans la réserve (mode classique)

// Mode de jeu
enum class Mode {
    Quick,    // partie rapide : sans or ni réservation
    Classic   // règles classiques : avec or et réservation
};

// Qui joue pour ce joueur : une personne ou l'ordinateur (voir ai.hpp)
enum class Controller { Human, AiEasy, AiMedium };

// Un joueur et tout ce qu'il possède
struct Player {
    std::string name;
    Controller controller = Controller::Human;
    Resources tokens = {0, 0, 0, 0, 0};
    int gold = 0;                 // jetons or (joker)
    std::vector<Card> cards;      // cartes achetées
    std::vector<Card> reserved;   // cartes réservées (mode classique)
    std::vector<Noble> nobles;    // nobles reçus
    bool forfeited = false;       // a abandonné (partie en ligne) : ne joue plus, classé dernier

    int score() const;
    Resources bonuses() const;
    int token_count() const;      // tous les jetons en main, or compris
};

// Étape du tour en cours. Game enchaîne lui-même les étapes :
// action -> défausse si plus de 10 jetons -> choix du noble s'il y en a
// plusieurs -> joueur suivant (ceux qui ne peuvent rien faire passent).
enum class TurnStep {
    Action,       // le joueur courant choisit son action
    Discard,      // il doit défausser jusqu'à 10 jetons
    ChooseNoble,  // plusieurs nobles peuvent venir : il en choisit un
    Over          // partie terminée
};

// Ce qui s'est passé pendant la partie (pour l'historique et les messages)
enum class EventType { TakeTokens, BuyCard, BuyReserved, Reserve, Discard, NobleVisit, Pass, Timeout, Forfeit };

struct Event {
    Event(EventType event_type, int event_player) : type(event_type), player(event_player) {}

    EventType type;
    int player;                                  // indice du joueur concerné
    Resources tokens = {0, 0, 0, 0, 0};          // jetons pris ou défaussés
    int gold = 0;                                // or reçu (réservation) ou défaussé
    Card card = {0, Gem::White, {0, 0, 0, 0, 0}};  // carte achetée ou réservée
    bool from_deck = false;                      // réservation sur le dessus d'une pioche
    std::string noble;                           // nom du noble en visite
};

// L'état complet d'une partie et les actions possibles pendant un tour.
// Les méthodes qui peuvent échouer retournent une chaîne vide en cas de
// succès, sinon un message d'erreur. Aucune entrée/sortie dans cette classe.
class Game {
public:
    // `names` : 2 à 4 noms de joueurs. `seed` : graine du mélange.
    Game(const std::vector<std::string>& names, unsigned seed, Mode mode);

    // Choisit qui joue pour le joueur `player` (humain par défaut)
    void set_controller(int player, Controller controller);

    // --- Consultation de l'état ---
    Mode mode() const;
    const std::vector<Player>& players() const;
    const Player& current_player() const;
    int current_index() const;
    int round() const;                              // numéro du tour de table (à partir de 1)
    const Resources& bank() const;                  // jetons de couleur de la réserve
    int bank_gold() const;                          // jetons or de la réserve
    const std::vector<Card>& visible(int level) const;  // cartes visibles, level de 1 à 3
    int deck_size(int level) const;                 // cartes restantes dans la pioche
    const std::vector<Noble>& nobles() const;       // nobles encore disponibles
    bool is_last_round() const;                     // quelqu'un a atteint 15 points
    TurnStep step() const;                          // étape du tour en cours
    bool is_over() const;
    const std::vector<Event>& events() const;       // historique de la partie
    bool is_blocked() const;                        // tous les joueurs ont passé d'affilée

    // --- Ce que le joueur courant peut faire ---
    bool can_buy(int level, int position) const;    // position de 1 à 4
    bool can_buy_reserved(int index) const;         // index de 1 à 3
    bool can_take_tokens() const;
    bool can_reserve() const;
    bool can_play() const;                          // faux => il passe son tour

    // --- Actions du joueur courant (étape Action) ---
    std::string take_tokens(const Resources& wanted);
    std::string buy_card(int level, int position);
    std::string buy_reserved(int index);
    // Réserve la carte visible `position` (1 à 4), ou le dessus de la pioche si position vaut 0
    std::string reserve_card(int level, int position);

    // --- Étape Discard ---
    bool must_discard() const;                      // plus de 10 jetons en main
    int tokens_to_discard() const;                  // combien il faut encore en défausser
    std::string discard(Gem color);
    std::string discard_gold();

    // --- Étape ChooseNoble ---
    const std::vector<int>& noble_choices() const;  // indices (dans nobles()) des nobles possibles
    std::string choose_noble(int noble_index);

    // --- Parties en ligne (décidé par le serveur, pas par le joueur) ---
    // Temps écoulé : le joueur courant passe son tour (étape Action seulement)
    std::string skip_turn();
    // Abandon du joueur `player` : il ne joue plus et sera classé dernier. Si
    // c'était son tour (étape Action), on passe au suivant. S'il ne reste
    // qu'un joueur, la partie se termine.
    std::string forfeit(int player);
    int active_players() const;                     // joueurs qui n'ont pas abandonné

    // Indices des gagnants (plusieurs en cas d'égalité parfaite)
    std::vector<int> winners() const;

private:
    Mode mode_;
    std::vector<Player> players_;
    Resources bank_;
    int bank_gold_ = 0;
    std::array<std::vector<Card>, LEVEL_COUNT> decks_;    // pioches (on tire à la fin)
    std::array<std::vector<Card>, LEVEL_COUNT> visible_;  // cartes face visible
    std::vector<Noble> nobles_;
    int current_ = 0;
    int round_ = 1;
    bool last_round_ = false;
    TurnStep step_ = TurnStep::Action;
    int consecutive_passes_ = 0;  // nombre de joueurs qui ont passé d'affilée
    std::vector<int> noble_choices_;
    std::vector<Event> events_;

    // Les tests peuvent préparer des situations précises
    friend struct GameTester;
    // La sauvegarde lit et écrit tout l'état (voir save.hpp)
    friend std::string write_game(const Game& game, int viewer);
    friend std::string load_game(const std::string& text, std::unique_ptr<Game>& game);

    Player& current();
    bool valid_position(int level, int position) const;
    bool can_afford(const Card& card) const;
    void pay_for(const Card& card);
    Card take_visible(int level, int position);   // retire la carte et la remplace
    std::vector<int> visiting_nobles() const;
    void receive_noble(int noble_index);
    void after_action();      // action faite : défausse, puis nobles, puis joueur suivant
    void after_discard();     // défausse finie : nobles, puis joueur suivant
    void advance_one();       // passe au joueur suivant (fin de tour de table, dernier tour)
    void next_player();       // idem, en faisant passer ceux qui ne peuvent rien faire
};

#endif
