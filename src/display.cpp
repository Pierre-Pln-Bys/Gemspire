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

std::string color_name(Gem color, Lang lang)
{
    switch (color) {
    case Gem::White:  return tr(lang, "White");
    case Gem::Blue:   return tr(lang, "Blue");
    case Gem::Green:  return tr(lang, "Green");
    case Gem::Red:    return tr(lang, "Red");
    case Gem::Black:  return tr(lang, "Black");
    }
    return "?";
}

std::string colored(Gem color, const std::string& text)
{
    switch (color) {
    case Gem::White:  return ansi("1;37", text);
    case Gem::Blue:   return ansi("1;34", text);
    case Gem::Green:  return ansi("1;32", text);
    case Gem::Red:    return ansi("1;31", text);
    case Gem::Black:  return ansi("1;90", text);  // gris foncé, lisible sur fond noir
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

std::string format_resources(const Resources& resources, Lang lang)
{
    std::string text;
    for (int c = 0; c < COLOR_COUNT; c++) {
        if (resources[c] == 0) {
            continue;
        }
        if (!text.empty()) {
            text += ", ";
        }
        text += colored(color_at(c), color_name(color_at(c), lang) + ": " + std::to_string(resources[c]));
    }
    if (text.empty()) {
        return tr(lang, "none");
    }
    return text;
}

std::string format_tokens(const Player& player, Lang lang)
{
    std::string text = format_resources(player.tokens, lang);
    if (player.gold > 0) {
        if (text == tr(lang, "none")) {
            text = "";
        } else {
            text += ", ";
        }
        text += gold(tr(lang, "Gold") + ": " + std::to_string(player.gold));
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
static Block card_block(const Card& card, const std::string& label, bool buyable, Lang lang)
{
    Gem c = card.provides;
    std::string side = colored(c, "│");
    Block block;
    block.push_back(colored(c, "┌" + repeat("─", CARD_INNER) + "┐"));

    std::string points = card.points > 0 ? bold(std::to_string(card.points)) : " ";
    std::string gem = colored(c, "◆ " + color_name(c, lang));
    int spaces = CARD_INNER - 1 - visible_width(points) - visible_width(gem);
    block.push_back(side + " " + points + std::string(spaces, ' ') + gem + side);

    // Coûts alignés en bas de la carte (au plus 4 couleurs)
    std::vector<std::string> costs;
    for (int i = 0; i < COLOR_COUNT; i++) {
        if (card.cost[i] > 0) {
            Gem cost_color = color_at(i);
            costs.push_back(" " + colored(cost_color, "●") + " " + std::to_string(card.cost[i])
                            + " " + colored(cost_color, color_name(cost_color, lang)));
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
static Block empty_block(Lang lang)
{
    Block block;
    for (int line = 0; line < BLOCK_HEIGHT; line++) {
        std::string content = line == 3 ? center(tr(lang, "(empty)"), CARD_INNER + 2) : std::string(CARD_INNER + 2, ' ');
        block.push_back(dim(content));
    }
    return block;
}

// La pioche d'un niveau, à gauche de la rangée
static Block deck_block(int level, int count, Lang lang)
{
    const std::string roman[] = {"I", "II", "III"};
    Block block;
    block.push_back("╔" + repeat("═", DECK_INNER) + "╗");
    block.push_back("║" + center(tr(lang, "LEVEL"), DECK_INNER) + "║");
    block.push_back("║" + center(bold(roman[level - 1]), DECK_INNER) + "║");
    block.push_back("║" + std::string(DECK_INNER, ' ') + "║");
    block.push_back("║" + center(tr(lang, "deck"), DECK_INNER) + "║");
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
static Block noble_block(const Noble& noble, Lang lang)
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
            Gem c = color_at(i);
            costs.push_back(" " + colored(c, "◆") + " " + std::to_string(noble.cost[i]) + " "
                            + colored(c, color_name(c, lang)));
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
static std::vector<std::string> bank_lines(const Game& game, Lang lang)
{
    std::vector<std::string> lines;
    lines.push_back(bold(tr(lang, "BANK")));
    lines.push_back("");
    for (int c = 0; c < COLOR_COUNT; c++) {
        Gem color = color_at(c);
        std::string count = std::to_string(game.bank()[c]);
        lines.push_back(dim(std::to_string(c + 1)) + " " + colored(color, "●") + " " + count + " "
                        + colored(color, color_name(color, lang)));
    }
    if (game.mode() == Mode::Classic) {
        lines.push_back("  " + gold("●") + " " + std::to_string(game.bank_gold()) + " " + gold(tr(lang, "Gold")));
    }
    return lines;
}

// ---------------------------------------------------------------- Plateau

static void show_nobles(const Game& game, Lang lang)
{
    std::cout << bold(" NOBLES") << dim(tr(lang, "  (3 pts, require bonuses ◆ = owned cards)")) << "\n";
    std::vector<Block> blocks;
    for (const Noble& noble : game.nobles()) {
        blocks.push_back(noble_block(noble, lang));
    }
    if (blocks.empty()) {
        std::cout << tr(lang, "   no nobles left") << "\n";
    } else {
        print_row(blocks, " ");
    }
}

static void show_cards(const Game& game, Lang lang)
{
    // Les 3 rangées (niveau 3 en haut, comme sur la table), la réserve à droite
    std::vector<std::string> left;
    for (int level = LEVEL_COUNT; level >= 1; level--) {
        std::vector<Block> blocks;
        blocks.push_back(deck_block(level, game.deck_size(level), lang));
        const std::vector<Card>& row = game.visible(level);
        for (int i = 0; i < VISIBLE_PER_LEVEL; i++) {
            if (i < static_cast<int>(row.size())) {
                blocks.push_back(card_block(row[i], std::to_string(i + 1), game.can_buy(level, i + 1), lang));
            } else {
                blocks.push_back(empty_block(lang));
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

    std::vector<std::string> bank = bank_lines(game, lang);
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

static void show_players(const Game& game, Lang lang)
{
    std::string header = pad(bold(tr(lang, " PLAYERS")), 17);
    for (int c = 0; c < COLOR_COUNT; c++) {
        header += pad(colored(color_at(c), color_name(color_at(c), lang)), 6);
    }
    if (game.mode() == Mode::Classic) {
        header += gold(tr(lang, "Gold"));
    }
    std::cout << header << "\n";

    for (int i = 0; i < static_cast<int>(game.players().size()); i++) {
        const Player& player = game.players()[i];
        bool is_current = i == game.current_index();

        std::string name = (is_current ? player_colored(i, "▶ ") : "  ") + player_colored(i, player.name);
        std::string bonus_line = " " + pad(name, 9) + pad(" bonus", 7);
        std::string token_line = " " + pad("", 9) + pad(tr(lang, " tokens"), 7);
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
                      + tr(lang, "{0} cards  {1} nobles",
                           {std::to_string(player.cards.size()), std::to_string(player.nobles.size())});
        token_line += std::to_string(player.token_count()) + "/" + std::to_string(MAX_TOKENS_IN_HAND)
                      + tr(lang, " tokens");
        if (game.mode() == Mode::Classic) {
            token_line += tr(lang, "  {0} reserved", {std::to_string(player.reserved.size())});
        }
        std::cout << bonus_line << "\n" << token_line << "\n";
    }
}

static void show_reserved(const Game& game, Lang lang)
{
    const Player& me = game.current_player();
    if (game.mode() != Mode::Classic || me.reserved.empty()) {
        return;
    }
    std::cout << "\n" << bold(tr(lang, " YOUR RESERVED CARDS")) << dim(tr(lang, "  (b r <n> to buy)")) << "\n";
    std::vector<Block> blocks;
    for (int i = 0; i < static_cast<int>(me.reserved.size()); i++) {
        blocks.push_back(card_block(me.reserved[i], "R" + std::to_string(i + 1), game.can_buy_reserved(i + 1), lang));
    }
    print_row(blocks, " ");
}

void show_board(const Game& game, Lang lang)
{
    std::string mode = tr(lang, game.mode() == Mode::Classic ? "classic" : "quick");
    std::string title = tr(lang, " GEMSPIRE ({0}) - round {1} - turn of ", {mode, std::to_string(game.round())});
    std::cout << "\n" << repeat("━", 79) << "\n" << bold(title)
              << player_colored(game.current_index(), game.current_player().name) << "\n"
              << repeat("━", 79) << "\n";
    if (game.is_last_round()) {
        std::cout << bold(tr(lang, " LAST ROUND!")) << "\n";
    }

    show_nobles(game, lang);
    std::cout << "\n";
    show_cards(game, lang);
    std::cout << "\n";
    show_players(game, lang);
    show_reserved(game, lang);

    std::cout << dim(tr(lang, "\n * = card you can buy    ? = help")) << "\n";
}

void show_help(Mode mode, Lang lang)
{
    // Les commandes françaises d'origine (j, a, r ... p) marchent aussi
    if (lang == Lang::French) {
        std::cout << "\nCommandes :\n"
                  << "  j 1 3 5   prendre 3 jetons de couleurs différentes (ici Blanc, Vert, Noir)\n"
                  << "  j 2 2     prendre 2 jetons de la même couleur (pile d'au moins 4)\n"
                  << "  a 2 4     acheter la carte 4 du niveau 2\n";
        if (mode == Mode::Classic) {
            std::cout << "  a r 1     acheter votre carte réservée R1\n"
                      << "  r 2 4     réserver la carte 4 du niveau 2 (+1 or s'il en reste)\n"
                      << "  r 3 p     réserver la carte du dessus de la pioche du niveau 3\n";
        }
        std::cout << "  s         sauvegarder la partie (s fichier.splendor pour choisir le nom)\n"
                  << "  ?         afficher cette aide\n"
                  << "  q         quitter la partie\n"
                  << "Couleurs : 1=Blanc 2=Bleu 3=Vert 4=Rouge 5=Noir";
        if (mode == Mode::Classic) {
            std::cout << "\n           6=Or (seulement pour défausser)";
        }
        std::cout << "\n";
        return;
    }
    std::cout << "\nCommands:\n"
              << "  t 1 3 5   take 3 tokens of different colours (here White, Green, Black)\n"
              << "  t 2 2     take 2 tokens of the same colour (stack of at least 4)\n"
              << "  b 2 4     buy card 4 of level 2\n";
    if (mode == Mode::Classic) {
        std::cout << "  b r 1     buy your reserved card R1\n"
                  << "  r 2 4     reserve card 4 of level 2 (+1 gold if any is left)\n"
                  << "  r 3 d     reserve the top card of the level 3 deck\n";
    }
    std::cout << "  s         save the game (s file.splendor to choose the name)\n"
              << "  ?         show this help\n"
              << "  q         quit the game\n"
              << "Colours: 1=White 2=Blue 3=Green 4=Red 5=Black";
    if (mode == Mode::Classic) {
        std::cout << "\n         6=Gold (only to discard)";
    }
    std::cout << "\n";
}

void show_results(const Game& game, Lang lang)
{
    std::cout << "\n" << repeat("━", 79) << "\n" << bold(tr(lang, " GAME OVER")) << "\n" << repeat("━", 79) << "\n";
    if (game.is_blocked()) {
        std::cout << tr(lang, "Game blocked: no player can move anymore.") << "\n";
    }
    for (int i = 0; i < static_cast<int>(game.players().size()); i++) {
        const Player& player = game.players()[i];
        std::cout << "  " << pad(player_colored(i, player.name), 15) << " : "
                  << tr(lang, "{0} pts, {1} cards", {std::to_string(player.score()), std::to_string(player.cards.size())})
                  << "\n";
    }
    std::vector<int> winners = game.winners();
    if (winners.size() == 1) {
        std::cout << "\n" << tr(lang, "{0} wins!", {player_colored(winners[0], game.players()[winners[0]].name)}) << "\n";
    } else {
        std::cout << "\n" << tr(lang, "Perfect tie between:");
        for (int i : winners) {
            std::cout << " " << game.players()[i].name;
        }
        std::cout << "\n";
    }
}

// ---------------------------------------------------------------- Historique

// Liste "2 White, 1 Green" sans couleurs ANSI (utilisable par la fenêtre)
static std::string plain_tokens(const Resources& tokens, int gold_count, Lang lang)
{
    std::string text;
    for (int c = 0; c < COLOR_COUNT; c++) {
        if (tokens[c] > 0) {
            if (!text.empty()) {
                text += ", ";
            }
            text += std::to_string(tokens[c]) + " " + color_name(color_at(c), lang);
        }
    }
    if (gold_count > 0) {
        if (!text.empty()) {
            text += ", ";
        }
        text += std::to_string(gold_count) + " " + tr(lang, "Gold");
    }
    return text;
}

static std::string plain_card(const Card& card, Lang lang)
{
    std::string text = tr(lang, "a {0} card", {color_name(card.provides, lang)});
    if (card.points > 0) {
        text += " (" + std::to_string(card.points) + (card.points > 1 ? " pts)" : " pt)");
    }
    return text;
}

std::string describe_event(const Game& game, const Event& event, Lang lang)
{
    std::string name = game.players()[event.player].name;
    switch (event.type) {
    case EventType::TakeTokens:
        return tr(lang, "{0} takes {1}.", {name, plain_tokens(event.tokens, 0, lang)});
    case EventType::BuyCard:
        return tr(lang, "{0} buys {1}.", {name, plain_card(event.card, lang)});
    case EventType::BuyReserved:
        return tr(lang, "{0} buys their reserved card, {1}.", {name, plain_card(event.card, lang)});
    case EventType::Reserve: {
        std::string card = event.from_deck ? tr(lang, "a card from the deck") : plain_card(event.card, lang);
        return tr(lang, event.gold > 0 ? "{0} reserves {1} and takes a gold token." : "{0} reserves {1}.", {name, card});
    }
    case EventType::Discard:
        return tr(lang, "{0} discards {1}.", {name, plain_tokens(event.tokens, event.gold, lang)});
    case EventType::NobleVisit:
        return tr(lang, "{0} visits {1} (+3 pts).", {event.noble, name});
    case EventType::Pass:
        return tr(lang, "{0} can't do anything and passes.", {name});
    case EventType::Timeout:
        return tr(lang, "{0} didn't play in time and passes.", {name});
    case EventType::Forfeit:
        return tr(lang, "{0} forfeits the game (ranked last).", {name});
    }
    return "";
}
