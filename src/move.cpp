#include "move.hpp"

std::string apply_move(Game& game, const Move& move)
{
    switch (move.type) {
    case MoveType::TakeTokens:
        return game.take_tokens(move.tokens);
    case MoveType::BuyCard:
        return game.buy_card(move.level, move.position);
    case MoveType::BuyReserved:
        return game.buy_reserved(move.position);
    case MoveType::Reserve:
        return game.reserve_card(move.level, move.position);
    case MoveType::Discard:
        if (move.gem == GOLD_GEM) {
            return game.discard_gold();
        }
        if (move.gem < 0 || move.gem >= COLOR_COUNT) {
            return "Couleur inconnue.";
        }
        return game.discard(color_at(move.gem));
    case MoveType::ChooseNoble:
        return game.choose_noble(move.noble);
    }
    return "Coup inconnu.";
}

Move take_move(const Resources& tokens)
{
    Move move;
    move.type = MoveType::TakeTokens;
    move.tokens = tokens;
    return move;
}

Move buy_move(int level, int position)
{
    Move move;
    move.type = MoveType::BuyCard;
    move.level = level;
    move.position = position;
    return move;
}

Move buy_reserved_move(int index)
{
    Move move;
    move.type = MoveType::BuyReserved;
    move.position = index;
    return move;
}

Move reserve_move(int level, int position)
{
    Move move;
    move.type = MoveType::Reserve;
    move.level = level;
    move.position = position;
    return move;
}

Move discard_move(int gem)
{
    Move move;
    move.type = MoveType::Discard;
    move.gem = gem;
    return move;
}

Move noble_move(int noble)
{
    Move move;
    move.type = MoveType::ChooseNoble;
    move.noble = noble;
    return move;
}
