#include "game.hpp"

#include <algorithm>

#include "data.hpp"
#include "rules.hpp"

// ---------------------------------------------------------------- Player

int Player::score() const
{
    int points = ::score(cards);
    for (const Noble& noble : nobles) {
        points += noble.points;
    }
    return points;
}

Resources Player::bonuses() const
{
    return ::bonuses(cards);
}

int Player::token_count() const
{
    return total(tokens) + gold;
}

// ---------------------------------------------------------------- Création

Game::Game(const std::vector<std::string>& names, unsigned seed, Mode mode)
    : mode_(mode)
{
    std::mt19937 random(seed);

    for (const std::string& name : names) {
        Player player;
        player.name = name;
        players_.push_back(player);
    }

    int count = tokens_per_color(static_cast<int>(names.size()));
    bank_ = {count, count, count, count, count};
    if (mode_ == Mode::Classic) {
        bank_gold_ = GOLD_TOKENS;
    }

    // Chaque niveau : on mélange la pioche puis on retourne 4 cartes
    for (int l = 0; l < LEVEL_COUNT; l++) {
        decks_[l] = cards_of_level(l + 1);
        std::shuffle(decks_[l].begin(), decks_[l].end(), random);
        for (int i = 0; i < VISIBLE_PER_LEVEL && !decks_[l].empty(); i++) {
            visible_[l].push_back(decks_[l].back());
            decks_[l].pop_back();
        }
    }

    // Nobles : nombre de joueurs + 1, tirés au hasard
    nobles_ = all_nobles();
    std::shuffle(nobles_.begin(), nobles_.end(), random);
    nobles_.resize(names.size() + 1);
}

// ---------------------------------------------------------------- Consultation

void Game::set_controller(int player, Controller controller)
{
    players_[player].controller = controller;
}

Mode Game::mode() const { return mode_; }
const std::vector<Player>& Game::players() const { return players_; }
const Player& Game::current_player() const { return players_[current_]; }
Player& Game::current() { return players_[current_]; }
int Game::current_index() const { return current_; }
int Game::round() const { return round_; }
const Resources& Game::bank() const { return bank_; }
int Game::bank_gold() const { return bank_gold_; }
const std::vector<Card>& Game::visible(int level) const { return visible_[level - 1]; }
int Game::deck_size(int level) const { return static_cast<int>(decks_[level - 1].size()); }
const std::vector<Noble>& Game::nobles() const { return nobles_; }
bool Game::is_last_round() const { return last_round_; }
TurnStep Game::step() const { return step_; }
bool Game::is_over() const { return step_ == TurnStep::Over; }
const std::vector<Event>& Game::events() const { return events_; }
const std::vector<int>& Game::noble_choices() const { return noble_choices_; }
bool Game::is_blocked() const { return consecutive_passes_ >= static_cast<int>(players_.size()); }

bool Game::valid_position(int level, int position) const
{
    if (level < 1 || level > LEVEL_COUNT) {
        return false;
    }
    return position >= 1 && position <= static_cast<int>(visible_[level - 1].size());
}

// ---------------------------------------------------------------- Possibilités

// Les jetons or complètent les jetons de couleur manquants
bool Game::can_afford(const Card& card) const
{
    const Player& player = current_player();
    return missing_tokens(player.bonuses(), player.tokens, card) <= player.gold;
}

bool Game::can_buy(int level, int position) const
{
    if (!valid_position(level, position)) {
        return false;
    }
    return can_afford(visible_[level - 1][position - 1]);
}

bool Game::can_buy_reserved(int index) const
{
    const std::vector<Card>& reserved = current_player().reserved;
    if (index < 1 || index > static_cast<int>(reserved.size())) {
        return false;
    }
    return can_afford(reserved[index - 1]);
}

bool Game::can_take_tokens() const
{
    return total(bank_) > 0;
}

bool Game::can_reserve() const
{
    if (mode_ != Mode::Classic) {
        return false;
    }
    if (static_cast<int>(current_player().reserved.size()) >= MAX_RESERVED) {
        return false;
    }
    for (int l = 0; l < LEVEL_COUNT; l++) {
        if (!visible_[l].empty() || !decks_[l].empty()) {
            return true;
        }
    }
    return false;
}

bool Game::can_play() const
{
    if (can_take_tokens() || can_reserve()) {
        return true;
    }
    for (int level = 1; level <= LEVEL_COUNT; level++) {
        for (int position = 1; position <= static_cast<int>(visible_[level - 1].size()); position++) {
            if (can_buy(level, position)) {
                return true;
            }
        }
    }
    for (int index = 1; index <= static_cast<int>(current_player().reserved.size()); index++) {
        if (can_buy_reserved(index)) {
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------- Actions

// Message d'erreur commun quand une action arrive à la mauvaise étape
static const char* const WRONG_STEP = "Ce n'est pas le moment de faire cette action.";

std::string Game::take_tokens(const Resources& wanted)
{
    if (step_ != TurnStep::Action) {
        return WRONG_STEP;
    }
    std::string error = check_pick(wanted, bank_);
    if (!error.empty()) {
        return error;
    }
    for (int c = 0; c < COLOR_COUNT; c++) {
        bank_[c] -= wanted[c];
        current().tokens[c] += wanted[c];
    }
    Event event(EventType::TakeTokens, current_);
    event.tokens = wanted;
    events_.push_back(event);
    after_action();
    return "";
}

// Paye la carte : jetons de couleur d'abord, puis or pour le reste.
// Les jetons dépensés retournent dans la réserve.
void Game::pay_for(const Card& card)
{
    Player& player = current();
    Resources to_pay = tokens_to_pay(player.bonuses(), card);
    for (int c = 0; c < COLOR_COUNT; c++) {
        int used = std::min(to_pay[c], player.tokens[c]);
        player.tokens[c] -= used;
        bank_[c] += used;
        int gold_used = to_pay[c] - used;
        player.gold -= gold_used;
        bank_gold_ += gold_used;
    }
}

// Retire la carte visible et la remplace par la pioche du même niveau si possible
Card Game::take_visible(int level, int position)
{
    std::vector<Card>& row = visible_[level - 1];
    std::vector<Card>& deck = decks_[level - 1];
    Card card = row[position - 1];
    if (deck.empty()) {
        row.erase(row.begin() + (position - 1));
    } else {
        row[position - 1] = deck.back();
        deck.pop_back();
    }
    return card;
}

std::string Game::buy_card(int level, int position)
{
    if (step_ != TurnStep::Action) {
        return WRONG_STEP;
    }
    if (!valid_position(level, position)) {
        return "Cette carte n'existe pas.";
    }
    if (!can_buy(level, position)) {
        return "Pas assez de ressources pour acheter cette carte.";
    }
    Card card = visible_[level - 1][position - 1];
    pay_for(card);
    take_visible(level, position);
    current().cards.push_back(card);
    Event event(EventType::BuyCard, current_);
    event.card = card;
    events_.push_back(event);
    after_action();
    return "";
}

std::string Game::buy_reserved(int index)
{
    if (step_ != TurnStep::Action) {
        return WRONG_STEP;
    }
    std::vector<Card>& reserved = current().reserved;
    if (index < 1 || index > static_cast<int>(reserved.size())) {
        return "Vous n'avez pas de carte réservée à cette position.";
    }
    if (!can_buy_reserved(index)) {
        return "Pas assez de ressources pour acheter cette carte.";
    }
    Card card = reserved[index - 1];
    pay_for(card);
    reserved.erase(reserved.begin() + (index - 1));
    current().cards.push_back(card);
    Event event(EventType::BuyReserved, current_);
    event.card = card;
    events_.push_back(event);
    after_action();
    return "";
}

std::string Game::reserve_card(int level, int position)
{
    if (step_ != TurnStep::Action) {
        return WRONG_STEP;
    }
    if (mode_ != Mode::Classic) {
        return "La réservation n'existe pas en partie rapide.";
    }
    if (static_cast<int>(current_player().reserved.size()) >= MAX_RESERVED) {
        return "Vous avez déjà 3 cartes réservées.";
    }
    if (level < 1 || level > LEVEL_COUNT) {
        return "Ce niveau n'existe pas.";
    }

    Card card = {0, Color::White, {0, 0, 0, 0, 0}};
    if (position == 0) {
        std::vector<Card>& deck = decks_[level - 1];
        if (deck.empty()) {
            return "La pioche de ce niveau est vide.";
        }
        card = deck.back();
        deck.pop_back();
    } else {
        if (!valid_position(level, position)) {
            return "Cette carte n'existe pas.";
        }
        card = take_visible(level, position);
    }

    current().reserved.push_back(card);
    Event event(EventType::Reserve, current_);
    event.card = card;
    event.from_deck = position == 0;
    // On gagne un jeton or s'il en reste
    if (bank_gold_ > 0) {
        bank_gold_--;
        current().gold++;
        event.gold = 1;
    }
    events_.push_back(event);
    after_action();
    return "";
}

// ---------------------------------------------------------------- Fin de tour

bool Game::must_discard() const
{
    return current_player().token_count() > MAX_TOKENS_IN_HAND;
}

int Game::tokens_to_discard() const
{
    int extra = current_player().token_count() - MAX_TOKENS_IN_HAND;
    return extra > 0 ? extra : 0;
}

std::string Game::discard(Color color)
{
    if (step_ != TurnStep::Discard) {
        return WRONG_STEP;
    }
    int c = index_of(color);
    if (current().tokens[c] == 0) {
        return "Vous n'avez pas de jeton de cette couleur.";
    }
    current().tokens[c]--;
    bank_[c]++;
    Event event(EventType::Discard, current_);
    event.tokens[c] = 1;
    events_.push_back(event);
    if (!must_discard()) {
        after_discard();
    }
    return "";
}

std::string Game::discard_gold()
{
    if (step_ != TurnStep::Discard) {
        return WRONG_STEP;
    }
    if (current().gold == 0) {
        return "Vous n'avez pas de jeton or.";
    }
    current().gold--;
    bank_gold_++;
    Event event(EventType::Discard, current_);
    event.gold = 1;
    events_.push_back(event);
    if (!must_discard()) {
        after_discard();
    }
    return "";
}

std::vector<int> Game::visiting_nobles() const
{
    std::vector<int> result;
    Resources bonus = current_player().bonuses();
    for (int i = 0; i < static_cast<int>(nobles_.size()); i++) {
        if (noble_can_visit(bonus, nobles_[i])) {
            result.push_back(i);
        }
    }
    return result;
}

void Game::receive_noble(int noble_index)
{
    Event event(EventType::NobleVisit, current_);
    event.noble = nobles_[noble_index].name;
    events_.push_back(event);
    current().nobles.push_back(nobles_[noble_index]);
    nobles_.erase(nobles_.begin() + noble_index);
}

std::string Game::choose_noble(int noble_index)
{
    if (step_ != TurnStep::ChooseNoble) {
        return WRONG_STEP;
    }
    for (int choice : noble_choices_) {
        if (choice == noble_index) {
            receive_noble(noble_index);
            noble_choices_.clear();
            next_player();
            return "";
        }
    }
    return "Ce noble ne peut pas vous rendre visite.";
}

// ---------------------------------------------------------------- Enchaînement du tour

void Game::after_action()
{
    consecutive_passes_ = 0;
    if (must_discard()) {
        step_ = TurnStep::Discard;
    } else {
        after_discard();
    }
}

void Game::after_discard()
{
    // Un seul noble par tour : il vient tout seul, ou le joueur choisit
    std::vector<int> candidates = visiting_nobles();
    if (candidates.size() > 1) {
        noble_choices_ = candidates;
        step_ = TurnStep::ChooseNoble;
        return;
    }
    if (candidates.size() == 1) {
        receive_noble(candidates[0]);
    }
    next_player();
}

void Game::advance_one()
{
    if (current_player().score() >= WINNING_SCORE) {
        last_round_ = true;
    }
    current_ = (current_ + 1) % static_cast<int>(players_.size());
    // Retour au premier joueur : le tour de table est terminé
    if (current_ == 0) {
        if (last_round_) {
            step_ = TurnStep::Over;
            return;
        }
        round_++;
    }
}

void Game::next_player()
{
    advance_one();
    // Ceux qui ne peuvent rien faire passent leur tour
    while (step_ != TurnStep::Over && !can_play()) {
        events_.push_back(Event(EventType::Pass, current_));
        consecutive_passes_++;
        // Partie bloquée : plus personne ne peut jouer
        if (is_blocked()) {
            step_ = TurnStep::Over;
            return;
        }
        advance_one();
    }
    if (step_ != TurnStep::Over) {
        step_ = TurnStep::Action;
    }
}

std::vector<int> Game::winners() const
{
    // Meilleur score, puis le moins de cartes achetées
    int best_score = -1;
    int fewest_cards = 0;
    for (const Player& player : players_) {
        int cards = static_cast<int>(player.cards.size());
        if (player.score() > best_score
            || (player.score() == best_score && cards < fewest_cards)) {
            best_score = player.score();
            fewest_cards = cards;
        }
    }

    std::vector<int> result;
    for (int i = 0; i < static_cast<int>(players_.size()); i++) {
        if (players_[i].score() == best_score
            && static_cast<int>(players_[i].cards.size()) == fewest_cards) {
            result.push_back(i);
        }
    }
    return result;
}
