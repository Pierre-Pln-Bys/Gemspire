#ifndef SPLENDOR_GAME_HPP
#define SPLENDOR_GAME_HPP

#include <array>
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

// Un joueur et tout ce qu'il possède
struct Player {
    std::string name;
    Resources tokens = {0, 0, 0, 0, 0};
    int gold = 0;                 // jetons or (joker)
    std::vector<Card> cards;      // cartes achetées
    std::vector<Card> reserved;   // cartes réservées (mode classique)
    std::vector<Noble> nobles;    // nobles reçus

    int score() const;
    Resources bonuses() const;
    int token_count() const;      // tous les jetons en main, or compris
};

// L'état complet d'une partie et les actions possibles pendant un tour.
// Les méthodes qui peuvent échouer retournent une chaîne vide en cas de
// succès, sinon un message d'erreur. Aucune entrée/sortie dans cette classe.
class Game {
public:
    // `names` : 2 à 4 noms de joueurs. `seed` : graine du mélange.
    Game(const std::vector<std::string>& names, unsigned seed, Mode mode);

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
    bool is_over() const;
    bool is_blocked() const;                        // tous les joueurs ont passé d'affilée

    // --- Ce que le joueur courant peut faire ---
    bool can_buy(int level, int position) const;    // position de 1 à 4
    bool can_buy_reserved(int index) const;         // index de 1 à 3
    bool can_take_tokens() const;
    bool can_reserve() const;
    bool can_play() const;                          // faux => il passe son tour

    // --- Actions du joueur courant ---
    std::string take_tokens(const Resources& wanted);
    std::string buy_card(int level, int position);
    std::string buy_reserved(int index);
    // Réserve la carte visible `position` (1 à 4), ou le dessus de la pioche si position vaut 0
    std::string reserve_card(int level, int position);

    // --- Fin de tour ---
    bool must_discard() const;                      // plus de 10 jetons en main
    std::string discard(Color color);
    std::string discard_gold();
    std::vector<int> visiting_nobles() const;       // indices des nobles qui peuvent venir
    void receive_noble(int noble_index);
    void pass_turn();                               // le joueur ne peut rien faire
    void end_turn();                                // passe au joueur suivant

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
    bool over_ = false;
    int consecutive_passes_ = 0;  // nombre de joueurs qui ont passé d'affilée

    Player& current();
    bool valid_position(int level, int position) const;
    bool can_afford(const Card& card) const;
    void pay_for(const Card& card);
    Card take_visible(int level, int position);   // retire la carte et la remplace
};

#endif
