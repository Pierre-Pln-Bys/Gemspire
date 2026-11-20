#include <iostream>
#ifdef _WIN32
#include <windows.h>
#endif
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "display.hpp"
#include "game.hpp"
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

// Défausse tant que le joueur a plus de 10 jetons.
static bool discard_excess(Game& game)
{
    bool classic = game.mode() == Mode::Classic;
    int max_choice = classic ? COLOR_COUNT + 1 : COLOR_COUNT;
    std::string prompt = classic ? "Jeton à défausser (1-5, 6 = Or) : " : "Couleur à défausser (1-5) : ";
    while (game.must_discard()) {
        std::cout << "Vous avez plus de 10 jetons : " << format_tokens(game.current_player()) << "\n";
        int choice = 0;
        if (!ask_number(prompt, 1, max_choice, choice)) {
            return false;
        }
        std::string error = choice == COLOR_COUNT + 1 ? game.discard_gold() : game.discard(color_at(choice - 1));
        if (!error.empty()) {
            std::cout << "Erreur : " << error << "\n";
        }
    }
    return true;
}

// Visite d'un noble en fin de tour (le joueur choisit s'il y en a plusieurs).
static bool noble_visit(Game& game)
{
    std::vector<int> candidates = game.visiting_nobles();
    if (candidates.empty()) {
        return true;
    }
    int choice = 0;
    if (candidates.size() > 1) {
        std::cout << "Plusieurs nobles veulent vous rendre visite :\n";
        for (int i = 0; i < static_cast<int>(candidates.size()); i++) {
            std::cout << "  " << (i + 1) << ") " << game.nobles()[candidates[i]].name << "\n";
        }
        int number = 0;
        if (!ask_number("Lequel choisissez-vous ? ", 1, static_cast<int>(candidates.size()), number)) {
            return false;
        }
        choice = number - 1;
    }
    std::cout << game.nobles()[candidates[choice]].name << " rend visite à "
              << game.current_player().name << " (+3 pts) !\n";
    game.receive_noble(candidates[choice]);
    return true;
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
              << "       " << program << " --version\n"
              << "  --rapide     partie rapide : sans or ni réservation\n"
              << "  --classique  règles classiques : avec or et réservation\n"
              << "  graine       nombre pour rejouer la même partie\n";
}

int main(int argc, char* argv[])
{
    prepare_console();
    unsigned seed = std::random_device{}();
    bool mode_chosen = false;
    Mode mode = Mode::Quick;

    for (int i = 1; i < argc; i++) {
        std::string argument = argv[i];
        std::istringstream number(argument);
        if (argument == "--version") {
            std::cout << "splendor " << SPLENDOR_VERSION << "\n";
            return 0;
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

    std::cout << "=== SPLENDOR " << SPLENDOR_VERSION << " ===\n";
    if (!mode_chosen) {
        std::cout << "  1) Partie rapide   (sans or ni réservation)\n"
                  << "  2) Partie classique (avec or et réservation)\n";
        int choice = 0;
        if (!ask_number("Mode de jeu (1-2) : ", 1, 2, choice)) {
            return 0;
        }
        mode = choice == 2 ? Mode::Classic : Mode::Quick;
    }

    int player_count = 0;
    if (!ask_number("Nombre de joueurs (2-4) : ", 2, 4, player_count)) {
        return 0;
    }
    std::vector<std::string> names;
    for (int i = 1; i <= player_count; i++) {
        std::string name;
        if (!read_line("Nom du joueur " + std::to_string(i) + " : ", name)) {
            return 0;
        }
        if (name.empty()) {
            name = "Joueur " + std::to_string(i);
        }
        names.push_back(name);
    }

    Game game(names, seed, mode);
    show_help(mode);

    while (!game.is_over()) {
        show_board(game);
        if (game.can_play()) {
            if (!play_action(game)) {
                std::cout << "Partie abandonnée.\n";
                return 0;
            }
        } else {
            std::cout << game.current_player().name << " ne peut rien faire et passe son tour.\n";
            game.pass_turn();
        }
        if (!discard_excess(game) || !noble_visit(game)) {
            std::cout << "Partie abandonnée.\n";
            return 0;
        }
        game.end_turn();
    }

    show_results(game);
    wait_before_exit();
    return 0;
}
