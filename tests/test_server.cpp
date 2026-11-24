// Tests du serveur en ligne : lance un vrai serveur (connexion de test), deux
// joueurs se connectent, refusent de mauvais coups, jouent une partie complète
// avec l'IA, et on vérifie Elo, classement et informations cachées.
// Lancer avec : make test-server
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <iostream>
#include <chrono>
#include <thread>

#include "../server/app.hpp"
#include "../src/ai.hpp"
#include "../src/elo.hpp"
#include "../src/move_json.hpp"
#include "../src/save.hpp"
#include "../src/version.hpp"

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

// Connexion de test : comme le jeu, on ouvre /auth/dev/start (avec un secret),
// puis on interroge /auth/poll en présentant ce même secret.
static std::string login(httplib::Client& client, const std::string& name, const std::string& login_id,
                         const std::string& as = "")
{
    std::string verifier = "v" + login_id;  // secret quelconque (alphanumérique)
    auto page = client.Get("/auth/dev/start?login=" + login_id + "&name=" + name
                           + "&verifier=" + verifier + (as.empty() ? "" : "&as=" + as));
    assert(page && page->status == 200);
    json poll = get(client, "/auth/poll?login=" + login_id + "&verifier=" + verifier, "");
    assert(poll["status"] == "done");
    // Le jeton n'est donné qu'une fois
    assert(get(client, "/auth/poll?login=" + login_id + "&verifier=" + verifier, "")["status"] == "pending");
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
    config.min_version = SPLENDOR_VERSION;
    config.turn_seconds = 1;  // l'horloge n'avance que quand le test appelle app.tick()
    config.admin_discord_ids = {"777"};
    config.rate_limit = 0;  // pas de limite de débit : le test enchaîne beaucoup de requêtes
    ServerApp app(config);
    assert(app.ok());
    httplib::Server server;
    app.setup_routes(server);
    std::thread thread([&]() { server.listen("127.0.0.1", PORT); });
    server.wait_until_ready();

    // --- Version du jeu : sans en-tête ou trop ancienne, refusé (426) ; le site,
    //     le classement public et le bot ne sont pas concernés
    {
        httplib::Client old_game("127.0.0.1", PORT);
        auto refused_config = old_game.Get("/config");
        assert(refused_config && refused_config->status == 426);
        assert(json::parse(refused_config->body)["min_version"] == SPLENDOR_VERSION);
        auto too_old = old_game.Get("/me", {{"X-Splendor-Version", "1.0.0"}});
        assert(too_old && too_old->status == 426);
        auto garbage = old_game.Get("/me", {{"X-Splendor-Version", "9.x.0"}});
        assert(garbage && garbage->status == 426);
        auto newer = old_game.Get("/config", {{"X-Splendor-Version", "99.0.0"}});
        assert(newer && newer->status == 200);
        assert(old_game.Get("/leaderboard")->status == 200);
        assert(old_game.Get("/")->status == 200);
        assert(old_game.Get("/health")->status == 200);
        // Icônes du site, accessibles sans en-tête de version (navigateurs)
        auto icon = old_game.Get("/favicon.ico");
        assert(icon && icon->status == 200 && icon->get_header_value("Content-Type") == "image/x-icon");
        auto touch_icon = old_game.Get("/apple-touch-icon.png");
        assert(touch_icon && touch_icon->status == 200 && touch_icon->body.substr(1, 3) == "PNG");
        assert(old_game.Get("/")->body.find("/favicon.ico") != std::string::npos);
    }

    httplib::Client client("127.0.0.1", PORT);
    client.set_read_timeout(5);
    client.set_default_headers({{"X-Splendor-Version", SPLENDOR_VERSION}});

    // --- Comptes et pseudos
    std::string alice = login(client, "alice", "aaaaaaaaaaaaaaaaaaaaaaaa");
    std::string bob = login(client, "bob", "bbbbbbbbbbbbbbbbbbbbbbbb");

    // --- H1 : prise de compte. /auth/poll ne remet le jeton qu'avec le bon secret ;
    //     connaître l'identifiant de connexion seul (lien, Referer, journaux) ne suffit plus.
    {
        std::string lid = "hhhhhhhhhhhhhhhhhhhhhhhh";
        std::string secret = "topsecrettopsecret123456";
        assert(client.Get("/auth/dev/start?login=" + lid + "&name=mallory&verifier=" + secret)->status == 200);
        // Sans secret, ou avec un mauvais secret : le jeton n'est pas donné (reste « pending »)
        assert(get(client, "/auth/poll?login=" + lid, "")["status"] == "pending");
        assert(get(client, "/auth/poll?login=" + lid + "&verifier=mauvaismauvaismauvais1", "")["status"] == "pending");
        // Avec le bon secret : jeton remis et utilisable
        json done = get(client, "/auth/poll?login=" + lid + "&verifier=" + secret, "");
        assert(done["status"] == "done" && done.contains("token"));
        assert(get(client, "/me", done["token"].get<std::string>())["id"] != 0);
        // Jeton donné une seule fois (l'entrée est ensuite effacée)
        assert(get(client, "/auth/poll?login=" + lid + "&verifier=" + secret, "")["status"] == "pending");
        // Un /auth/<fournisseur>/start non préparé par /auth/begin ne redirige pas vers l'OAuth
        auto not_begun = client.Get("/auth/discord/start?login=zzzzzzzzzzzzzzzzzzzzzzzz");
        assert(not_begun && not_begun->status != 302);
    }

    // --- H1 (suite) : /auth/begin prépare le secret ; /auth/start redirige alors vers
    //     l'OAuth, sans prompt=none. Serveur isolé (client Discord factice) : begin et
    //     start ne font aucun appel réseau (seul le callback en ferait).
    {
        std::remove("tests/test_server_oauth.db");
        ServerConfig oauth_config;
        oauth_config.port = PORT + 1;
        oauth_config.database = "tests/test_server_oauth.db";
        oauth_config.discord_client_id = "fake-discord-client";
        oauth_config.public_url = "http://127.0.0.1";
        oauth_config.min_version = SPLENDOR_VERSION;
        ServerApp oauth_app(oauth_config);
        assert(oauth_app.ok());
        httplib::Server oauth_server;
        oauth_app.setup_routes(oauth_server);
        std::thread oauth_thread([&]() { oauth_server.listen("127.0.0.1", PORT + 1); });
        oauth_server.wait_until_ready();
        httplib::Client oauth("127.0.0.1", PORT + 1);
        oauth.set_default_headers({{"X-Splendor-Version", SPLENDOR_VERSION}});
        oauth.set_follow_location(false);

        std::string lid = "oauthoauthoauthoauth1234";
        std::string secret = "secretsecretsecret567890";
        // start avant begin : refusé (pas de redirection)
        assert(oauth.Get("/auth/discord/start?login=" + lid)->status != 302);
        // begin enregistre le secret et renvoie l'identifiant
        auto begin_json = json({{"provider", "discord"}, {"login", lid}, {"verifier", secret}}).dump();
        auto begun = oauth.Post("/auth/begin", begin_json, "application/json");
        assert(begun && begun->status == 200 && json::parse(begun->body)["login"] == lid);
        // start après begin : redirige vers Discord, avec le state, et SANS prompt=none
        auto redirect = oauth.Get("/auth/discord/start?login=" + lid);
        assert(redirect && redirect->status == 302);
        std::string location = redirect->get_header_value("Location");
        assert(location.find("discord.com") != std::string::npos);
        assert(location.find("state=" + lid) != std::string::npos);
        assert(location.find("prompt=none") == std::string::npos);
        // begin refuse un identifiant invalide (trop court)
        auto bad = json({{"provider", "discord"}, {"login", "court"}, {"verifier", secret}}).dump();
        assert(oauth.Post("/auth/begin", bad, "application/json")->status == 400);

        oauth_server.stop();
        oauth_thread.join();
    }

    // --- L5 : limitation de débit par IP et plafond de connexions en attente.
    //     Serveur isolé avec des limites basses (le serveur principal ne limite pas).
    {
        std::remove("tests/test_server_rate.db");
        ServerConfig rate_config;
        rate_config.port = PORT + 2;
        rate_config.database = "tests/test_server_rate.db";
        rate_config.dev_login = true;
        rate_config.discord_client_id = "fake-discord-client";
        rate_config.min_version = SPLENDOR_VERSION;
        rate_config.rate_limit = 5;        // 5 requêtes/minute par IP
        rate_config.max_pending_logins = 2;  // au plus 2 connexions en attente
        ServerApp rate_app(rate_config);
        assert(rate_app.ok());
        httplib::Server rate_server;
        rate_app.setup_routes(rate_server);
        std::thread rate_thread([&]() { rate_server.listen("127.0.0.1", PORT + 2); });
        rate_server.wait_until_ready();
        httplib::Client rc("127.0.0.1", PORT + 2);
        rc.set_default_headers({{"X-Splendor-Version", SPLENDOR_VERSION}});

        auto begin = [&](const std::string& id) {
            auto body = json({{"provider", "discord"}, {"login", id}, {"verifier", "verifierverifier1234"}}).dump();
            return rc.Post("/auth/begin", body, "application/json");
        };
        // Plafond : 2 connexions en attente acceptées, la 3e est refusée (429)
        assert(begin("rate0000000000000000")->status == 200);
        assert(begin("rate1111111111111111")->status == 200);
        auto capped = begin("rate2222222222222222");
        assert(capped && capped->status == 429
               && json::parse(capped->body)["error"].get<std::string>().find("in progress") != std::string::npos);
        // Débit : 5 requêtes/minute par IP (3 begin déjà faits) ; la 6e requête est refusée
        assert(rc.Get("/auth/poll?login=rate0000000000000000")->status == 200);  // 4e
        assert(rc.Get("/auth/poll?login=rate0000000000000000")->status == 200);  // 5e
        auto throttled = rc.Get("/auth/poll?login=rate0000000000000000");        // 6e
        assert(throttled && throttled->status == 429
               && json::parse(throttled->body)["error"].get<std::string>().find("Slow down") != std::string::npos);

        rate_server.stop();
        rate_thread.join();
    }
    get(client, "/me", "faux-jeton", 401);
    assert(get(client, "/me", alice)["pseudo"] == "");
    post(client, "/lobbies", alice, {{"mode", "rapide"}, {"seats", 2}}, 403);  // pas encore de pseudo
    // Activité Discord : requête mal formée refusée (pas d'application Discord configurée dans le test)
    post(client, "/auth/discord/activity", "", json::object(), 400);
    post(client, "/auth/discord/activity", "", {{"code", "abc"}}, 400);
    post(client, "/me/pseudo", alice, {{"pseudo", "a"}}, 400);                 // trop court
    post(client, "/me/pseudo", alice, {{"pseudo", "Alice<script>"}}, 400);     // caractères interdits
    post(client, "/me/pseudo", alice, {{"pseudo", "Alice"}});
    post(client, "/me/pseudo", bob, {{"pseudo", "alice"}}, 400);               // déjà pris (sans casse)
    post(client, "/me/pseudo", bob, {{"pseudo", "Bob l'éponge"}}, 400);        // apostrophe interdite
    post(client, "/me/pseudo", bob, {{"pseudo", "Bébert"}});
    assert(get(client, "/me", bob)["elo"] == 1000);

    // --- Amis : demande, liste des deux côtés, acceptation, statut, retrait
    post(client, "/friends/add", alice, {{"pseudo", "personne"}}, 404);
    post(client, "/friends/add", alice, {{"pseudo", "alice"}}, 400);               // soi-même
    post(client, "/friends/add", alice, {{"pseudo", "bébert"}});
    assert(get(client, "/friends", alice)["friends"][0]["relation"] == "outgoing");
    assert(get(client, "/friends", bob)["friends"][0]["relation"] == "incoming");
    post(client, "/friends/add", bob, {{"pseudo", "Alice"}});
    {
        json mine = get(client, "/friends", alice)["friends"];
        assert(mine.size() == 1 && mine[0]["relation"] == "friend" && mine[0]["pseudo"] == "Bébert");
        assert(mine[0]["status"] == "online");                                       // il vient de jouer une requête
    }
    post(client, "/friends/remove", bob, {{"pseudo", "Alice"}});
    assert(get(client, "/friends", alice)["friends"].empty());

    // --- Activité Discord : la même instance donne le même salon (4 places)
    std::string shared = post(client, "/lobbies/activity", alice, {{"instance", "i-123"}})["code"];
    assert(post(client, "/lobbies/activity", bob, {{"instance", "i-123"}})["code"] == shared);
    assert(get(client, "/lobbies/" + shared, alice)["members"].size() == 2);
    assert(post(client, "/lobbies/activity", alice, {{"instance", "autre"}})["code"] != shared);
    post(client, "/lobbies/activity", alice, {{"instance", "<x>"}}, 400);
    post(client, "/lobbies/" + shared + "/leave", bob, json::object());

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
    // Parties à regarder, et statut « en partie » (avec le numéro) pour les amis
    {
        json live = get(client, "/games/live", bob)["games"];
        assert(!live.empty() && live[0]["id"] == game_id && live[0]["players"].size() == 2);
        post(client, "/friends/add", alice, {{"pseudo", "Bébert"}});
        post(client, "/friends/add", bob, {{"pseudo", "Alice"}});
        json friends = get(client, "/friends", alice)["friends"];
        assert(friends[0]["status"] == "playing" && friends[0]["game_id"] == game_id);
        post(client, "/friends/remove", alice, {{"pseudo", "Bébert"}});
    }

    // --- Coups invalides
    std::string path = "/games/" + std::to_string(game_id);
    // --- Chat : texte nettoyé, réaction, refus (vide, trop long, spam, pas joueur)
    post(client, path + "/chat", alice, {{"text", "  Bonne chance\n!  "}});
    post(client, path + "/chat", bob, {{"reaction", 0}});
    post(client, path + "/chat", alice, {{"text", "encore"}}, 400);              // trop vite
    post(client, path + "/chat", bob, {{"text", "   "}}, 400);
    post(client, path + "/chat", bob, {{"text", std::string(121, 'x')}}, 400);
    post(client, path + "/chat", bob, {{"reaction", REACTION_COUNT}}, 400);
    {
        // Pas de l'UTF-8 : envoyé brut (la bibliothèque JSON refuse de l'écrire)
        auto raw_chat = client.Post(path + "/chat", {{"Authorization", "Bearer " + bob}}, "{\"text\": \"\xff\xfe\"}",
                                    "application/json");
        assert(raw_chat && raw_chat->status == 400);
    }
    {
        json chat = get(client, path, bob)["chat"];
        assert(chat.size() == 2 && chat[0]["text"] == "Bonne chance !" && chat[0]["seat"] == 0);
        assert(chat[1]["reaction"] == 0 && chat[1]["seat"] == 1);
    }
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
    // Elo modifié, sauf égalité parfaite (même place pour les deux)
    {
        std::unique_ptr<Game> ended;
        assert(load_game(final_state["save"], ended).empty());
        std::vector<int> ended_places = final_places(*ended);
        assert((ended_places[0] == ended_places[1]) == (changes[0] == 0));
    }
    post(client, path + "/move", alice, {{"type", "take"}, {"tokens", {1, 1, 1, 0, 0}}}, 400);  // terminée
    // Rediffusion : rejouer les actions enregistrées redonne exactement la même fin
    {
        int replay_id = final_state["replay_id"];
        assert(replay_id > 0);
        json replay = get(client, "/replays/" + std::to_string(replay_id), bob);
        std::unique_ptr<Game> replayed;
        int count = 0;
        assert(replay_game(replay, -1, replayed, count).empty() && count >= moves && replayed->is_over());
        std::unique_ptr<Game> served;
        assert(load_game(final_state["save"], served).empty());
        assert(replayed->round() == served->round());
        for (int p = 0; p < 2; p++) {
            assert(replayed->players()[p].score() == served->players()[p].score());
            assert(replayed->players()[p].cards.size() == served->players()[p].cards.size());
        }
        assert(replay_game(replay, 3, replayed, count).empty() && !replayed->is_over());
        json mine = get(client, "/me/games", alice)["games"];
        assert(mine[0]["id"] == replay_id && mine[0]["replay"] == true);
        get(client, "/replays/999999", bob, 404);
    }
    // Succès : « first_game » pour les deux, montré dans la partie et dans /me
    {
        json finished = get(client, path, alice);
        assert(finished["achievements"].size() == 2);
        json mine = get(client, "/me", alice)["achievements"];
        assert(std::find(mine.begin(), mine.end(), "first_game") != mine.end());
        assert(std::find(finished["achievements"][0].begin(), finished["achievements"][0].end(), "first_game")
               != finished["achievements"][0].end());
    }

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
        assert(view["missed"][0] == 1 && view["log"].back().get<std::string>().find("in time") != std::string::npos
           && view["log_fr"].back().get<std::string>().find("à temps") != std::string::npos);
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

    // --- Informations cachées : Bob ne voit ni les pioches ni les réservées
    //     d'Alice ; un spectateur ne voit aucune pioche non plus
    std::string eve = login(client, "eve", "eeeeeeeeeeeeeeeeeeeeeeee");
    json spectator_view = get(client, path, eve);
    assert(spectator_view["your_seat"] == -1);
    std::string hidden = "card 0 0 0 0 0 0 0";
    for (const std::string& text : {get(client, path, bob)["save"].get<std::string>(),
                                    spectator_view["save"].get<std::string>()}) {
        // Entre « deck L » et « visible L », toutes les cartes sont cachées (une pioche
        // peut être vide en fin de partie : on regarde les trois)
        size_t count = 0;
        for (int level = 1; level <= 3; level++) {
            size_t deck = text.find("deck " + std::to_string(level) + " ");
            size_t visible = text.find("visible " + std::to_string(level) + " ");
            assert(deck != std::string::npos && visible != std::string::npos);
            std::string deck_part = text.substr(deck, visible - deck);
            for (size_t at = deck_part.find("card "); at != std::string::npos; at = deck_part.find("card ", at + 1)) {
                assert(deck_part.compare(at, hidden.size(), hidden) == 0);
                count++;
            }
        }
        assert(count > 0);
    }

    // --- Recherche de partie : deux joueurs du même mode, d'Elo proche, partie lancée
    {
        assert(get(client, "/matchmaking", alice)["status"] == "idle");
        json waiting = post(client, "/matchmaking/join", alice, {{"mode", "rapide"}});
        assert(waiting["status"] == "waiting");
        post(client, "/matchmaking/leave", alice, json::object());
        assert(get(client, "/matchmaking", alice)["status"] == "idle");
        post(client, "/matchmaking/join", alice, {{"mode", "rapide"}});
        assert(post(client, "/matchmaking/join", bob, {{"mode", "classique"}})["status"] == "waiting");  // autre mode
        json matched = post(client, "/matchmaking/join", bob, {{"mode", "rapide"}});
        assert(matched["status"] == "matched");
        json found = get(client, "/matchmaking", alice);
        assert(found["status"] == "matched" && found["game_id"] == matched["game_id"]);
        std::string match_path = "/games/" + std::to_string(found["game_id"].get<int>());
        assert(get(client, match_path, alice)["your_seat"] >= 0);
        post(client, match_path + "/forfeit", bob, json::object());  // on la termine
    }

    // --- Saisons : fin de saison par le bot, badges, Elo rapprochés de 1000
    {
        assert(get(client, "/me", alice)["season"] == 1);
        int alice_elo = get(client, "/me", alice)["elo"];
        httplib::Headers season_bot = {{"X-Bot-Key", "cle-de-test-du-bot"}};
        auto ended = client.Post("/bot/season/end", season_bot, "{}", "application/json");
        assert(ended && ended->status == 200);
        json result = json::parse(ended->body);
        assert(result["ended"] == 1 && result["season"] == 2 && !result["top"].empty());
        json me = get(client, "/me", alice);
        assert(me["season"] == 2 && me["elo"] == 1000 + (alice_elo - 1000) / 2);
        assert(!me["badges"].empty() && me["badges"][0]["season"] == 1);
        auto refused_season = client.Post("/bot/season/end", "{}", "application/json");
        assert(refused_season && refused_season->status == 403);
    }

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

    // --- Modération (commandes du bot)
    {
        auto bot_get = [&](const std::string& path_query) {
            auto result = client.Get(path_query, bot);
            assert(result && result->status == 200);
            return json::parse(result->body);
        };
        auto bot_post = [&](const std::string& route, const json& body, int expected = 200) {
            auto result = client.Post(route, bot, body.dump(), "application/json");
            assert(result && result->status == expected);
            return json::parse(result->body, nullptr, false);
        };
        assert(client.Get("/bot/profile?pseudo=alice", bad_bot)->status == 403);
        json profile = bot_get("/bot/profile?pseudo=ALICE");  // sans casse
        assert(profile["found"] == true && profile["pseudo"] == "Alice" && profile["rank"].get<int>() >= 1);
        assert(bot_get("/bot/profile?pseudo=personne")["found"] == false);
        assert(bot_get("/bot/profile?discord_id=123456")["pseudo"] == "Carole");

        // Historique : 4 parties pour Alice (dont celle de la recherche de partie),
        // avec les pseudos des joueurs
        json history = bot_get("/bot/history?pseudo=Alice");
        assert(history["games_list"].size() == 4);
        json last_game = history["games_list"][0];
        assert(last_game["players"].size() == 2);
        int elo_before_cancel = bot_get("/bot/profile?pseudo=Alice")["elo"];
        int change = last_game["elo_change"];
        int last_id = last_game["id"];

        // M1 : une action de modération sans identité du modérateur (actor) est refusée
        assert(bot_post("/bot/cancel-game", {{"game_id", last_id}}, 400)
                   .value("error", std::string())
                   .find("moderator") != std::string::npos);
        // Annuler la dernière partie rend l'Elo ; une seconde fois : refusé
        bot_post("/bot/cancel-game", {{"game_id", last_id}, {"actor", "777"}});
        assert(bot_get("/bot/profile?pseudo=Alice")["elo"] == elo_before_cancel - change);
        assert(bot_get("/bot/history?pseudo=Alice")["games_list"][0]["cancelled"] == true);
        bot_post("/bot/cancel-game", {{"game_id", last_id}, {"actor", "777"}}, 400);
        bot_post("/bot/cancel-game", {{"game_id", 999999}, {"actor", "777"}}, 400);

        // Suspension : sessions fermées, reconnexion refusée, absent du classement
        bot_post("/bot/ban", {{"pseudo", "Bébert"}, {"reason", "multi-comptes"}, {"days", 7}, {"actor", "777"}});
        get(client, "/me", bob, 401);
        auto page = client.Get("/auth/dev/start?login=bbbbbbbbbbbbbbbbbbbbbbb2&name=bob");
        assert(page && page->status == 200);
        json refused_login = get(client, "/auth/poll?login=bbbbbbbbbbbbbbbbbbbbbbb2", "");
        assert(refused_login["status"] == "error");
        assert(refused_login["error"].get<std::string>().find("suspended") != std::string::npos);
        for (const json& player : get(client, "/leaderboard", "")["players"]) {
            assert(player["pseudo"] != "Bébert");
        }
        assert(bot_get("/bot/profile?pseudo=Bébert")["banned"] == true);
        bot_post("/bot/ban", {{"pseudo", "Bébert"}, {"days", -1}, {"actor", "777"}}, 400);
        bot_post("/bot/ban", {{"pseudo", "personne"}, {"days", 1}, {"actor", "777"}}, 400);
        bot_post("/bot/unban", {{"pseudo", "Bébert"}, {"actor", "777"}});
        assert(bot_get("/bot/profile?pseudo=Bébert")["banned"] == false);
        bob = login(client, "bob", "bbbbbbbbbbbbbbbbbbbbbbb3");
        assert(get(client, "/me", bob)["pseudo"] == "Bébert");

        // Suppression du compte : le pseudo est libéré, l'historique reste
        bot_post("/bot/delete-account", {{"discord_id", "123456"}, {"actor", "777"}});
        get(client, "/me", carol, 401);
        assert(bot_get("/bot/profile?pseudo=Carole")["found"] == false);
        assert(bot_get("/bot/profile?discord_id=123456")["found"] == false);
    }

    // --- v4 : connexions liées (Discord + Google), administration (bot /dev et panel)
    {
        httplib::Headers bot = {{"X-Bot-Key", "cle-de-test-du-bot"}};
        auto admin = [&](json body, int expected = 200) {
            if (!body.contains("admin_discord_id")) {
                body["admin_discord_id"] = "777";
            }
            auto result = client.Post("/bot/admin", bot, body.dump(), "application/json");
            assert(result);
            if (result->status != expected) {
                std::cerr << "/bot/admin : " << result->status << " " << result->body << "\n";
            }
            assert(result->status == expected);
            return json::parse(result->body, nullptr, false);
        };

        // Lier Google au compte d'Alice : se connecter avec Google ouvre ensuite le même compte
        json me = get(client, "/me", alice);
        std::string link_id = post(client, "/me/link", alice, {{"provider", "google"}})["login"];
        assert(client.Get("/auth/dev/start?login=" + link_id + "&name=g-alice&as=google")->status == 200);
        json linked = get(client, "/auth/poll?login=" + link_id, "");
        assert(linked["status"] == "done" && linked["linked"] == "google" && !linked.contains("token"));
        json providers = get(client, "/me", alice)["providers"];
        assert(std::find(providers.begin(), providers.end(), "google") != providers.end());
        std::string alice_google = login(client, "g-alice", "gggggggggggggggggggggggg", "google");
        assert(get(client, "/me", alice_google)["id"] == me["id"]);
        // Ce compte Google est déjà pris : Bob ne peut pas le lier ; fournisseur inconnu refusé
        std::string taken = post(client, "/me/link", bob, {{"provider", "google"}})["login"];
        client.Get("/auth/dev/start?login=" + taken + "&name=g-alice&as=google");
        json refused = get(client, "/auth/poll?login=" + taken, "");
        assert(refused["status"] == "error" && refused["error"].get<std::string>().find("already used") != std::string::npos);
        post(client, "/me/link", alice, {{"provider", "twitter"}}, 400);

        // Commandes /dev : réservées aux administrateurs
        admin({{"admin_discord_id", "999"}, {"action", "status"}}, 403);
        assert(client.Post("/bot/admin", json({{"admin_discord_id", "777"}}).dump(), "application/json")->status == 403);
        json status = admin({{"action", "status"}});
        assert(status["version"] == SPLENDOR_VERSION && status["accounts"].get<int>() >= 2);
        assert(status["signups_closed"] == false && status["games_paused"] == false);

        // Inscriptions fermées : nouveau compte refusé, ancien compte accepté
        admin({{"action", "signups"}, {"value", "closed"}});
        client.Get("/auth/dev/start?login=nnnnnnnnnnnnnnnnnnnnnnnn&name=nouveau");
        json closed = get(client, "/auth/poll?login=nnnnnnnnnnnnnnnnnnnnnnnn", "");
        assert(closed["status"] == "error" && closed["error"].get<std::string>().find("closed") != std::string::npos);
        alice = login(client, "alice", "aaaaaaaaaaaaaaaaaaaaaaa9");
        admin({{"action", "signups"}, {"value", "open"}});
        admin({{"action", "signups"}, {"value", "maybe"}}, 400);

        // Nouvelles parties suspendues (salons, recherche) ; les parties en cours continuent
        admin({{"action", "games"}, {"value", "paused"}});
        post(client, "/lobbies", alice, {{"mode", "rapide"}, {"seats", 2}}, 503);
        post(client, "/matchmaking/join", alice, {{"mode", "rapide"}}, 503);
        admin({{"action", "games"}, {"value", "on"}});

        // Annonce : dans /config, /me et envoyée aux joueurs connectés
        admin({{"action", "announce"}, {"text", "Maintenance à 20h <b>"}});
        assert(get(client, "/config", "")["announcement"] == "Maintenance à 20h <b>");
        assert(get(client, "/me", alice)["announcement"] == "Maintenance à 20h <b>");
        bool announced = false;
        json notifications = get(client, "/me/notifications?since=0", alice);
        for (const json& notification : notifications["notifications"]) {
            announced = announced || notification["type"] == "announcement";
        }
        assert(announced);

        // Gestion d'un compte
        admin({{"action", "elo"}, {"username", "Alice"}, {"value", "1234"}});
        assert(get(client, "/me", alice)["elo"] == 1234);
        admin({{"action", "elo"}, {"username", "Alice"}, {"value", "abc"}}, 400);
        admin({{"action", "elo"}, {"username", "personne"}, {"value", "1000"}}, 400);
        admin({{"action", "badge"}, {"username", "Alice"}, {"season", "3"}, {"rank", "2"}});
        auto has_badge = [&]() {
            json badges = get(client, "/me", alice)["badges"];
            return std::find(badges.begin(), badges.end(), json({{"season", 3}, {"rank", 2}})) != badges.end();
        };
        assert(has_badge());
        admin({{"action", "badge"}, {"username", "Alice"}, {"season", "3"}, {"rank", "11"}}, 400);
        admin({{"action", "unbadge"}, {"username", "Alice"}, {"season", "3"}});
        assert(!has_badge());
        admin({{"action", "rename"}, {"username", "Alice"}, {"value", "Alicia"}});
        assert(get(client, "/me", alice)["pseudo"] == "Alicia");
        admin({{"action", "rename"}, {"username", "Alicia"}, {"value", "Bébert"}}, 400);  // déjà pris
        admin({{"action", "logout"}, {"username", "Alicia"}});
        get(client, "/me", alice, 401);
        get(client, "/me", alice_google, 401);
        alice = login(client, "alice", "aaaaaaaaaaaaaaaaaaaaaa10");
        admin({{"action", "delete"}, {"username", "Alicia"}}, 400);   // confirmation manquante

        // Partie bloquée terminée par un administrateur : plus de partie, pas d'Elo
        std::string code = post(client, "/lobbies", alice, {{"mode", "rapide"}, {"seats", 2}})["code"];
        post(client, "/lobbies/" + code + "/join", bob, json::object());
        int stuck = post(client, "/lobbies/" + code + "/start", alice, json::object())["game_id"];
        int elo_before = get(client, "/me", alice)["elo"];
        admin({{"action", "end_game"}, {"game", std::to_string(stuck)}});
        get(client, "/games/" + std::to_string(stuck), alice, 404);
        assert(get(client, "/me", alice)["elo"] == elo_before);
        admin({{"action", "end_game"}, {"game", std::to_string(stuck)}}, 400);

        // Version minimale changée sans redémarrer
        admin({{"action", "min_version"}, {"value", "99.0.0"}});
        assert(client.Get("/config")->status == 426);
        admin({{"action", "min_version"}, {"value", "4.x"}}, 400);
        admin({{"action", "min_version"}, {"value", ""}});
        assert(client.Get("/config")->status == 200);

        // Signalement venu du bot, visible et traité dans le panel
        assert(client.Post("/bot/report", bot, json({{"reporter", "Témoin (42)"}, {"pseudo", "Bébert"},
                                                     {"reason", "Triche <i>évidente</i>"}}).dump(),
                           "application/json")->status == 200);
        assert(client.Post("/bot/report", json({{"pseudo", "x"}, {"reason", "y"}}).dump(), "application/json")->status == 403);

        // Panel web : connexion de test (SPLENDOR_DEV_LOGIN), cookie, jeton anti-CSRF
        httplib::Client web("127.0.0.1", PORT);
        auto entry = web.Get("/panel-admin");
        assert(entry && entry->status == 200 && entry->body.find("Admin log") == std::string::npos);
        assert(entry->get_header_value("X-Robots-Tag") == "noindex, nofollow");
        assert(web.Get("/panel-admin/dev-login?id=999")->status == 403);
        auto signed_in = web.Get("/panel-admin/dev-login?id=777");
        assert(signed_in && signed_in->status == 303);
        std::string cookie = signed_in->get_header_value("Set-Cookie");
        assert(cookie.find("HttpOnly") != std::string::npos && cookie.find("SameSite=Strict") != std::string::npos);
        cookie = cookie.substr(0, cookie.find(';'));
        httplib::Headers with_cookie = {{"Cookie", cookie}};
        auto dashboard = web.Get("/panel-admin", with_cookie);
        assert(dashboard && dashboard->status == 200 && dashboard->body.find("Admin log") != std::string::npos);
        assert(dashboard->body.find("<b>") == std::string::npos);   // texte échappé
        assert(dashboard->body.find("Maintenance à 20h &lt;b&gt;") != std::string::npos);
        assert(dashboard->body.find("Games per day") != std::string::npos);
        assert(dashboard->body.find("Triche &lt;i&gt;évidente&lt;/i&gt;") != std::string::npos);
        assert(dashboard->body.find("Mark handled") != std::string::npos);
        auto search = web.Get("/panel-admin?q=alic", with_cookie);
        assert(search && search->body.find("Alicia") != std::string::npos);
        std::string csrf_key = "name=\"csrf\" value=\"";
        size_t at = dashboard->body.find(csrf_key) + csrf_key.size();
        std::string csrf = dashboard->body.substr(at, dashboard->body.find('"', at) - at);
        std::string alicia = std::to_string(me["id"].get<int>());
        assert(web.Get("/panel-admin/account/" + alicia, with_cookie)->body.find("Alicia") != std::string::npos);
        httplib::Params action = {{"action", "elo"}, {"account_id", alicia}, {"value", "1500"},
                                  {"back", "/panel-admin/account/" + alicia}};
        assert(web.Post("/panel-admin/action", with_cookie, action)->status == 403);         // sans jeton
        assert(web.Post("/panel-admin/action", action)->status == 403);                      // sans cookie
        action.emplace("csrf", csrf);
        auto applied = web.Post("/panel-admin/action", with_cookie, action);
        assert(applied && applied->status == 303 && applied->get_header_value("Location") == "/panel-admin/account/" + alicia);
        assert(get(client, "/me", alice)["elo"] == 1500);
        assert(web.Get("/panel-admin/account/" + alicia, with_cookie)->body.find("Done: Elo of Alicia") != std::string::npos);
        httplib::Params evil = {{"csrf", csrf}, {"action", "status"}, {"back", "https://evil.example/"}};
        assert(web.Post("/panel-admin/action", with_cookie, evil)->get_header_value("Location") == "/panel-admin");
        httplib::Params handled = {{"csrf", csrf}, {"action", "handle_report"}, {"report", "1"}, {"back", "/panel-admin"}};
        web.Post("/panel-admin/action", with_cookie, handled);
        assert(web.Get("/panel-admin", with_cookie)->body.find("Mark handled") == std::string::npos);
        // Les actions sont dans le journal
        assert(web.Get("/panel-admin", with_cookie)->body.find("Elo of Alicia: 1234 -&gt; 1500") != std::string::npos);
        web.Post("/panel-admin/logout", with_cookie, httplib::Params{{"csrf", csrf}});
        assert(web.Get("/panel-admin", with_cookie)->body.find("Admin log") == std::string::npos);
        admin({{"action", "announce"}, {"text", ""}});
        assert(get(client, "/config", "")["announcement"] == "");
    }

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
