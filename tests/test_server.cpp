// Tests du serveur en ligne : lance un vrai serveur (connexion de test), deux
// joueurs se connectent, refusent de mauvais coups, jouent une partie complète
// avec l'IA, et on vérifie Elo, classement et informations cachées.
// Lancer avec : make test-server
#include <cassert>
#include <cstdio>
#include <iostream>
#include <chrono>
#include <thread>

#include "../server/app.hpp"
#include "../src/ai.hpp"
#include "../src/move_json.hpp"
#include "../src/save.hpp"

using nlohmann::json;

const int PORT = 18431;

static json get(httplib::Client& client, const std::string& path, const std::string& token, int expected = 200)
{
    httplib::Headers headers = {{"Authorization", "Bearer " + token}};
    auto response = client.Get(path, headers);
    assert(response);
    if (response->status != expected) {
        std::cerr << "GET " << path << " : " << response->status << " " << response->body << "\n";
    }
    assert(response->status == expected);
    return json::parse(response->body, nullptr, false);
}

static json post(httplib::Client& client, const std::string& path, const std::string& token, const json& body,
                 int expected = 200)
{
    httplib::Headers headers = {{"Authorization", "Bearer " + token}};
    auto response = client.Post(path, headers, body.dump(), "application/json");
    assert(response);
    if (response->status != expected) {
        std::cerr << "POST " << path << " : " << response->status << " " << response->body << "\n";
    }
    assert(response->status == expected);
    return json::parse(response->body, nullptr, false);
}

// Connexion de test : comme le jeu, on ouvre /auth/dev/start puis on interroge /auth/poll
static std::string login(httplib::Client& client, const std::string& name, const std::string& login_id,
                         const std::string& as = "")
{
    auto page = client.Get("/auth/dev/start?login=" + login_id + "&name=" + name + (as.empty() ? "" : "&as=" + as));
    assert(page && page->status == 200);
    json poll = get(client, "/auth/poll?login=" + login_id, "");
    assert(poll["status"] == "done");
    // Le jeton n'est donné qu'une fois
    assert(get(client, "/auth/poll?login=" + login_id, "")["status"] == "pending");
    return poll["token"];
}

int main()
{
    std::remove("tests/test_server.db");
    ServerConfig config;
    config.port = PORT;
    config.database = "tests/test_server.db";
    config.dev_login = true;
    config.wait_seconds = 1;
    config.bot_key = "cle-de-test-du-bot";
    config.turn_seconds = 1;  // l'horloge n'avance que quand le test appelle app.tick()
    ServerApp app(config);
    assert(app.ok());
    httplib::Server server;
    app.setup_routes(server);
    std::thread thread([&]() { server.listen("127.0.0.1", PORT); });
    server.wait_until_ready();

    httplib::Client client("127.0.0.1", PORT);
    client.set_read_timeout(5);

    // --- Comptes et pseudos
    std::string alice = login(client, "alice", "aaaaaaaaaaaaaaaaaaaaaaaa");
    std::string bob = login(client, "bob", "bbbbbbbbbbbbbbbbbbbbbbbb");
    get(client, "/me", "faux-jeton", 401);
    assert(get(client, "/me", alice)["pseudo"] == "");
    post(client, "/lobbies", alice, {{"mode", "rapide"}, {"seats", 2}}, 403);  // pas encore de pseudo
    post(client, "/me/pseudo", alice, {{"pseudo", "a"}}, 400);                 // trop court
    post(client, "/me/pseudo", alice, {{"pseudo", "Alice<script>"}}, 400);     // caractères interdits
    post(client, "/me/pseudo", alice, {{"pseudo", "Alice"}});
    post(client, "/me/pseudo", bob, {{"pseudo", "alice"}}, 400);               // déjà pris (sans casse)
    post(client, "/me/pseudo", bob, {{"pseudo", "Bob l'éponge"}}, 400);        // apostrophe interdite
    post(client, "/me/pseudo", bob, {{"pseudo", "Bébert"}});
    assert(get(client, "/me", bob)["elo"] == 1000);

    // --- Salon
    std::string code = post(client, "/lobbies", alice, {{"mode", "rapide"}, {"seats", 2}})["code"];
    assert(code.size() == 5);
    post(client, "/lobbies/ZZZZZ/join", bob, json::object(), 400);
    post(client, "/lobbies/" + code + "/start", alice, json::object(), 400);  // seule
    post(client, "/lobbies/" + code + "/join", bob, json::object());
    post(client, "/lobbies/" + code + "/start", bob, json::object(), 400);    // pas l'hôte
    json lobby = get(client, "/lobbies/" + code, bob);
    assert(lobby["members"].size() == 2);
    int game_id = post(client, "/lobbies/" + code + "/start", alice, json::object())["game_id"];
    assert(game_id > 0 && get(client, "/lobbies/" + code, alice)["game_id"] == game_id);

    // --- Coups invalides
    std::string path = "/games/" + std::to_string(game_id);
    json state = get(client, path, alice);
    int first = state["your_seat"] == 0 ? 0 : 1;
    assert(first == 0);
    post(client, path + "/move", bob, {{"type", "take"}, {"tokens", {1, 1, 1, 0, 0}}}, 400);  // pas son tour
    post(client, path + "/move", alice, {{"type", "take"}, {"tokens", {3, 0, 0, 0, 0}}}, 400);  // règles
    post(client, path + "/move", alice, {{"type", "voler"}}, 400);
    auto raw = client.Post(path + "/move", {{"Authorization", "Bearer " + alice}}, "{pas du json", "application/json");
    assert(raw && raw->status == 400);

    // Attente longue : sans changement, la réponse arrive après le délai avec la même version
    long version = state["version"];
    assert(get(client, path + "?since=" + std::to_string(version), alice)["version"] == version);

    // --- Partie complète : chaque joueur joue le coup de l'IA moyenne sur SA vue
    std::string tokens[2] = {alice, bob};
    int moves = 0;
    while (true) {
        json view = get(client, path, tokens[0]);
        if (view["over"]) {
            break;
        }
        std::unique_ptr<Game> mine;
        assert(load_game(view["save"], mine).empty());
        int seat = mine->current_index();
        json seat_view = get(client, path, tokens[seat]);
        std::unique_ptr<Game> game;
        assert(load_game(seat_view["save"], game).empty());
        Move move = ai_choose(*game, true);
        post(client, path + "/move", tokens[seat], move_to_json(move));
        assert(++moves < 2000);
    }
    json final_state = get(client, path, alice);
    assert(final_state["over"] == true);
    std::vector<int> changes = final_state["elo_changes"];
    assert(changes.size() == 2 && changes[0] + changes[1] >= -1 && changes[0] + changes[1] <= 1);
    assert(changes[0] != 0);
    post(client, path + "/move", alice, {{"type", "take"}, {"tokens", {1, 1, 1, 0, 0}}}, 400);  // terminée

    json leaderboard = get(client, "/leaderboard", "");
    assert(leaderboard["players"].size() == 2);
    assert(leaderboard["players"][0]["games"] == 1);
    int elo_alice = get(client, "/me", alice)["elo"];
    assert(elo_alice == 1000 + changes[0]);

    // --- Temps écoulé et abandon : 3 tours manqués d'affilée = forfait
    {
        std::string timed = post(client, "/lobbies", alice, {{"mode", "rapide"}, {"seats", 2}})["code"];
        post(client, "/lobbies/" + timed + "/join", bob, json::object());
        int timed_id = post(client, "/lobbies/" + timed + "/start", alice, json::object())["game_id"];
        std::string timed_path = "/games/" + std::to_string(timed_id);
        json view = get(client, timed_path, alice);
        assert(view.contains("turn_seconds_left"));
        auto expire = [&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(1100));
            app.tick();
        };
        expire();  // Alice (1 tour manqué)
        view = get(client, timed_path, alice);
        assert(view["missed"][0] == 1 && view["log"].back().get<std::string>().find("à temps") != std::string::npos);
        post(client, timed_path + "/move", bob, {{"type", "take"}, {"tokens", {1, 1, 1, 0, 0}}});
        expire();  // Alice (2)
        expire();  // Bob (1)
        expire();  // Alice (3) : abandon, Bob gagne
        view = get(client, timed_path, alice);
        assert(view["over"] == true && view["missed"][0] == 3 && view["missed"][1] == 1);
        std::vector<int> timed_changes = view["elo_changes"];
        assert(timed_changes[0] < 0 && timed_changes[1] > 0);

        // Abandon volontaire
        std::string quit = post(client, "/lobbies", alice, {{"mode", "classique"}, {"seats", 2}})["code"];
        post(client, "/lobbies/" + quit + "/join", bob, json::object());
        int quit_id = post(client, "/lobbies/" + quit + "/start", alice, json::object())["game_id"];
        std::string quit_path = "/games/" + std::to_string(quit_id);
        post(client, quit_path + "/forfeit", "faux-jeton", json::object(), 401);
        post(client, quit_path + "/forfeit", bob, json::object());
        json quit_view = get(client, quit_path, alice);
        assert(quit_view["over"] == true);
        std::vector<int> quit_changes = quit_view["elo_changes"];
        assert(quit_changes[0] > 0 && quit_changes[1] < 0);
        post(client, quit_path + "/forfeit", alice, json::object(), 400);  // déjà finie
    }

    // --- Informations cachées : Bob ne voit ni les pioches ni les réservées d'Alice
    std::string bob_text = get(client, path, bob)["save"];
    std::string hidden = "card 0 0 0 0 0 0 0";
    // Entre « deck L » et « visible L », toutes les cartes sont cachées (une pioche
    // peut être vide en fin de partie : on regarde les trois)
    size_t count = 0;
    for (int level = 1; level <= 3; level++) {
        size_t deck = bob_text.find("deck " + std::to_string(level) + " ");
        size_t visible = bob_text.find("visible " + std::to_string(level) + " ");
        assert(deck != std::string::npos && visible != std::string::npos);
        std::string deck_part = bob_text.substr(deck, visible - deck);
        for (size_t at = deck_part.find("card "); at != std::string::npos; at = deck_part.find("card ", at + 1)) {
            assert(deck_part.compare(at, hidden.size(), hidden) == 0);
            count++;
        }
    }
    assert(count > 0);

    // --- Discord : présence, proposition du bot, salon lié au vocal
    httplib::Headers bot = {{"X-Bot-Key", "cle-de-test-du-bot"}};
    httplib::Headers bad_bot = {{"X-Bot-Key", "mauvaise-cle-du-bot"}};
    auto refused = client.Get("/bot/presence?discord_id=123456", bad_bot);
    assert(refused && refused->status == 403);
    auto unknown = client.Get("/bot/presence?discord_id=123456", bot);
    assert(unknown && json::parse(unknown->body)["linked"] == false);

    std::string carol = login(client, "123456", "cccccccccccccccccccccccc", "discord");
    post(client, "/me/pseudo", carol, {{"pseudo", "Carole"}});
    json first_notes = get(client, "/me/notifications", carol);  // présence, sans attendre
    long last_note = first_notes["last"];
    auto presence = client.Get("/bot/presence?discord_id=123456", bot);
    json presence_body = json::parse(presence->body);
    assert(presence_body["linked"] == true && presence_body["online"] == true && presence_body["pseudo"] == "Carole");

    auto offer = client.Post("/bot/offer", bot,
                             json({{"discord_id", "123456"}, {"voice_channel", "987654321"}, {"channel_name", "Partie de Carole"}}).dump(),
                             "application/json");
    assert(offer && json::parse(offer->body)["offered"] == true);
    json notes = get(client, "/me/notifications?since=" + std::to_string(last_note), carol);
    assert(notes["notifications"].size() == 1);
    assert(notes["notifications"][0]["type"] == "voice_offer");
    assert(notes["notifications"][0]["voice_channel"] == "987654321");

    post(client, "/lobbies", carol, {{"mode", "classique"}, {"seats", 3}, {"voice_channel", "abc"}}, 400);
    std::string voice_code = post(client, "/lobbies", carol,
                                  {{"mode", "classique"}, {"seats", 3}, {"voice_channel", "987654321"}})["code"];
    auto events = client.Get("/bot/events?since=0", bot);
    json events_body = json::parse(events->body);
    assert(!events_body["events"].empty());
    json last_event = events_body["events"].back();
    assert(last_event["code"] == voice_code && last_event["voice_channel"] == "987654321");
    assert(last_event["seats"] == 3 && last_event["players"] == 1 && last_event["closed"] == false);
    post(client, "/lobbies/" + voice_code + "/leave", carol, json::object());
    events = client.Get("/bot/events?since=" + std::to_string(events_body["last"].get<long>()), bot);
    assert(json::parse(events->body)["events"].back()["closed"] == true);
    assert(get(client, "/config", "")["discord_app_id"] == "");

    // Déconnexion
    post(client, "/auth/logout", alice, json::object());
    get(client, "/me", alice, 401);

    server.stop();
    thread.join();
    std::remove("tests/test_server.db");
    std::remove("tests/test_server.db-wal");
    std::remove("tests/test_server.db-shm");
    std::cout << "Serveur : tous les tests passent (" << moves << " coups joués en ligne).\n";
    return 0;
}
