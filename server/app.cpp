#include "app.hpp"

#include <cstdlib>
#include <ctime>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

#include "../src/move_json.hpp"
#include "../src/version.hpp"
#include "pages.hpp"

using nlohmann::json;

// ---------------------------------------------------------------- Réglages

static std::string env(const char* name, const std::string& fallback = "")
{
    const char* value = std::getenv(name);
    return value == nullptr ? fallback : value;
}

ServerConfig config_from_environment()
{
    ServerConfig config;
    config.host = env("SPLENDOR_HOST", config.host);
    config.port = std::atoi(env("SPLENDOR_PORT", "8080").c_str());
    config.database = env("SPLENDOR_DATABASE", config.database);
    config.public_url = env("SPLENDOR_PUBLIC_URL", config.public_url);
    config.discord_client_id = env("DISCORD_CLIENT_ID");
    config.discord_client_secret = env("DISCORD_CLIENT_SECRET");
    config.google_client_id = env("GOOGLE_CLIENT_ID");
    config.google_client_secret = env("GOOGLE_CLIENT_SECRET");
    config.discord_webhook = env("DISCORD_WEBHOOK_URL");
    config.bot_key = env("SPLENDOR_BOT_KEY");
    config.dev_login = env("SPLENDOR_DEV_LOGIN") == "1";
    config.min_version = env("SPLENDOR_MIN_VERSION", SPLENDOR_VERSION);
    config.contact_email = env("SPLENDOR_CONTACT_EMAIL");
    config.discord_invite = env("SPLENDOR_DISCORD_INVITE");
    config.turn_seconds = std::atoi(env("SPLENDOR_TURN_SECONDS", "120").c_str());
    if (config.turn_seconds < 5) {
        config.turn_seconds = 120;
    }
    return config;
}

// ---------------------------------------------------------------- Outils HTTP

static void reply(httplib::Response& response, int status, const json& body)
{
    response.status = status;
    response.set_content(body.dump(), "application/json; charset=utf-8");
}

static void reply_error(httplib::Response& response, int status, const std::string& message)
{
    reply(response, status, {{"error", message}});
}

// « 3.4.1 » -> {3, 4, 1} ; une version mal écrite donne {-1, -1, -1}
static std::vector<int> version_parts(const std::string& text)
{
    std::vector<int> parts;
    std::istringstream in(text);
    std::string piece;
    while (std::getline(in, piece, '.')) {
        if (piece.empty() || piece.size() > 4 || piece.find_first_not_of("0123456789") != std::string::npos) {
            return {-1, -1, -1};
        }
        parts.push_back(std::atoi(piece.c_str()));
    }
    if (parts.size() != 3) {
        return {-1, -1, -1};
    }
    return parts;
}

// Le jeu qui envoie `version` est-il assez récent ?
static bool version_allowed(const std::string& version, const std::string& minimum)
{
    std::vector<int> mine = version_parts(version);
    return mine[0] >= 0 && mine >= version_parts(minimum);
}

// Routes appelées par le jeu : elles exigent une version assez récente
// (en-tête X-Splendor-Version). Le site, le navigateur (OAuth) et le bot
// ne sont pas concernés.
static bool route_needs_version(const std::string& path)
{
    const char* prefixes[] = {"/me", "/lobbies", "/games/", "/auth/poll", "/auth/logout", "/config"};
    for (const char* prefix : prefixes) {
        if (path.compare(0, std::string(prefix).size(), prefix) == 0) {
            return true;
        }
    }
    return false;
}

// Date lisible (jj/mm/aaaa, heure UTC)
static std::string date_text(long long seconds)
{
    std::time_t time = static_cast<std::time_t>(seconds);
    std::tm* parts = std::gmtime(&time);
    char text[32] = "";
    if (parts != nullptr) {
        std::strftime(text, sizeof(text), "%d/%m/%Y %H:%M UTC", parts);
    }
    return text;
}

// Compte vu par le bot (commandes /profil, /historique, modération)
static json account_json(Store& store, int id)
{
    Account account;
    if (id == 0 || !store.get_account(id, account)) {
        return {{"found", false}};
    }
    return {{"found", true},
            {"pseudo", account.pseudo},
            {"provider", account.provider},
            {"elo", account.elo},
            {"games", account.games},
            {"wins", account.wins},
            {"rank", store.rank_of(id)},
            {"banned", account.banned()},
            {"banned_until", account.banned_until},
            {"ban_reason", account.ban_reason}};
}

// Page du site, avec des en-têtes de sécurité (pas de script, pas d'intégration)
static void reply_site(httplib::Response& response, const std::string& html, int status = 200)
{
    response.status = status;
    response.set_header("Content-Security-Policy",
                        "default-src 'none'; style-src 'unsafe-inline'; img-src 'self'; frame-ancestors 'none'");
    response.set_header("X-Content-Type-Options", "nosniff");
    response.set_header("Referrer-Policy", "no-referrer");
    response.set_header("Cache-Control", "public, max-age=60");
    response.set_content(html, "text/html; charset=utf-8");
}

static void reply_page(httplib::Response& response, const std::string& title, const std::string& text)
{
    response.set_content("<!doctype html><html lang=\"fr\"><head><meta charset=\"utf-8\"><title>Splendor</title>"
                         "<style>body{font-family:sans-serif;background:#0b2d1e;color:#eee;text-align:center;"
                         "padding-top:15vh}h1{color:#ffd140}</style></head><body><h1>"
                         + title + "</h1><p>" + text + "</p></body></html>",
                         "text/html; charset=utf-8");
}

// Encode une valeur pour une adresse (RFC 3986)
static std::string url_encode(const std::string& value)
{
    std::ostringstream out;
    const char* digits = "0123456789ABCDEF";
    for (unsigned char c : value) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out << c;
        } else {
            out << '%' << digits[c >> 4] << digits[c & 15];
        }
    }
    return out.str();
}

static bool valid_login_id(const std::string& login)
{
    if (login.size() < 16 || login.size() > 64) {
        return false;
    }
    for (unsigned char c : login) {
        if (!std::isalnum(c)) {
            return false;
        }
    }
    return true;
}

static bool parse_body(const httplib::Request& request, json& body)
{
    body = json::parse(request.body, nullptr, false);
    return !body.is_discarded() && body.is_object();
}

static long since_parameter(const httplib::Request& request)
{
    return request.has_param("since") ? std::atol(request.get_param_value("since").c_str()) : 0;
}

// ---------------------------------------------------------------- OAuth

static std::string redirect_uri(const ServerConfig& config, const std::string& provider)
{
    return config.public_url + "/auth/" + provider + "/callback";
}

static std::string authorize_url(const ServerConfig& config, const std::string& provider, const std::string& state)
{
    std::string redirect = url_encode(redirect_uri(config, provider));
    if (provider == "discord") {
        return "https://discord.com/oauth2/authorize?response_type=code&scope=identify&prompt=none&client_id="
               + url_encode(config.discord_client_id) + "&redirect_uri=" + redirect + "&state=" + state;
    }
    return "https://accounts.google.com/o/oauth2/v2/auth?response_type=code&scope=openid&prompt=select_account"
           "&client_id="
           + url_encode(config.google_client_id) + "&redirect_uri=" + redirect + "&state=" + state;
}

// Échange le code OAuth contre l'identifiant du joueur chez le fournisseur.
// Retourne "" (et remplit user_id) ou l'erreur.
static std::string oauth_identity(const ServerConfig& config, const std::string& provider, const std::string& code,
                                  std::string& user_id)
{
    bool discord = provider == "discord";
    httplib::Params params = {
        {"grant_type", "authorization_code"},
        {"code", code},
        {"redirect_uri", redirect_uri(config, provider)},
        {"client_id", discord ? config.discord_client_id : config.google_client_id},
        {"client_secret", discord ? config.discord_client_secret : config.google_client_secret},
    };
    httplib::SSLClient token_client(discord ? "discord.com" : "oauth2.googleapis.com");
    token_client.set_connection_timeout(10);
    token_client.set_read_timeout(10);
    auto token_response = token_client.Post(discord ? "/api/oauth2/token" : "/token", params);
    if (!token_response || token_response->status != 200) {
        return "Le fournisseur a refusé la connexion.";
    }
    json token = json::parse(token_response->body, nullptr, false);
    if (token.is_discarded() || !token.contains("access_token") || !token["access_token"].is_string()) {
        return "Réponse inattendue du fournisseur.";
    }
    httplib::Headers headers = {{"Authorization", "Bearer " + token["access_token"].get<std::string>()}};
    httplib::SSLClient user_client(discord ? "discord.com" : "openidconnect.googleapis.com");
    user_client.set_connection_timeout(10);
    user_client.set_read_timeout(10);
    auto user_response = user_client.Get(discord ? "/api/users/@me" : "/v1/userinfo", headers);
    if (!user_response || user_response->status != 200) {
        return "Impossible de lire le compte chez le fournisseur.";
    }
    json user = json::parse(user_response->body, nullptr, false);
    const char* key = discord ? "id" : "sub";
    if (user.is_discarded() || !user.contains(key) || !user[key].is_string()) {
        return "Réponse inattendue du fournisseur.";
    }
    user_id = user[key].get<std::string>();
    return "";
}

// ---------------------------------------------------------------- ServerApp

ServerApp::ServerApp(const ServerConfig& config) : config_(config), store_(config.database), rooms_(store_)
{
    rooms_.set_wait_seconds(config.wait_seconds);
    rooms_.set_turn_seconds(config.turn_seconds);
    rooms_.on_lobby_created = [this](const Lobby& lobby) { announce_lobby(lobby); };
}

// Annonce un nouveau salon dans le salon Discord du webhook (dans un fil :
// la requête ne doit pas retarder la réponse au joueur)
void ServerApp::announce_lobby(const Lobby& lobby)
{
    // Un salon créé depuis le bot a déjà son salon vocal : pas d'annonce
    if (config_.discord_webhook.empty() || !lobby.voice_channel.empty()) {
        return;
    }
    const std::string prefix = "https://discord.com/api/webhooks/";
    if (config_.discord_webhook.compare(0, prefix.size(), prefix) != 0) {
        std::cerr << "Webhook Discord ignoré : l'adresse doit commencer par " << prefix << std::endl;
        return;
    }
    std::string path = config_.discord_webhook.substr(std::string("https://discord.com").size());
    std::string text = "**" + lobby.members[0].pseudo + "** ouvre une partie "
                       + (lobby.mode == Mode::Classic ? "classique" : "rapide") + " (1/" + std::to_string(lobby.seats)
                       + ") — code **" + lobby.code + "** à taper dans Splendor (Jouer en ligne > Rejoindre).";
    std::string body = json({{"content", text}, {"allowed_mentions", {{"parse", json::array()}}}}).dump();
    std::thread([path, body]() {
        httplib::SSLClient client("discord.com");
        client.set_connection_timeout(5);
        httplib::Result result = client.Post(path, body, "application/json");
        if (!result || result->status / 100 != 2) {
            std::cerr << "Webhook Discord : échec de l'annonce (HTTP " << (result ? result->status : 0) << ")"
                      << std::endl;
        }
    }).detach();
}

void ServerApp::tick()
{
    rooms_.tick();
}

// Le bot Discord s'identifie avec la clé partagée SPLENDOR_BOT_KEY
bool ServerApp::bot_allowed(const httplib::Request& request) const
{
    std::string given = request.get_header_value("X-Bot-Key");
    const std::string& expected = config_.bot_key;
    if (expected.empty() || given.size() != expected.size()) {
        return false;
    }
    // Comparaison en temps constant
    unsigned char difference = 0;
    for (size_t i = 0; i < expected.size(); i++) {
        difference |= static_cast<unsigned char>(given[i] ^ expected[i]);
    }
    return difference == 0;
}

bool ServerApp::ok() const
{
    return store_.ok();
}

int ServerApp::authenticate(const httplib::Request& request)
{
    std::string header = request.get_header_value("Authorization");
    const std::string prefix = "Bearer ";
    if (header.compare(0, prefix.size(), prefix) != 0) {
        return 0;
    }
    int account = store_.account_for_token(header.substr(prefix.size()));
    if (account != 0) {
        rooms_.touch(account);  // présence : le jeu de ce joueur est ouvert
    }
    return account;
}

// Place d'un joueur connecté ; il doit avoir choisi son pseudo
bool ServerApp::seat_for(int account, Seat& seat, std::string& error)
{
    Account data;
    if (!store_.get_account(account, data)) {
        error = "Compte introuvable.";
        return false;
    }
    if (data.pseudo.empty()) {
        error = "Choisissez d'abord un pseudo.";
        return false;
    }
    seat = {data.id, data.pseudo, data.elo};
    return true;
}

void ServerApp::finish_login(const std::string& login, const std::string& provider, const std::string& provider_id,
                             const std::string& error)
{
    std::string token;
    std::string refused = error;
    if (refused.empty()) {
        int account = store_.find_or_create_account(provider, provider_id);
        Account data;
        if (account != 0 && store_.get_account(account, data) && data.banned()) {
            refused = "Compte suspendu" + (data.ban_reason.empty() ? std::string() : " (" + data.ban_reason + ")")
                      + (data.banned_until == BAN_FOREVER ? " définitivement." : " jusqu'au " + date_text(data.banned_until) + ".");
        } else {
            token = account == 0 ? "" : store_.create_session(account);
        }
    }
    std::lock_guard<std::mutex> lock(logins_mutex_);
    PendingLogin& pending = logins_[login];
    if (!token.empty()) {
        pending.status = "done";
        pending.token = token;
    } else {
        pending.status = "error";
        pending.error = refused.empty() ? "Erreur du serveur." : refused;
    }
}

void ServerApp::setup_routes(httplib::Server& server)
{
    server.set_payload_max_length(16 * 1024);

    // Jeu trop ancien (ou sans version) : refusé avec 426, le jeu affiche
    // alors l'écran « Mise à jour nécessaire »
    server.set_pre_routing_handler([this](const httplib::Request& request, httplib::Response& response) {
        if (!route_needs_version(request.path)) {
            return httplib::Server::HandlerResponse::Unhandled;
        }
        std::string version = request.get_header_value("X-Splendor-Version");
        if (version_allowed(version, config_.min_version)) {
            return httplib::Server::HandlerResponse::Unhandled;
        }
        reply(response, 426, {{"error", "Cette version de Splendor est trop ancienne : téléchargez la version "
                                        + config_.min_version + " ou plus récente."},
                              {"min_version", config_.min_version},
                              {"download", "https://github.com/Pierre-Pln-Bys/ProjectSplendor/releases/latest"}});
        return httplib::Server::HandlerResponse::Handled;
    });

    server.Get("/health", [](const httplib::Request&, httplib::Response& response) {
        reply(response, 200, {{"status", "ok"}});
    });

    // --- Connexion : le jeu ouvre /auth/<fournisseur>/start?login=ID dans le
    //     navigateur, puis interroge /auth/poll?login=ID jusqu'à la fin.
    server.Get(R"(/auth/(discord|google)/start)", [this](const httplib::Request& request, httplib::Response& response) {
        std::string provider = request.matches[1];
        std::string login = request.get_param_value("login");
        bool configured = provider == "discord" ? !config_.discord_client_id.empty() : !config_.google_client_id.empty();
        if (!valid_login_id(login) || !configured) {
            reply_page(response, "Connexion impossible", "Ce mode de connexion n'est pas disponible.");
            return;
        }
        {
            std::lock_guard<std::mutex> lock(logins_mutex_);
            // Les connexions de plus de 10 minutes sont oubliées
            long long now = std::time(nullptr);
            for (auto it = logins_.begin(); it != logins_.end();) {
                it = now - it->second.created > 600 ? logins_.erase(it) : std::next(it);
            }
            PendingLogin& pending = logins_[login];
            pending.provider = provider;
            pending.created = now;
        }
        response.set_redirect(authorize_url(config_, provider, login));
    });

    server.Get(R"(/auth/(discord|google)/callback)", [this](const httplib::Request& request, httplib::Response& response) {
        std::string provider = request.matches[1];
        std::string login = request.get_param_value("state");
        {
            std::lock_guard<std::mutex> lock(logins_mutex_);
            auto found = logins_.find(login);
            if (found == logins_.end() || found->second.provider != provider || found->second.status != "pending") {
                reply_page(response, "Connexion expirée", "Recommencez depuis le jeu.");
                return;
            }
        }
        std::string user_id;
        std::string error = request.has_param("code")
                                ? oauth_identity(config_, provider, request.get_param_value("code"), user_id)
                                : "Connexion annulée.";
        finish_login(login, provider, user_id, error);
        if (error.empty()) {
            reply_page(response, "Connexion réussie", "Vous pouvez fermer cette page et revenir dans Splendor.");
        } else {
            reply_page(response, "Connexion impossible", error);
        }
    });

    // Connexion de test, sans OAuth (seulement avec SPLENDOR_DEV_LOGIN=1)
    server.Get("/auth/dev/start", [this](const httplib::Request& request, httplib::Response& response) {
        std::string login = request.get_param_value("login");
        std::string name = request.get_param_value("name");
        if (!config_.dev_login || !valid_login_id(login) || name.empty() || name.size() > 32) {
            reply_page(response, "Connexion impossible", "La connexion de test est désactivée.");
            return;
        }
        // as=discord : simule un compte Discord (tests du bot), toujours en mode test
        std::string provider = request.get_param_value("as") == "discord" ? "discord" : "dev";
        {
            std::lock_guard<std::mutex> lock(logins_mutex_);
            logins_[login].provider = provider;
            logins_[login].created = std::time(nullptr);
        }
        finish_login(login, provider, name, "");
        reply_page(response, "Connexion de test réussie", "Vous pouvez revenir dans Splendor.");
    });

    server.Get("/auth/poll", [this](const httplib::Request& request, httplib::Response& response) {
        std::string login = request.get_param_value("login");
        std::lock_guard<std::mutex> lock(logins_mutex_);
        auto found = logins_.find(login);
        if (found == logins_.end()) {
            reply(response, 200, {{"status", "pending"}});
            return;
        }
        json body = {{"status", found->second.status}};
        if (found->second.status == "done") {
            body["token"] = found->second.token;
        } else if (found->second.status == "error") {
            body["error"] = found->second.error;
        }
        if (found->second.status != "pending") {
            logins_.erase(found);  // le jeton n'est donné qu'une fois
        }
        reply(response, 200, body);
    });

    server.Post("/auth/logout", [this](const httplib::Request& request, httplib::Response& response) {
        std::string header = request.get_header_value("Authorization");
        if (header.size() > 7) {
            store_.delete_session(header.substr(7));
        }
        reply(response, 200, {{"status", "ok"}});
    });

    // --- Compte
    server.Get("/me", [this](const httplib::Request& request, httplib::Response& response) {
        Account account;
        int id = authenticate(request);
        if (id == 0 || !store_.get_account(id, account)) {
            reply_error(response, 401, "Non connecté.");
            return;
        }
        reply(response, 200, {{"id", account.id}, {"pseudo", account.pseudo}, {"provider", account.provider},
                              {"elo", account.elo}, {"games", account.games}, {"wins", account.wins}});
    });

    // Notifications du jeu (attente longue) ; elles servent aussi de présence
    server.Get("/me/notifications", [this](const httplib::Request& request, httplib::Response& response) {
        int id = authenticate(request);
        if (id == 0) {
            reply_error(response, 401, "Non connecté.");
            return;
        }
        json body;
        long since = request.has_param("since") ? std::atol(request.get_param_value("since").c_str()) : -1;
        rooms_.notifications_json(id, since, body);
        reply(response, 200, body);
    });

    // Réglages publics utiles au jeu (identifiant public de l'application Discord)
    server.Get("/config", [this](const httplib::Request&, httplib::Response& response) {
        reply(response, 200, {{"discord_app_id", config_.discord_client_id}, {"min_version", config_.min_version}});
    });

    server.Post("/me/pseudo", [this](const httplib::Request& request, httplib::Response& response) {
        int id = authenticate(request);
        json body;
        if (id == 0) {
            reply_error(response, 401, "Non connecté.");
            return;
        }
        if (!parse_body(request, body) || !body.contains("pseudo") || !body["pseudo"].is_string()) {
            reply_error(response, 400, "Pseudo manquant.");
            return;
        }
        std::string error = store_.set_pseudo(id, body["pseudo"].get<std::string>());
        if (!error.empty()) {
            reply_error(response, 400, error);
            return;
        }
        reply(response, 200, {{"pseudo", body["pseudo"]}});
    });

    // --- Salons
    server.Post("/lobbies", [this](const httplib::Request& request, httplib::Response& response) {
        Seat seat;
        std::string error;
        json body;
        int id = authenticate(request);
        if (id == 0) {
            reply_error(response, 401, "Non connecté.");
            return;
        }
        if (!seat_for(id, seat, error)) {
            reply_error(response, 403, error);
            return;
        }
        if (!parse_body(request, body)) {
            reply_error(response, 400, "Requête mal formée.");
            return;
        }
        Mode mode = body.value("mode", std::string("classique")) == "rapide" ? Mode::Quick : Mode::Classic;
        int seats = body.contains("seats") && body["seats"].is_number_integer() ? body["seats"].get<int>() : 2;
        // Salon vocal Discord lié (proposition du bot) : seulement des chiffres
        std::string voice = body.contains("voice_channel") && body["voice_channel"].is_string()
                                ? body["voice_channel"].get<std::string>()
                                : "";
        if (voice.size() > 25 || voice.find_first_not_of("0123456789") != std::string::npos) {
            reply_error(response, 400, "Salon vocal invalide.");
            return;
        }
        std::string code;
        error = rooms_.create_lobby(seat, mode, seats, code, voice);
        if (!error.empty()) {
            reply_error(response, 400, error);
            return;
        }
        reply(response, 200, {{"code", code}});
    });

    server.Post(R"(/lobbies/([A-Z0-9]{5})/(join|leave|start))", [this](const httplib::Request& request,
                                                                      httplib::Response& response) {
        Seat seat;
        std::string error;
        int id = authenticate(request);
        if (id == 0) {
            reply_error(response, 401, "Non connecté.");
            return;
        }
        if (!seat_for(id, seat, error)) {
            reply_error(response, 403, error);
            return;
        }
        std::string code = request.matches[1];
        std::string action = request.matches[2];
        int game_id = 0;
        if (action == "join") {
            error = rooms_.join_lobby(code, seat);
        } else if (action == "leave") {
            error = rooms_.leave_lobby(code, id);
        } else {
            error = rooms_.start_lobby(code, id, game_id);
        }
        if (!error.empty()) {
            reply_error(response, 400, error);
            return;
        }
        reply(response, 200, {{"status", "ok"}, {"game_id", game_id}});
    });

    server.Get(R"(/lobbies/([A-Z0-9]{5}))", [this](const httplib::Request& request, httplib::Response& response) {
        if (authenticate(request) == 0) {
            reply_error(response, 401, "Non connecté.");
            return;
        }
        json body;
        if (!rooms_.lobby_json(request.matches[1], since_parameter(request), body)) {
            reply_error(response, 404, "Aucun salon avec ce code.");
            return;
        }
        reply(response, 200, body);
    });

    // --- Parties
    server.Get(R"(/games/(\d{1,9}))", [this](const httplib::Request& request, httplib::Response& response) {
        int id = authenticate(request);
        if (id == 0) {
            reply_error(response, 401, "Non connecté.");
            return;
        }
        json body;
        if (!rooms_.game_json(std::atoi(request.matches[1].str().c_str()), id, since_parameter(request), body)) {
            reply_error(response, 404, "Partie introuvable.");
            return;
        }
        reply(response, 200, body);
    });

    server.Post(R"(/games/(\d{1,9})/move)", [this](const httplib::Request& request, httplib::Response& response) {
        int id = authenticate(request);
        json body;
        Move move;
        if (id == 0) {
            reply_error(response, 401, "Non connecté.");
            return;
        }
        if (!parse_body(request, body)) {
            reply_error(response, 400, "Requête mal formée.");
            return;
        }
        std::string error = move_from_json(body, move);
        if (error.empty()) {
            error = rooms_.play(std::atoi(request.matches[1].str().c_str()), id, move);
        }
        if (!error.empty()) {
            reply_error(response, 400, error);
            return;
        }
        reply(response, 200, {{"status", "ok"}});
    });

    server.Post(R"(/games/(\d{1,9})/forfeit)", [this](const httplib::Request& request, httplib::Response& response) {
        int id = authenticate(request);
        if (id == 0) {
            reply_error(response, 401, "Non connecté.");
            return;
        }
        std::string error = rooms_.forfeit(std::atoi(request.matches[1].str().c_str()), id);
        if (!error.empty()) {
            reply_error(response, 400, error);
            return;
        }
        reply(response, 200, {{"status", "ok"}});
    });

    // --- API du bot Discord (en-tête X-Bot-Key)
    server.Get("/bot/presence", [this](const httplib::Request& request, httplib::Response& response) {
        if (!bot_allowed(request)) {
            reply_error(response, 403, "Clé du bot invalide.");
            return;
        }
        int account = store_.find_account("discord", request.get_param_value("discord_id"));
        Account data;
        bool linked = account != 0 && store_.get_account(account, data);
        reply(response, 200, {{"linked", linked},
                              {"online", linked && rooms_.is_online(account)},
                              {"pseudo", linked ? data.pseudo : ""}});
    });

    // --- Commandes du bot : profil, historique, modération
    // Compte désigné par ?pseudo= ou ?discord_id= (ou les mêmes champs en JSON)
    auto target = [this](const std::string& pseudo, const std::string& discord_id) {
        if (!pseudo.empty()) {
            return store_.find_by_pseudo(pseudo);
        }
        return discord_id.empty() ? 0 : store_.find_account("discord", discord_id);
    };
    auto json_text = [](const json& body, const char* key) {
        return body.contains(key) && body[key].is_string() ? body[key].get<std::string>() : std::string();
    };

    server.Get("/bot/profile", [this, target](const httplib::Request& request, httplib::Response& response) {
        if (!bot_allowed(request)) {
            reply_error(response, 403, "Clé du bot invalide.");
            return;
        }
        int id = target(request.get_param_value("pseudo"), request.get_param_value("discord_id"));
        reply(response, 200, account_json(store_, id));
    });

    server.Get("/bot/history", [this, target](const httplib::Request& request, httplib::Response& response) {
        if (!bot_allowed(request)) {
            reply_error(response, 403, "Clé du bot invalide.");
            return;
        }
        int id = target(request.get_param_value("pseudo"), request.get_param_value("discord_id"));
        json body = account_json(store_, id);
        if (id == 0) {
            reply(response, 200, body);
            return;
        }
        body["games_list"] = json::array();
        for (const HistoryEntry& entry : store_.history(id, 15)) {
            body["games_list"].push_back({{"id", entry.game_id},
                                          {"mode", entry.mode},
                                          {"finished_at", entry.finished_at},
                                          {"place", entry.place},
                                          {"score", entry.score},
                                          {"elo_change", entry.elo_change},
                                          {"cancelled", entry.cancelled},
                                          {"players", entry.players}});
        }
        reply(response, 200, body);
    });

    server.Post("/bot/ban", [this, target, json_text](const httplib::Request& request, httplib::Response& response) {
        json body;
        if (!bot_allowed(request)) {
            reply_error(response, 403, "Clé du bot invalide.");
            return;
        }
        if (!parse_body(request, body) || !body.contains("days") || !body["days"].is_number_integer()) {
            reply_error(response, 400, "Requête mal formée.");
            return;
        }
        int id = target(json_text(body, "pseudo"), json_text(body, "discord_id"));
        int days = body["days"].get<int>();
        std::string reason = json_text(body, "reason").substr(0, 200);
        if (id == 0 || days < 0 || days > 3650) {
            reply_error(response, 400, id == 0 ? "Joueur introuvable." : "Durée invalide (0 = définitif).");
            return;
        }
        long long until = days == 0 ? BAN_FOREVER : static_cast<long long>(std::time(nullptr)) + days * 86400LL;
        store_.ban(id, until, reason);
        rooms_.remove_account(id);
        std::cout << "Modération : compte " << id << " suspendu (" << (days == 0 ? std::string("définitif")
                                                                                 : std::to_string(days) + " j")
                  << ")" << std::endl;
        reply(response, 200, account_json(store_, id));
    });

    server.Post("/bot/unban", [this, target, json_text](const httplib::Request& request, httplib::Response& response) {
        json body;
        if (!bot_allowed(request)) {
            reply_error(response, 403, "Clé du bot invalide.");
            return;
        }
        if (!parse_body(request, body)) {
            reply_error(response, 400, "Requête mal formée.");
            return;
        }
        int id = target(json_text(body, "pseudo"), json_text(body, "discord_id"));
        if (id == 0) {
            reply_error(response, 400, "Joueur introuvable.");
            return;
        }
        store_.unban(id);
        std::cout << "Modération : compte " << id << " réactivé" << std::endl;
        reply(response, 200, account_json(store_, id));
    });

    server.Post("/bot/cancel-game", [this](const httplib::Request& request, httplib::Response& response) {
        json body;
        if (!bot_allowed(request)) {
            reply_error(response, 403, "Clé du bot invalide.");
            return;
        }
        if (!parse_body(request, body) || !body.contains("game_id") || !body["game_id"].is_number_integer()) {
            reply_error(response, 400, "Requête mal formée.");
            return;
        }
        std::string error = store_.cancel_game(body["game_id"].get<int>());
        if (!error.empty()) {
            reply_error(response, 400, error);
            return;
        }
        std::cout << "Modération : partie " << body["game_id"].get<int>() << " annulée" << std::endl;
        reply(response, 200, {{"status", "ok"}});
    });

    server.Post("/bot/delete-account", [this, target, json_text](const httplib::Request& request,
                                                                 httplib::Response& response) {
        json body;
        if (!bot_allowed(request)) {
            reply_error(response, 403, "Clé du bot invalide.");
            return;
        }
        if (!parse_body(request, body)) {
            reply_error(response, 400, "Requête mal formée.");
            return;
        }
        int id = target(json_text(body, "pseudo"), json_text(body, "discord_id"));
        if (id == 0) {
            reply_error(response, 400, "Joueur introuvable.");
            return;
        }
        rooms_.remove_account(id);
        store_.delete_account(id);
        std::cout << "Compte " << id << " supprimé à la demande" << std::endl;
        reply(response, 200, {{"status", "ok"}});
    });

    server.Post("/bot/offer", [this](const httplib::Request& request, httplib::Response& response) {
        json body;
        if (!bot_allowed(request)) {
            reply_error(response, 403, "Clé du bot invalide.");
            return;
        }
        if (!parse_body(request, body) || !body.contains("discord_id") || !body["discord_id"].is_string()
            || !body.contains("voice_channel") || !body["voice_channel"].is_string()) {
            reply_error(response, 400, "Requête mal formée.");
            return;
        }
        int account = store_.find_account("discord", body["discord_id"].get<std::string>());
        if (account == 0 || !rooms_.is_online(account)) {
            reply(response, 200, {{"offered", false}});
            return;
        }
        rooms_.notify(account, {{"type", "voice_offer"},
                                {"voice_channel", body["voice_channel"]},
                                {"channel_name", body.value("channel_name", std::string("Discord"))}});
        reply(response, 200, {{"offered", true}});
    });

    server.Get("/bot/events", [this](const httplib::Request& request, httplib::Response& response) {
        if (!bot_allowed(request)) {
            reply_error(response, 403, "Clé du bot invalide.");
            return;
        }
        json body;
        rooms_.bot_events_json(since_parameter(request), body);
        reply(response, 200, body);
    });

    // --- Site web, en anglais et en français (?lang=, sinon langue du navigateur).
    //     Les adresses françaises d'origine restent valables.
    SiteInfo site = {config_.contact_email, config_.discord_invite, false};
    auto lang_of = [](const httplib::Request& request, Lang fallback) {
        return page_lang(request.get_param_value("lang"), request.get_header_value("Accept-Language"), fallback);
    };
    server.Get("/", [site, lang_of](const httplib::Request& request, httplib::Response& response) {
        reply_site(response, page_home(site, lang_of(request, Lang::English)));
    });
    auto leaderboard_page = [this, site, lang_of](Lang fallback) {
        return [this, site, lang_of, fallback](const httplib::Request& request, httplib::Response& response) {
            reply_site(response, page_leaderboard(site, lang_of(request, fallback), store_.leaderboard(100)));
        };
    };
    // /leaderboard : page pour un navigateur (il demande du HTML), JSON pour le
    // jeu et le bot (50 meilleurs)
    server.Get("/leaderboard", [this, leaderboard_page](const httplib::Request& request, httplib::Response& response) {
        if (request.get_header_value("Accept").find("text/html") != std::string::npos) {
            leaderboard_page(Lang::English)(request, response);
            return;
        }
        json list = json::array();
        for (const Account& account : store_.leaderboard(50)) {
            list.push_back({{"pseudo", account.pseudo}, {"elo", account.elo}, {"games", account.games},
                            {"wins", account.wins}});
        }
        reply(response, 200, {{"players", list}});
    });
    server.Get("/classement", leaderboard_page(Lang::French));
    auto terms_page = [site, lang_of](Lang fallback) {
        return [site, lang_of, fallback](const httplib::Request& request, httplib::Response& response) {
            reply_site(response, page_terms(site, lang_of(request, fallback)));
        };
    };
    server.Get("/terms", terms_page(Lang::English));
    server.Get("/conditions", terms_page(Lang::French));
    auto privacy_page = [site, lang_of](Lang fallback) {
        return [site, lang_of, fallback](const httplib::Request& request, httplib::Response& response) {
            reply_site(response, page_privacy(site, lang_of(request, fallback)));
        };
    };
    server.Get("/privacy", privacy_page(Lang::English));
    server.Get("/confidentialite", privacy_page(Lang::French));
    server.Get("/robots.txt", [](const httplib::Request&, httplib::Response& response) {
        response.set_content("User-agent: *\nDisallow: /auth/\nDisallow: /games/\nDisallow: /bot/\n",
                             "text/plain");
    });
}
