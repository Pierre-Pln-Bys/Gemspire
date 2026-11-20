#include "display.hpp"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <vector>

#include "rules.hpp"

// Un "bloc" est un dessin rectangulaire : des lignes de même largeur visible.
// On les colle côte à côte pour former une rangée du plateau.
using Block = std::vector<std::string>;

const int CARD_INNER = 11;    // largeur intérieure d'une carte
const int NOBLE_INNER = 12;   // largeur intérieure d'une tuile noble
const int DECK_INNER = 7;     // largeur intérieure d'une pioche
const int BLOCK_HEIGHT = 7;   // hauteur des cartes, pioches et nobles

// ---------------------------------------------------------------- Couleurs

static bool colors_enabled()
{
    return std::getenv("NO_COLOR") == nullptr;
}

static std::string ansi(const std::string& code, const std::string& text)
{
    if (!colors_enabled()) {
        return text;
    }
    return "\033[" + code + "m" + text + "\033[0m";
}

std::string color_name(Color color)
{
    switch (color) {
    case Color::White:  return "Blanc";
    case Color::Blue:   return "Bleu";
    case Color::Green:  return "Vert";
    case Color::Red:    return "Rouge";
    case Color::Black:  return "Noir";
    }
    return "?";
}

std::string colored(Color color, const std::string& text)
{
    switch (color) {
    case Color::White:  return ansi("1;37", text);
    case Color::Blue:   return ansi("1;34", text);
    case Color::Green:  return ansi("1;32", text);
    case Color::Red:    return ansi("1;31", text);
    case Color::Black:  return ansi("1;90", text);  // gris foncé, lisible sur fond noir
    }
    return text;
}

static std::string gold(const std::string& text)
{
    return ansi("1;33", text);
}

static std::string dim(const std::string& text)
{
    return ansi("2", text);
}

static std::string bold(const std::string& text)
{
    return ansi("1", text);
}

// Couleur de chaque joueur dans la console (violet, orange, turquoise, rose),
// différente des couleurs des jetons
std::string player_colored(int player, const std::string& text)
{
    const std::string codes[] = {"1;38;5;141", "1;38;5;208", "1;38;5;44", "1;38;5;205"};
    return ansi(codes[player % 4], text);
}

// ---------------------------------------------------------------- Mise en page

// Largeur à l'écran : on ignore les codes ANSI et on compte les caractères
// UTF-8 (les octets de continuation 10xxxxxx ne comptent pas).
static int visible_width(const std::string& text)
{
    int width = 0;
    bool in_escape = false;
    for (unsigned char c : text) {
        if (in_escape) {
            if (c == 'm') {
                in_escape = false;
            }
        } else if (c == '\033') {
            in_escape = true;
        } else if ((c & 0xC0) != 0x80) {
            width++;
        }
    }
    return width;
}

// Complète `text` avec des espaces jusqu'à `width` caractères visibles
static std::string pad(const std::string& text, int width)
{
    std::string result = text;
    for (int i = visible_width(text); i < width; i++) {
        result += ' ';
    }
    return result;
}

static std::string center(const std::string& text, int width)
{
    int left = (width - visible_width(text)) / 2;
    return pad(std::string(left > 0 ? left : 0, ' ') + text, width);
}

static std::string repeat(const std::string& piece, int count)
{
    std::string result;
    for (int i = 0; i < count; i++) {
        result += piece;
    }
    return result;
}

// Affiche les blocs côte à côte, séparés par un espace
static void print_row(const std::vector<Block>& blocks, const std::string& margin)
{
    for (int line = 0; line < BLOCK_HEIGHT; line++) {
        std::cout << margin;
        for (const Block& block : blocks) {
            std::cout << block[line] << " ";
        }
        std::cout << "\n";
    }
}

// ---------------------------------------------------------------- Textes

std::string format_resources(const Resources& resources)
{
    std::string text;
    for (int c = 0; c < COLOR_COUNT; c++) {
        if (resources[c] == 0) {
            continue;
        }
        if (!text.empty()) {
            text += ", ";
        }
        text += colored(color_at(c), color_name(color_at(c)) + ": " + std::to_string(resources[c]));
    }
    if (text.empty()) {
        return "aucun";
    }
    return text;
}

std::string format_tokens(const Player& player)
{
    std::string text = format_resources(player.tokens);
    if (player.gold > 0) {
        if (text == "aucun") {
            text = "";
        } else {
            text += ", ";
        }
        text += gold("Or: " + std::to_string(player.gold));
    }
    return text;
}

// ---------------------------------------------------------------- Dessins

// Une carte, comme la vraie : points et gemme en haut, coûts en bas à gauche.
//   ┌───────────┐
//   │ 3   ◆ Rouge│
//   │           │
//   │ ● 3 Blanc │
//   │ ● 5 Vert  │
//   └─ 2 * ─────┘   (numéro de la carte, * = achetable)
static Block card_block(const Card& card, const std::string& label, bool buyable)
{
    Color c = card.provides;
    std::string side = colored(c, "│");
    Block block;
    block.push_back(colored(c, "┌" + repeat("─", CARD_INNER) + "┐"));

    std::string points = card.points > 0 ? bold(std::to_string(card.points)) : " ";
    std::string gem = colored(c, "◆ " + color_name(c));
    int spaces = CARD_INNER - 1 - visible_width(points) - visible_width(gem);
    block.push_back(side + " " + points + std::string(spaces, ' ') + gem + side);

    // Coûts alignés en bas de la carte (au plus 4 couleurs)
    std::vector<std::string> costs;
    for (int i = 0; i < COLOR_COUNT; i++) {
        if (card.cost[i] > 0) {
            Color cost_color = color_at(i);
            costs.push_back(" " + colored(cost_color, "●") + " " + std::to_string(card.cost[i])
                            + " " + colored(cost_color, color_name(cost_color)));
        }
    }
    for (int line = 0; line < 4; line++) {
        int index = line - (4 - static_cast<int>(costs.size()));
        std::string content = index >= 0 ? costs[index] : "";
        block.push_back(side + pad(content, CARD_INNER) + side);
    }

    std::string tag = " " + label + (buyable ? " *" : "") + " ";
    std::string bottom_tag = buyable ? bold(gold(tag)) : bold(tag);
    block.push_back(colored(c, "└─") + bottom_tag
                    + colored(c, repeat("─", CARD_INNER - 1 - visible_width(tag)) + "┘"));
    return block;
}

// Emplacement vide (la pioche du niveau est épuisée)
static Block empty_block()
{
    Block block;
    for (int line = 0; line < BLOCK_HEIGHT; line++) {
        std::string content = line == 3 ? center("(vide)", CARD_INNER + 2) : std::string(CARD_INNER + 2, ' ');
        block.push_back(dim(content));
    }
    return block;
}

// La pioche d'un niveau, à gauche de la rangée
static Block deck_block(int level, int count)
{
    const std::string roman[] = {"I", "II", "III"};
    Block block;
    block.push_back("╔" + repeat("═", DECK_INNER) + "╗");
    block.push_back("║" + center("NIVEAU", DECK_INNER) + "║");
    block.push_back("║" + center(bold(roman[level - 1]), DECK_INNER) + "║");
    block.push_back("║" + std::string(DECK_INNER, ' ') + "║");
    block.push_back("║" + center("pioche", DECK_INNER) + "║");
    block.push_back("║" + center(std::to_string(count), DECK_INNER) + "║");
    block.push_back("╚" + repeat("═", DECK_INNER) + "╝");
    return block;
}

// Coupe un nom en 2 lignes d'au plus `width` caractères (au niveau des espaces)
static std::vector<std::string> wrap_name(const std::string& name, int width)
{
    std::istringstream words(name);
    std::string word;
    std::vector<std::string> lines(2);
    int line = 0;
    while (words >> word) {
        // Le mot ne tient plus sur la première ligne : on passe à la seconde
        if (line == 0 && !lines[0].empty() && visible_width(lines[0] + " " + word) > width) {
            line = 1;
        }
        lines[line] = lines[line].empty() ? word : lines[line] + " " + word;
    }
    return lines;
}

// Une tuile noble : nom, puis les bonus demandés
static Block noble_block(const Noble& noble)
{
    std::string side = gold("│");
    std::string title = " " + std::to_string(noble.points) + " pts ";
    Block block;
    block.push_back(gold("┌─" + title + repeat("─", NOBLE_INNER - 1 - visible_width(title)) + "┐"));
    for (const std::string& line : wrap_name(noble.name, NOBLE_INNER - 1)) {
        block.push_back(side + pad(" " + bold(line), NOBLE_INNER) + side);
    }
    std::vector<std::string> costs;
    for (int i = 0; i < COLOR_COUNT; i++) {
        if (noble.cost[i] > 0) {
            Color c = color_at(i);
            costs.push_back(" " + colored(c, "◆") + " " + std::to_string(noble.cost[i]) + " "
                            + colored(c, color_name(c)));
        }
    }
    for (int line = 0; line < 3; line++) {
        std::string content = line < static_cast<int>(costs.size()) ? costs[line] : "";
        block.push_back(side + pad(content, NOBLE_INNER) + side);
    }
    block.push_back(gold("└" + repeat("─", NOBLE_INNER) + "┘"));
    return block;
}

// La réserve de jetons, sur le côté droit du plateau
static std::vector<std::string> bank_lines(const Game& game)
{
    std::vector<std::string> lines;
    lines.push_back(bold("RÉSERVE"));
    lines.push_back("");
    for (int c = 0; c < COLOR_COUNT; c++) {
        Color color = color_at(c);
        std::string count = std::to_string(game.bank()[c]);
        lines.push_back(dim(std::to_string(c + 1)) + " " + colored(color, "●") + " " + count + " "
                        + colored(color, color_name(color)));
    }
    if (game.mode() == Mode::Classic) {
        lines.push_back("  " + gold("●") + " " + std::to_string(game.bank_gold()) + " " + gold("Or"));
    }
    return lines;
}

// ---------------------------------------------------------------- Plateau

static void show_nobles(const Game& game)
{
    std::cout << bold(" NOBLES") << dim("  (3 pts, demandent des bonus ◆ = cartes possédées)") << "\n";
    std::vector<Block> blocks;
    for (const Noble& noble : game.nobles()) {
        blocks.push_back(noble_block(noble));
    }
    if (blocks.empty()) {
        std::cout << "   plus aucun noble\n";
    } else {
        print_row(blocks, " ");
    }
}

static void show_cards(const Game& game)
{
    // Les 3 rangées (niveau 3 en haut, comme sur la table), la réserve à droite
    std::vector<std::string> left;
    for (int level = LEVEL_COUNT; level >= 1; level--) {
        std::vector<Block> blocks;
        blocks.push_back(deck_block(level, game.deck_size(level)));
        const std::vector<Card>& row = game.visible(level);
        for (int i = 0; i < VISIBLE_PER_LEVEL; i++) {
            if (i < static_cast<int>(row.size())) {
                blocks.push_back(card_block(row[i], std::to_string(i + 1), game.can_buy(level, i + 1)));
            } else {
                blocks.push_back(empty_block());
            }
        }
        for (int line = 0; line < BLOCK_HEIGHT; line++) {
            std::string text;
            for (const Block& block : blocks) {
                text += block[line] + " ";
            }
            left.push_back(text);
        }
    }

    std::vector<std::string> bank = bank_lines(game);
    int bank_start = BLOCK_HEIGHT;  // la réserve commence en face du niveau II
    for (int i = 0; i < static_cast<int>(left.size()); i++) {
        std::cout << " " << left[i];
        int b = i - bank_start;
        if (b >= 0 && b < static_cast<int>(bank.size())) {
            std::cout << bank[b];
        }
        std::cout << "\n";
    }
}

// Une cellule de 6 caractères pour le tableau des joueurs ("·" pour zéro)
static std::string cell(int value)
{
    std::string text = value == 0 ? dim("·") : std::to_string(value);
    return pad("  " + text, 6);
}

static void show_players(const Game& game)
{
    const std::string short_names[] = {"Blanc", "Bleu", "Vert", "Rouge", "Noir"};
    std::string header = pad(bold(" JOUEURS"), 17);
    for (int c = 0; c < COLOR_COUNT; c++) {
        header += pad(colored(color_at(c), short_names[c]), 6);
    }
    if (game.mode() == Mode::Classic) {
        header += gold("Or");
    }
    std::cout << header << "\n";

    for (int i = 0; i < static_cast<int>(game.players().size()); i++) {
        const Player& player = game.players()[i];
        bool is_current = i == game.current_index();

        std::string name = (is_current ? player_colored(i, "▶ ") : "  ") + player_colored(i, player.name);
        std::string bonus_line = " " + pad(name, 9) + pad(" bonus", 7);
        std::string token_line = " " + pad("", 9) + pad(" jetons", 7);
        for (int c = 0; c < COLOR_COUNT; c++) {
            bonus_line += cell(player.bonuses()[c]);
            token_line += cell(player.tokens[c]);
        }
        if (game.mode() == Mode::Classic) {
            bonus_line += pad("", 4);
            token_line += pad(player.gold == 0 ? dim("·") : gold(std::to_string(player.gold)), 4);
        } else {
            bonus_line += " ";
            token_line += " ";
        }

        bonus_line += bold(std::to_string(player.score()) + " pts") + "  "
                      + std::to_string(player.cards.size()) + " cartes  "
                      + std::to_string(player.nobles.size()) + " nobles";
        token_line += std::to_string(player.token_count()) + "/" + std::to_string(MAX_TOKENS_IN_HAND)
                      + " jetons";
        if (game.mode() == Mode::Classic) {
            token_line += "  " + std::to_string(player.reserved.size()) + " réservée(s)";
        }
        std::cout << bonus_line << "\n" << token_line << "\n";
    }
}

static void show_reserved(const Game& game)
{
    const Player& me = game.current_player();
    if (game.mode() != Mode::Classic || me.reserved.empty()) {
        return;
    }
    std::cout << "\n" << bold(" VOS CARTES RÉSERVÉES") << dim("  (a r <n> pour acheter)") << "\n";
    std::vector<Block> blocks;
    for (int i = 0; i < static_cast<int>(me.reserved.size()); i++) {
        blocks.push_back(card_block(me.reserved[i], "R" + std::to_string(i + 1), game.can_buy_reserved(i + 1)));
    }
    print_row(blocks, " ");
}

void show_board(const Game& game)
{
    std::string mode = game.mode() == Mode::Classic ? "classique" : "rapide";
    std::string title = " SPLENDOR (" + mode + ") - tour " + std::to_string(game.round())
                        + " - au tour de ";
    std::cout << "\n" << repeat("━", 79) << "\n" << bold(title)
              << player_colored(game.current_index(), game.current_player().name) << "\n"
              << repeat("━", 79) << "\n";
    if (game.is_last_round()) {
        std::cout << bold(" DERNIER TOUR !") << "\n";
    }

    show_nobles(game);
    std::cout << "\n";
    show_cards(game);
    std::cout << "\n";
    show_players(game);
    show_reserved(game);

    std::cout << dim("\n * = carte achetable    ? = aide") << "\n";
}

void show_help(Mode mode)
{
    std::cout << "\nCommandes :\n"
              << "  j 1 3 5   prendre 3 jetons de couleurs différentes (ici Blanc, Vert, Noir)\n"
              << "  j 2 2     prendre 2 jetons de la même couleur (pile d'au moins 4)\n"
              << "  a 2 4     acheter la carte 4 du niveau 2\n";
    if (mode == Mode::Classic) {
        std::cout << "  a r 1     acheter votre carte réservée R1\n"
                  << "  r 2 4     réserver la carte 4 du niveau 2 (+1 or s'il en reste)\n"
                  << "  r 3 p     réserver la carte du dessus de la pioche du niveau 3\n";
    }
    std::cout << "  ?         afficher cette aide\n"
              << "  q         quitter la partie\n"
              << "Couleurs : 1=Blanc 2=Bleu 3=Vert 4=Rouge 5=Noir";
    if (mode == Mode::Classic) {
        std::cout << "\n           6=Or (seulement pour défausser)";
    }
    std::cout << "\n";
}

void show_results(const Game& game)
{
    std::cout << "\n" << repeat("━", 79) << "\n" << bold(" FIN DE PARTIE") << "\n" << repeat("━", 79) << "\n";
    if (game.is_blocked()) {
        std::cout << "Partie bloquée : plus aucun joueur ne peut jouer.\n";
    }
    for (int i = 0; i < static_cast<int>(game.players().size()); i++) {
        const Player& player = game.players()[i];
        std::cout << "  " << pad(player_colored(i, player.name), 15) << " : " << player.score() << " pts, "
                  << player.cards.size() << " cartes\n";
    }
    std::vector<int> winners = game.winners();
    if (winners.size() == 1) {
        std::cout << "\nVictoire de " << player_colored(winners[0], game.players()[winners[0]].name) << " !\n";
    } else {
        std::cout << "\nÉgalité parfaite entre :";
        for (int i : winners) {
            std::cout << " " << game.players()[i].name;
        }
        std::cout << "\n";
    }
}

// ---------------------------------------------------------------- Historique

// Liste "2 Blanc, 1 Vert" sans couleurs ANSI (utilisable par la fenêtre)
static std::string plain_tokens(const Resources& tokens, int gold_count)
{
    std::string text;
    for (int c = 0; c < COLOR_COUNT; c++) {
        if (tokens[c] > 0) {
            if (!text.empty()) {
                text += ", ";
            }
            text += std::to_string(tokens[c]) + " " + color_name(color_at(c));
        }
    }
    if (gold_count > 0) {
        if (!text.empty()) {
            text += ", ";
        }
        text += std::to_string(gold_count) + " Or";
    }
    return text;
}

static std::string plain_card(const Card& card)
{
    std::string text = "une carte " + color_name(card.provides);
    if (card.points > 0) {
        text += " (" + std::to_string(card.points) + (card.points > 1 ? " pts)" : " pt)");
    }
    return text;
}

std::string describe_event(const Game& game, const Event& event)
{
    std::string name = game.players()[event.player].name;
    switch (event.type) {
    case EventType::TakeTokens:
        return name + " prend " + plain_tokens(event.tokens, 0) + ".";
    case EventType::BuyCard:
        return name + " achète " + plain_card(event.card) + ".";
    case EventType::BuyReserved:
        return name + " achète sa réservée, " + plain_card(event.card) + ".";
    case EventType::Reserve: {
        std::string text = name + " réserve " + (event.from_deck ? "une carte de la pioche" : plain_card(event.card));
        return text + (event.gold > 0 ? " et prend un jeton or." : ".");
    }
    case EventType::Discard:
        return name + " défausse " + plain_tokens(event.tokens, event.gold) + ".";
    case EventType::NobleVisit:
        return event.noble + " rend visite à " + name + " (+3 pts).";
    case EventType::Pass:
        return name + " ne peut rien faire et passe son tour.";
    }
    return "";
}
