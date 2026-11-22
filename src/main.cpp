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
static bool ask_number(const std::string& prompt, int min, int max, int& number)
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
        std::cout << "Réponse invalide : entrez un nombre entre " << min << " et " << max << ".\n";
    }
    return false;
}

// Lit les couleurs d'une commande "j" (ex : "1 3 5" ou "2 2").
// Retourne une chaîne vide si tout va bien, sinon le message d'erreur.
static std::string parse_colors(std::istringstream& input, Resources& wanted)
{
    wanted = {0, 0, 0, 0, 0};
    int color = 0;
    while (input >> color) {
        if (color < 1 || color > COLOR_COUNT) {
            return "Couleur inconnue : utilisez les numéros 1 à 5.";
        }
        wanted[color - 1]++;
    }
    if (!input.eof()) {
        return "Les couleurs doivent être des numéros (ex : j 1 3 5).";
    }
    return "";
}

// Lit "a <niveau> <position>" ou "a r <n>" (carte réservée)
static std::string buy_command(Game& game, std::istringstream& input)
{
    std::string first;
    input >> first;
    if (first == "r") {
        int index = 0;
        if (game.mode() != Mode::Classic) {
            return "Pas de cartes réservées en partie rapide.";
        }
        if (!(input >> index)) {
            return "Usage : a r <n>, par exemple a r 1.";
        }
        return game.buy_reserved(index);
    }
    std::istringstream numbers(first);
    int level = 0;
    int position = 0;
    if (!(numbers >> level) || !(input >> position)) {
        return "Usage : a <niveau> <position>, par exemple a 1 3.";
    }
    return game.buy_card(level, position);
}

// Lit "r <niveau> <position>" ou "r <niveau> p" (dessus de la pioche)
static std::string reserve_command(Game& game, std::istringstream& input)
{
    if (game.mode() != Mode::Classic) {
        return "La réservation n'existe pas en partie rapide.";
    }
    int level = 0;
    std::string where;
    if (!(input >> level >> where)) {
        return "Usage : r <niveau> <position> ou r <niveau> p, par exemple r 2 4.";
    }
    if (where == "p") {
        return game.reserve_card(level, 0);
    }
    std::istringstream number(where);
    int position = 0;
    if (!(number >> position) || position < 1) {
        return "Usage : r <niveau> <position> ou r <niveau> p, par exemple r 2 4.";
    }
    return game.reserve_card(level, position);
}

// Fait jouer l'action du joueur courant.
// Retourne false si le joueur veut quitter la partie.
static bool play_action(Game& game)
{
    std::string line;
    while (true) {
        if (!read_line(game.current_player().name + ", votre action : ", line)) {
            return false;
        }
        std::istringstream input(line);
        std::string command;
        input >> command;

        std::string error;
        if (command == "q") {
            return false;
        } else if (command == "?" || command.empty()) {
            show_help(game.mode());
            continue;
        } else if (command == "s") {
            std::string path = "partie.splendor";
            input >> path;
            error = save_game_to_file(game, path);
            if (error.empty()) {
                std::cout << "Partie sauvegardée dans " << path
                          << " (reprendre avec --charger " << path << ").\n";
                continue;
            }
        } else if (command == "j") {
            Resources wanted;
            error = parse_colors(input, wanted);
            if (error.empty()) {
                error = game.take_tokens(wanted);
            }
        } else if (command == "a") {
            error = buy_command(game, input);
        } else if (command == "r") {
            error = reserve_command(game, input);
        } else {
            error = "Commande inconnue (tapez ? pour l'aide).";
        }

        if (error.empty()) {
            return true;
        }
        std::cout << "Erreur : " << error << "\n";
    }
}

// Étape Discard : demande un jeton à défausser (Game passe à la suite tout seul).
// Retourne false si le joueur veut quitter.
static bool ask_discard(Game& game)
{
    bool classic = game.mode() == Mode::Classic;
    int max_choice = classic ? COLOR_COUNT + 1 : COLOR_COUNT;
    std::string prompt = classic ? "Jeton à défausser (1-5, 6 = Or) : " : "Couleur à défausser (1-5) : ";
    std::cout << "Plus de 10 jetons, encore " << game.tokens_to_discard() << " à défausser : "
              << format_tokens(game.current_player()) << "\n";
    int choice = 0;
    if (!ask_number(prompt, 1, max_choice, choice)) {
        return false;
    }
    std::string error = choice == COLOR_COUNT + 1 ? game.discard_gold() : game.discard(color_at(choice - 1));
    if (!error.empty()) {
        std::cout << "Erreur : " << error << "\n";
    }
    return true;
}

// Étape ChooseNoble : plusieurs nobles peuvent venir, le joueur en choisit un.
static bool ask_noble(Game& game)
{
    const std::vector<int>& choices = game.noble_choices();
    std::cout << "Plusieurs nobles veulent vous rendre visite :\n";
    for (int i = 0; i < static_cast<int>(choices.size()); i++) {
        std::cout << "  " << (i + 1) << ") " << game.nobles()[choices[i]].name << "\n";
    }
    int number = 0;
    if (!ask_number("Lequel choisissez-vous ? ", 1, static_cast<int>(choices.size()), number)) {
        return false;
    }
    std::string error = game.choose_noble(choices[number - 1]);
    if (!error.empty()) {
        std::cout << "Erreur : " << error << "\n";
    }
    return true;
}

// Affiche les événements apparus depuis le dernier appel
static void print_new_events(const Game& game, int& shown)
{
    const std::vector<Event>& events = game.events();
    for (; shown < static_cast<int>(events.size()); shown++) {
        std::cout << "  > " << describe_event(game, events[shown]) << "\n";
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
static void wait_before_exit()
{
#ifdef _WIN32
    std::string line;
    std::cout << "\nAppuyez sur Entrée pour quitter..." << std::flush;
    std::getline(std::cin, line);
#endif
}

static void print_usage(const char* program)
{
    std::cerr << "Usage : " << program << " [--rapide | --classique] [graine]\n"
              << "       " << program << " --charger fichier.splendor\n"
              << "       " << program << " --version\n"
              << "  --rapide     partie rapide : sans or ni réservation\n"
              << "  --classique  règles classiques : avec or et réservation\n"
              << "  graine       nombre pour rejouer la même partie\n";
}

// Nouvelle partie : demande le mode (si besoin), les joueurs et qui les joue.
// Retourne false si le joueur quitte.
static bool create_game(std::unique_ptr<Game>& game, unsigned seed, bool mode_chosen, Mode mode)
{
    if (!mode_chosen) {
        std::cout << "  1) Partie rapide   (sans or ni réservation)\n"
                  << "  2) Partie classique (avec or et réservation)\n";
        int choice = 0;
        if (!ask_number("Mode de jeu (1-2) : ", 1, 2, choice)) {
            return false;
        }
        mode = choice == 2 ? Mode::Classic : Mode::Quick;
    }

    int player_count = 0;
    if (!ask_number("Nombre de joueurs (2-4) : ", 2, 4, player_count)) {
        return false;
    }
    std::vector<std::string> names;
    std::vector<Controller> controllers;
    for (int i = 1; i <= player_count; i++) {
        std::string name;
        if (!read_line("Nom du joueur " + std::to_string(i) + " : ", name)) {
            return false;
        }
        if (name.empty()) {
            name = "Joueur " + std::to_string(i);
        }
        names.push_back(name);

        // Humain par défaut (Entrée), sinon ordinateur facile ou moyen
        std::string answer;
        if (!read_line("  Joué par : Entrée = humain, f = ordinateur facile, m = ordinateur moyen : ", answer)) {
            return false;
        }
        if (answer == "f") {
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
        } else if (argument == "--charger" && i + 1 < argc) {
            load_path = argv[++i];
        } else if (argument == "--rapide") {
            mode = Mode::Quick;
            mode_chosen = true;
        } else if (argument == "--classique") {
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
        std::cout << "Partie chargée depuis " << load_path << ".\n";
    } else if (!create_game(game_holder, seed, mode_chosen, mode)) {
        return 0;
    }
    Game& game = *game_holder;
    show_help(game.mode());

    int shown_events = 0;
    while (!game.is_over()) {
        bool keep_playing = true;
        if (ai_to_play(game)) {
            std::string error = ai_play(game);
            if (!error.empty()) {
                std::cout << "Erreur de l'ordinateur : " << error << "\n";
                return 1;
            }
        } else if (game.step() == TurnStep::Action) {
            show_board(game);
            keep_playing = play_action(game);
        } else if (game.step() == TurnStep::Discard) {
            keep_playing = ask_discard(game);
        } else if (game.step() == TurnStep::ChooseNoble) {
            keep_playing = ask_noble(game);
        }
        if (!keep_playing) {
            std::cout << "Partie abandonnée.\n";
            return 0;
        }
        print_new_events(game, shown_events);
    }

    show_results(game);
    wait_before_exit();
    return 0;
}
