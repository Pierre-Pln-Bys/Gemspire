#include "save.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

// Exemple de sauvegarde (une information par ligne) :
//
//   SPLENDOR 1
//   mode classique
//   round 3
//   current 1
//   step action
//   last_round 0
//   passes 0
//   bank 4 4 3 4 4 gold 4
//   players 2
//   name Alice
//   controller human
//   tokens 1 0 1 0 0 gold 1
//   cards 1
//   card 0 2 1 0 0 2 1          (points, couleur fournie, coût des 5 couleurs)
//   reserved 0
//   nobles 0
//   ... (joueur suivant)
//   deck 1 35
//   card ...
//   visible 1 4
//   card ...
//   (idem niveaux 2 et 3)
//   board_nobles 3
//   noble 3 4 4 0 0 0 Machiavel  (points, coût des 5 couleurs, nom)
//   choices 0
//   end

static const char* const STEP_NAMES[] = {"action", "discard", "choose_noble", "over"};
static const char* const CONTROLLER_NAMES[] = {"human", "ai_easy", "ai_medium"};

// ---------------------------------------------------------------- Écriture

static void write_resources(std::ostringstream& out, const Resources& resources)
{
    for (int c = 0; c < COLOR_COUNT; c++) {
        out << " " << resources[c];
    }
}

static void write_card(std::ostringstream& out, const Card& card)
{
    out << "card " << card.points << " " << index_of(card.provides);
    write_resources(out, card.cost);
    out << "\n";
}

static void write_cards(std::ostringstream& out, const std::string& label, const std::vector<Card>& cards)
{
    out << label << " " << cards.size() << "\n";
    for (const Card& card : cards) {
        write_card(out, card);
    }
}

static void write_noble(std::ostringstream& out, const Noble& noble)
{
    out << "noble " << noble.points;
    write_resources(out, noble.cost);
    out << " " << noble.name << "\n";
}

std::string save_game(const Game& game)
{
    std::ostringstream out;
    out << "SPLENDOR 1\n";
    out << "mode " << (game.mode_ == Mode::Classic ? "classique" : "rapide") << "\n";
    out << "round " << game.round_ << "\n";
    out << "current " << game.current_ << "\n";
    out << "step " << STEP_NAMES[static_cast<int>(game.step_)] << "\n";
    out << "last_round " << (game.last_round_ ? 1 : 0) << "\n";
    out << "passes " << game.consecutive_passes_ << "\n";
    out << "bank";
    write_resources(out, game.bank_);
    out << " gold " << game.bank_gold_ << "\n";

    out << "players " << game.players_.size() << "\n";
    for (const Player& player : game.players_) {
        out << "name " << player.name << "\n";
        out << "controller " << CONTROLLER_NAMES[static_cast<int>(player.controller)] << "\n";
        out << "tokens";
        write_resources(out, player.tokens);
        out << " gold " << player.gold << "\n";
        write_cards(out, "cards", player.cards);
        write_cards(out, "reserved", player.reserved);
        out << "nobles " << player.nobles.size() << "\n";
        for (const Noble& noble : player.nobles) {
            write_noble(out, noble);
        }
    }

    for (int l = 0; l < LEVEL_COUNT; l++) {
        write_cards(out, "deck " + std::to_string(l + 1), game.decks_[l]);
        write_cards(out, "visible " + std::to_string(l + 1), game.visible_[l]);
    }
    out << "board_nobles " << game.nobles_.size() << "\n";
    for (const Noble& noble : game.nobles_) {
        write_noble(out, noble);
    }
    out << "choices " << game.noble_choices_.size();
    for (int choice : game.noble_choices_) {
        out << " " << choice;
    }
    out << "\nend\n";
    return out.str();
}

// ---------------------------------------------------------------- Lecture

// Lit le texte mot par mot ; une erreur lève une exception avec un message
struct Reader {
    std::istringstream in;

    explicit Reader(const std::string& text) : in(text) {}

    void fail(const std::string& message)
    {
        throw std::runtime_error("Sauvegarde invalide : " + message);
    }

    void expect(const std::string& word)
    {
        std::string read;
        if (!(in >> read) || read != word) {
            fail("« " + word + " » attendu");
        }
    }

    int number(int min, int max)
    {
        int value = 0;
        if (!(in >> value) || value < min || value > max) {
            fail("nombre attendu entre " + std::to_string(min) + " et " + std::to_string(max));
        }
        return value;
    }

    std::string word()
    {
        std::string read;
        if (!(in >> read)) {
            fail("fin de fichier inattendue");
        }
        return read;
    }

    // Le reste de la ligne (pour les noms, qui peuvent contenir des espaces)
    std::string rest_of_line()
    {
        std::string line;
        std::getline(in, line);
        if (!line.empty() && line[0] == ' ') {
            line.erase(0, 1);
        }
        if (line.empty()) {
            fail("nom vide");
        }
        return line;
    }

    Resources resources(int max)
    {
        Resources values = {0, 0, 0, 0, 0};
        for (int c = 0; c < COLOR_COUNT; c++) {
            values[c] = number(0, max);
        }
        return values;
    }

    Card card()
    {
        expect("card");
        Card read = {0, Color::White, {0, 0, 0, 0, 0}};
        read.points = number(0, 15);
        read.provides = color_at(number(0, COLOR_COUNT - 1));
        read.cost = resources(20);
        return read;
    }

    // Un nombre de cartes puis les cartes
    std::vector<Card> card_list(int max)
    {
        int count = number(0, max);
        std::vector<Card> read;
        for (int i = 0; i < count; i++) {
            read.push_back(card());
        }
        return read;
    }

    std::vector<Card> cards(const std::string& label, int max)
    {
        expect(label);
        return card_list(max);
    }

    Noble noble()
    {
        expect("noble");
        Noble read;
        read.points = number(0, 15);
        read.cost = resources(20);
        read.name = rest_of_line();
        return read;
    }

    // Index d'un mot dans une liste de noms
    int choice(const char* const names[], int count)
    {
        std::string read = word();
        for (int i = 0; i < count; i++) {
            if (read == names[i]) {
                return i;
            }
        }
        fail("valeur inconnue « " + read + " »");
        return 0;
    }
};

std::string load_game(const std::string& text, std::unique_ptr<Game>& game)
{
    try {
        Reader reader(text);
        reader.expect("SPLENDOR");
        reader.number(1, 1);
        reader.expect("mode");
        std::string mode_name = reader.word();
        if (mode_name != "classique" && mode_name != "rapide") {
            reader.fail("mode inconnu");
        }
        Mode mode = mode_name == "classique" ? Mode::Classic : Mode::Quick;

        reader.expect("round");
        int round = reader.number(1, 100000);
        reader.expect("current");
        int current = reader.number(0, 3);
        reader.expect("step");
        TurnStep step = static_cast<TurnStep>(reader.choice(STEP_NAMES, 4));
        reader.expect("last_round");
        bool last_round = reader.number(0, 1) == 1;
        reader.expect("passes");
        int passes = reader.number(0, 4);
        reader.expect("bank");
        Resources bank = reader.resources(7);
        reader.expect("gold");
        int bank_gold = reader.number(0, GOLD_TOKENS);

        reader.expect("players");
        int count = reader.number(2, 4);
        if (current >= count) {
            reader.fail("joueur courant inexistant");
        }
        std::vector<Player> players;
        std::vector<std::string> names;
        for (int i = 0; i < count; i++) {
            Player player;
            reader.expect("name");
            player.name = reader.rest_of_line();
            reader.expect("controller");
            player.controller = static_cast<Controller>(reader.choice(CONTROLLER_NAMES, 3));
            reader.expect("tokens");
            player.tokens = reader.resources(7);
            reader.expect("gold");
            player.gold = reader.number(0, GOLD_TOKENS);
            player.cards = reader.cards("cards", 90);
            player.reserved = reader.cards("reserved", MAX_RESERVED);
            reader.expect("nobles");
            int noble_count = reader.number(0, 10);
            for (int k = 0; k < noble_count; k++) {
                player.nobles.push_back(reader.noble());
            }
            names.push_back(player.name);
            players.push_back(player);
        }

        std::unique_ptr<Game> loaded(new Game(names, 0, mode));
        for (int l = 0; l < LEVEL_COUNT; l++) {
            reader.expect("deck");
            reader.number(l + 1, l + 1);
            loaded->decks_[l] = reader.card_list(40);
            reader.expect("visible");
            reader.number(l + 1, l + 1);
            loaded->visible_[l] = reader.card_list(VISIBLE_PER_LEVEL);
        }
        reader.expect("board_nobles");
        int board_count = reader.number(0, 10);
        loaded->nobles_.clear();
        for (int k = 0; k < board_count; k++) {
            loaded->nobles_.push_back(reader.noble());
        }
        reader.expect("choices");
        int choice_count = reader.number(0, board_count);
        for (int k = 0; k < choice_count; k++) {
            loaded->noble_choices_.push_back(reader.number(0, board_count - 1));
        }
        reader.expect("end");

        loaded->players_ = players;
        loaded->bank_ = bank;
        loaded->bank_gold_ = bank_gold;
        loaded->current_ = current;
        loaded->round_ = round;
        loaded->step_ = step;
        loaded->last_round_ = last_round;
        loaded->consecutive_passes_ = passes;
        game = std::move(loaded);
    } catch (const std::runtime_error& error) {
        return error.what();
    }
    return "";
}

// ---------------------------------------------------------------- Fichiers

std::string save_game_to_file(const Game& game, const std::string& path)
{
    std::ofstream file(path);
    if (!file) {
        return "Impossible d'écrire le fichier " + path + ".";
    }
    file << save_game(game);
    return file ? "" : "Erreur pendant l'écriture de " + path + ".";
}

std::string load_game_from_file(const std::string& path, std::unique_ptr<Game>& game)
{
    std::ifstream file(path);
    if (!file) {
        return "Impossible d'ouvrir le fichier " + path + ".";
    }
    std::ostringstream text;
    text << file.rdbuf();
    return load_game(text.str(), game);
}
