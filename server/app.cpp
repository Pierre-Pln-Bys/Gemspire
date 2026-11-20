#include "app.hpp"

#include <cstdlib>
#include <ctime>
#include <sstream>

#include "../src/move_json.hpp"

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
    config.dev_login = env("SPLENDOR_DEV_LOGIN") == "1";
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
    return store_.account_for_token(header.substr(prefix.size()));
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
    if (error.empty()) {
        int account = store_.find_or_create_account(provider, provider_id);
        token = account == 0 ? "" : store_.create_session(account);
    }
    std::lock_guard<std::mutex> lock(logins_mutex_);
    PendingLogin& pending = logins_[login];
    if (!token.empty()) {
        pending.status = "done";
        pending.token = token;
    } else {
        pending.status = "error";
        pending.error = error.empty() ? "Erreur du serveur." : error;
    }
}

void ServerApp::setup_routes(httplib::Server& server)
{
    server.set_payload_max_length(16 * 1024);

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
        {
            std::lock_guard<std::mutex> lock(logins_mutex_);
            logins_[login].provider = "dev";
            logins_[login].created = std::time(nullptr);
        }
        finish_login(login, "dev", name, "");
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
        std::string code;
        error = rooms_.create_lobby(seat, mode, seats, code);
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

    // --- Classement
    server.Get("/leaderboard", [this](const httplib::Request&, httplib::Response& response) {
        json list = json::array();
        for (const Account& account : store_.leaderboard(50)) {
            list.push_back({{"pseudo", account.pseudo}, {"elo", account.elo}, {"games", account.games},
                            {"wins", account.wins}});
        }
        reply(response, 200, {{"players", list}});
    });
}
