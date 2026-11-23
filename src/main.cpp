#include <iostream>
#ifdef _WIN32
#include <windows.h>
#endif
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "ai.hpp"
#include "display.hpp"
#include "game.hpp"
#include "i18n.hpp"
#include "save.hpp"
#include "version.hpp"

// Lit une ligne sur l'entrée standard après avoir affiché `prompt`.
// Retourne false si l'entrée est terminée (Ctrl-D) : on quitte alors le jeu.
static bool read_line(const std::string& prompt, std::string& line)
{
    std::cout << prompt << std::flush;
    if (!std::getline(std::cin, line)) {
        std::cout << "\n";
        return false;
    }
    return true;
}

// Demande un entier entre `min` et `max` jusqu'à obtenir une réponse valide.
// Retourne false si l'entrée est terminée ou si le joueur tape q.
static bool ask_number(Lang lang, const std::string& prompt, int min, int max, int& number)
{
    std::string line;
    while (read_line(prompt, line)) {
        if (line == "q") {
            return false;
        }
        std::istringstream input(line);
        if (input >> number && number >= min && number <= max) {
            return true;
        }
        std::cout << tr(lang, "Invalid answer: enter a number between {0} and {1}.",
                        {std::to_string(min), std::to_string(max)}) << "\n";
    }
    return false;
}

// Lit les couleurs d'une commande "t" (ex : "1 3 5" ou "2 2").
// Retourne une chaîne vide si tout va bien, sinon le message d'erreur.
static std::string parse_colors(std::istringstream& input, Resources& wanted)
{
    wanted = {0, 0, 0, 0, 0};
    int color = 0;
    while (input >> color) {
        if (color < 1 || color > COLOR_COUNT) {
            return "Unknown colour: use the numbers 1 to 5.";
        }
        wanted[color - 1]++;
    }
    if (!input.eof()) {
        return "Colours must be numbers (e.g. t 1 3 5).";
    }
    return "";
}

// Lit "b <niveau> <position>" ou "b r <n>" (carte réservée)
static std::string buy_command(Game& game, std::istringstream& input)
{
    std::string first;
    input >> first;
    if (first == "r") {
        int index = 0;
        if (game.mode() != Mode::Classic) {
            return "No reserved cards in a quick game.";
        }
        if (!(input >> index)) {
            return "Usage: b r <n>, for example b r 1.";
        }
        return game.buy_reserved(index);
    }
    std::istringstream numbers(first);
    int level = 0;
    int position = 0;
    if (!(numbers >> level) || !(input >> position)) {
        return "Usage: b <level> <position>, for example b 1 3.";
    }
    return game.buy_card(level, position);
}

// Lit "r <niveau> <position>" ou "r <niveau> d" (dessus de la pioche ; « p » en français)
static std::string reserve_command(Game& game, std::istringstream& input)
{
    if (game.mode() != Mode::Classic) {
        return "Reservations don't exist in a quick game.";
    }
    int level = 0;
    std::string where;
    if (!(input >> level >> where)) {
        return "Usage: r <level> <position> or r <level> d, for example r 2 4.";
    }
    if (where == "d" || where == "p") {
        return game.reserve_card(level, 0);
    }
    std::istringstream number(where);
    int position = 0;
    if (!(number >> position) || position < 1) {
        return "Usage: r <level> <position> or r <level> d, for example r 2 4.";
    }
    return game.reserve_card(level, position);
}

// Fait jouer l'action du joueur courant.
// Retourne false si le joueur veut quitter la partie.
static bool play_action(Game& game, Lang lang)
{
    std::string line;
    while (true) {
        if (!read_line(tr(lang, "{0}, your action: ", {game.current_player().name}), line)) {
            return false;
        }
        std::istringstream input(line);
        std::string command;
        input >> command;

        std::string error;
        if (command == "q") {
            return false;
        } else if (command == "?" || command.empty()) {
            show_help(game.mode(), lang);
            continue;
        } else if (command == "s") {
            std::string path = "partie.splendor";
            input >> path;
            error = save_game_to_file(game, path);
            if (error.empty()) {
                std::cout << tr(lang, "Game saved in {0} (resume with --load {0}).", {path}) << "\n";
                continue;
            }
        } else if (command == "t" || command == "j") {  // j : jetons (français)
            Resources wanted;
            error = parse_colors(input, wanted);
            if (error.empty()) {
                error = game.take_tokens(wanted);
            }
        } else if (command == "b" || command == "a") {  // a : acheter (français)
            error = buy_command(game, input);
        } else if (command == "r") {
            error = reserve_command(game, input);
        } else {
            error = "Unknown command (type ? for help).";
        }

        if (error.empty()) {
            return true;
        }
        std::cout << tr(lang, "Error: {0}", {tr(lang, error)}) << "\n";
    }
}

// Étape Discard : demande un jeton à défausser (Game passe à la suite tout seul).
// Retourne false si le joueur veut quitter.
static bool ask_discard(Game& game, Lang lang)
{
    bool classic = game.mode() == Mode::Classic;
    int max_choice = classic ? COLOR_COUNT + 1 : COLOR_COUNT;
    std::string prompt = tr(lang, classic ? "Token to discard (1-5, 6 = Gold): " : "Colour to discard (1-5): ");
    std::cout << tr(lang, "More than 10 tokens, {0} more to discard: ", {std::to_string(game.tokens_to_discard())})
              << format_tokens(game.current_player(), lang) << "\n";
    int choice = 0;
    if (!ask_number(lang, prompt, 1, max_choice, choice)) {
        return false;
    }
    std::string error = choice == COLOR_COUNT + 1 ? game.discard_gold() : game.discard(color_at(choice - 1));
    if (!error.empty()) {
        std::cout << tr(lang, "Error: {0}", {tr(lang, error)}) << "\n";
    }
    return true;
}

// Étape ChooseNoble : plusieurs nobles peuvent venir, le joueur en choisit un.
static bool ask_noble(Game& game, Lang lang)
{
    const std::vector<int>& choices = game.noble_choices();
    std::cout << tr(lang, "Several nobles want to visit you:") << "\n";
    for (int i = 0; i < static_cast<int>(choices.size()); i++) {
        std::cout << "  " << (i + 1) << ") " << game.nobles()[choices[i]].name << "\n";
    }
    int number = 0;
    if (!ask_number(lang, tr(lang, "Which one do you choose? "), 1, static_cast<int>(choices.size()), number)) {
        return false;
    }
    std::string error = game.choose_noble(choices[number - 1]);
    if (!error.empty()) {
        std::cout << tr(lang, "Error: {0}", {tr(lang, error)}) << "\n";
    }
    return true;
}

// Affiche les événements apparus depuis le dernier appel
static void print_new_events(const Game& game, int& shown, Lang lang)
{
    const std::vector<Event>& events = game.events();
    for (; shown < static_cast<int>(events.size()); shown++) {
        std::cout << "  > " << describe_event(game, events[shown], lang) << "\n";
    }
}

// Sous Windows : console en UTF-8 et codes couleur ANSI activés
static void prepare_console()
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(output, &mode)) {
        SetConsoleMode(output, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
#endif
}

// Sous Windows, la console se ferme dès la fin du programme quand on l'a
// lancé par double-clic : on attend Entrée pour laisser lire le résultat.
static void wait_before_exit(Lang lang)
{
#ifdef _WIN32
    std::string line;
    std::cout << tr(lang, "\nPress Enter to quit...") << std::flush;
    std::getline(std::cin, line);
#else
    (void)lang;  // seulement sous Windows
#endif
}

static void print_usage(const char* program)
{
    std::cerr << "Usage: " << program << " [--quick | --classic] [seed]\n"
              << "       " << program << " --load file.splendor\n"
              << "       " << program << " --version\n"
              << "  --quick    quick game: no gold, no reservations   (or --rapide)\n"
              << "  --classic  classic rules: gold and reservations   (or --classique)\n"
              << "  seed       number to replay the same game\n"
              << "Language: English, or French when LANG starts with fr.\n";
}

// Nouvelle partie : demande le mode (si besoin), les joueurs et qui les joue.
// Retourne false si le joueur quitte.
static bool create_game(std::unique_ptr<Game>& game, unsigned seed, bool mode_chosen, Mode mode, Lang lang)
{
    if (!mode_chosen) {
        std::cout << tr(lang, "  1) Quick game   (no gold, no reservations)\n  2) Classic game (gold and reservations)\n");
        int choice = 0;
        if (!ask_number(lang, tr(lang, "Game mode (1-2): "), 1, 2, choice)) {
            return false;
        }
        mode = choice == 2 ? Mode::Classic : Mode::Quick;
    }

    int player_count = 0;
    if (!ask_number(lang, tr(lang, "Number of players (2-4): "), 2, 4, player_count)) {
        return false;
    }
    std::vector<std::string> names;
    std::vector<Controller> controllers;
    for (int i = 1; i <= player_count; i++) {
        std::string name;
        if (!read_line(tr(lang, "Name of player {0}: ", {std::to_string(i)}), name)) {
            return false;
        }
        if (name.empty()) {
            name = tr(lang, "Player {0}", {std::to_string(i)});
        }
        names.push_back(name);

        // Humain par défaut (Entrée), sinon ordinateur facile ou moyen
        std::string answer;
        if (!read_line(tr(lang, "  Played by: Enter = human, e = easy computer, m = medium computer: "), answer)) {
            return false;
        }
        if (answer == "e" || answer == "f") {  // f : facile (français)
            controllers.push_back(Controller::AiEasy);
        } else if (answer == "m") {
            controllers.push_back(Controller::AiMedium);
        } else {
            controllers.push_back(Controller::Human);
        }
    }

    game.reset(new Game(names, seed, mode));
    for (int i = 0; i < player_count; i++) {
        game->set_controller(i, controllers[i]);
    }
    return true;
}

int main(int argc, char* argv[])
{
    prepare_console();
    Lang lang = system_lang();
    unsigned seed = std::random_device{}();
    bool mode_chosen = false;
    Mode mode = Mode::Quick;
    std::string load_path;

    for (int i = 1; i < argc; i++) {
        std::string argument = argv[i];
        std::istringstream number(argument);
        if (argument == "--version") {
            std::cout << "splendor " << SPLENDOR_VERSION << "\n";
            return 0;
        } else if ((argument == "--load" || argument == "--charger") && i + 1 < argc) {
            load_path = argv[++i];
        } else if (argument == "--quick" || argument == "--rapide") {
            mode = Mode::Quick;
            mode_chosen = true;
        } else if (argument == "--classic" || argument == "--classique") {
            mode = Mode::Classic;
            mode_chosen = true;
        } else if (!(number >> seed)) {
            print_usage(argv[0]);
            return 1;
        }
    }

    std::cout << "=== GEMSPIRE " << SPLENDOR_VERSION << " ===\n";
    std::unique_ptr<Game> game_holder;
    if (!load_path.empty()) {
        std::string error = load_game_from_file(load_path, game_holder);
        if (!error.empty()) {
            std::cerr << error << "\n";
            return 1;
        }
        std::cout << tr(lang, "Game loaded from {0}.", {load_path}) << "\n";
    } else if (!create_game(game_holder, seed, mode_chosen, mode, lang)) {
        return 0;
    }
    Game& game = *game_holder;
    show_help(game.mode(), lang);

    int shown_events = 0;
    while (!game.is_over()) {
        bool keep_playing = true;
        if (ai_to_play(game)) {
            std::string error = ai_play(game);
            if (!error.empty()) {
                std::cout << tr(lang, "Computer error: {0}", {error}) << "\n";
                return 1;
            }
        } else if (game.step() == TurnStep::Action) {
            show_board(game, lang);
            keep_playing = play_action(game, lang);
        } else if (game.step() == TurnStep::Discard) {
            keep_playing = ask_discard(game, lang);
        } else if (game.step() == TurnStep::ChooseNoble) {
            keep_playing = ask_noble(game, lang);
        }
        if (!keep_playing) {
            std::cout << tr(lang, "Game abandoned.") << "\n";
            return 0;
        }
        print_new_events(game, shown_events, lang);
    }

    show_results(game, lang);
    wait_before_exit(lang);
    return 0;
}
