#include "move_json.hpp"

nlohmann::json move_to_json(const Move& move)
{
    nlohmann::json json;
    switch (move.type) {
    case MoveType::TakeTokens:
        json["type"] = "take";
        json["tokens"] = move.tokens;
        break;
    case MoveType::BuyCard:
        json["type"] = "buy";
        json["level"] = move.level;
        json["position"] = move.position;
        break;
    case MoveType::BuyReserved:
        json["type"] = "buy_reserved";
        json["index"] = move.position;
        break;
    case MoveType::Reserve:
        json["type"] = "reserve";
        json["level"] = move.level;
        json["position"] = move.position;
        break;
    case MoveType::Discard:
        json["type"] = "discard";
        json["gem"] = move.gem;
        break;
    case MoveType::ChooseNoble:
        json["type"] = "noble";
        json["noble"] = move.noble;
        break;
    }
    return json;
}

// Lit un entier entre min et max ; false si absent ou invalide
static bool read_int(const nlohmann::json& json, const char* key, int min, int max, int& value)
{
    if (!json.contains(key) || !json[key].is_number_integer()) {
        return false;
    }
    long long read = json[key].get<long long>();
    if (read < min || read > max) {
        return false;
    }
    value = static_cast<int>(read);
    return true;
}

std::string move_from_json(const nlohmann::json& json, Move& move)
{
    if (!json.is_object() || !json.contains("type") || !json["type"].is_string()) {
        return "Coup mal formé.";
    }
    std::string type = json["type"].get<std::string>();
    move = Move();
    if (type == "take") {
        move.type = MoveType::TakeTokens;
        const nlohmann::json& tokens = json.contains("tokens") ? json["tokens"] : nlohmann::json();
        if (!tokens.is_array() || tokens.size() != COLOR_COUNT) {
            return "Jetons mal formés.";
        }
        for (int c = 0; c < COLOR_COUNT; c++) {
            if (!tokens[c].is_number_integer() || tokens[c].get<long long>() < 0 || tokens[c].get<long long>() > 3) {
                return "Jetons mal formés.";
            }
            move.tokens[c] = tokens[c].get<int>();
        }
        return "";
    }
    if (type == "buy" || type == "reserve") {
        move.type = type == "buy" ? MoveType::BuyCard : MoveType::Reserve;
        if (!read_int(json, "level", 1, LEVEL_COUNT, move.level) || !read_int(json, "position", 0, VISIBLE_PER_LEVEL, move.position)) {
            return "Carte mal désignée.";
        }
        return "";
    }
    if (type == "buy_reserved") {
        move.type = MoveType::BuyReserved;
        return read_int(json, "index", 1, MAX_RESERVED, move.position) ? "" : "Carte réservée mal désignée.";
    }
    if (type == "discard") {
        move.type = MoveType::Discard;
        return read_int(json, "gem", 0, GOLD_GEM, move.gem) ? "" : "Jeton mal désigné.";
    }
    if (type == "noble") {
        move.type = MoveType::ChooseNoble;
        return read_int(json, "noble", 0, 9, move.noble) ? "" : "Noble mal désigné.";
    }
    return "Type de coup inconnu.";
}
