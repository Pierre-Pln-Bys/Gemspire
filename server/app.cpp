#include "app.hpp"

#include <cstdlib>
#include <ctime>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

#include "../src/move_json.hpp"
#include "../src/version.hpp"
#include "favicon.hpp"
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
    config.web_dir = env("SPLENDOR_WEB_DIR", config.web_dir);
    // Adresse de contact affichée sur le site (confidentialité, conditions)
    config.contact_email = env("SPLENDOR_CONTACT_EMAIL");
    if (config.contact_email.empty()) {
        config.contact_email = "contact@epsilon-forge.fr";
    }
    config.discord_invite = env("SPLENDOR_DISCORD_INVITE");
    std::istringstream admins(env("SPLENDOR_ADMIN_DISCORD_IDS"));
    std::string admin;
    while (std::getline(admins, admin, ',')) {
        if (!admin.empty() && admin.find_first_not_of("0123456789") == std::string::npos) {
            config.admin_discord_ids.push_back(admin);
        }
    }
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
    const char* prefixes[] = {"/me", "/lobbies", "/games/", "/friends", "/replays/", "/matchmaking", "/auth/poll", "/auth/logout", "/config"};
    for (const char* prefix : prefixes) {
        if (path.compare(0, std::string(prefix).size(), prefix) == 0) {
            return true;
        }
    }
    return false;
}

// Pages du site (pas l'API du jeu ni du bot)
static bool is_site_page(const std::string& path)
{
    const char* pages[] = {"/", "/classement", "/terms", "/conditions", "/privacy", "/confidentialite", "/play"};
    for (const char* page : pages) {
        if (path == page) {
            return true;
        }
    }
    return path.compare(0, 6, "/play/") == 0;
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
// Badges de saison : [{"season": 1, "rank": 2}, ...]
static json badges_json(Store& store, int id)
{
    json list = json::array();
    for (const SeasonBadge& badge : store.badges(id)) {
        list.push_back({{"season", badge.season}, {"rank", badge.rank}});
    }
    return list;
}

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
            {"achievements", store.achievements(id).size()},
            {"badges", badges_json(store, id)},
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

// Petite page de fin de connexion, dans la langue du navigateur
static void reply_page(const httplib::Request& request, httplib::Response& response, const std::string& title,
                       const std::string& text)
{
    Lang lang = page_lang(request.get_param_value("lang"), request.get_header_value("Accept-Language"),
                          Lang::English);
    response.set_content("<!doctype html><html lang=\"" + lang_code(lang) + "\"><head><meta charset=\"utf-8\"><title>Gemspire</title>"
                         "<style>body{font-family:sans-serif;background:#0b2d1e;color:#eee;text-align:center;"
                         "padding-top:15vh}h1{color:#ffd140}</style></head><body><h1>"
                         + tr(lang, title) + "</h1><p>" + html_escape(tr(lang, text)) + "</p></body></html>",
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
// `activity` : code donné par l'Activité Discord (SDK intégré), échangé sans redirect_uri
static std::string oauth_identity(const ServerConfig& config, const std::string& provider, const std::string& code,
                                  std::string& user_id, bool activity = false)
{
    bool discord = provider == "discord";
    httplib::Params params = {
        {"grant_type", "authorization_code"},
        {"code", code},
        {"client_id", discord ? config.discord_client_id : config.google_client_id},
        {"client_secret", discord ? config.discord_client_secret : config.google_client_secret},
    };
    if (!activity) {
        params.emplace("redirect_uri", redirect_uri(config, provider));
    }
    httplib::SSLClient token_client(discord ? "discord.com" : "oauth2.googleapis.com");
    token_client.set_connection_timeout(10);
    token_client.set_read_timeout(10);
    auto token_response = token_client.Post(discord ? "/api/oauth2/token" : "/token", params);
    if (!token_response || token_response->status != 200) {
        return "The provider refused the sign-in.";
    }
    json token = json::parse(token_response->body, nullptr, false);
    if (token.is_discarded() || !token.contains("access_token") || !token["access_token"].is_string()) {
        return "Unexpected reply from the provider.";
    }
    httplib::Headers headers = {{"Authorization", "Bearer " + token["access_token"].get<std::string>()}};
    httplib::SSLClient user_client(discord ? "discord.com" : "openidconnect.googleapis.com");
    user_client.set_connection_timeout(10);
    user_client.set_read_timeout(10);
    auto user_response = user_client.Get(discord ? "/api/users/@me" : "/v1/userinfo", headers);
    if (!user_response || user_response->status != 200) {
        return "Couldn't read the account from the provider.";
    }
    json user = json::parse(user_response->body, nullptr, false);
    const char* key = discord ? "id" : "sub";
    if (user.is_discarded() || !user.contains(key) || !user[key].is_string()) {
        return "Unexpected reply from the provider.";
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
    std::string text = "**" + lobby.members[0].pseudo + "** opens a "
                       + (lobby.mode == Mode::Classic ? "classic" : "quick") + " game (1/" + std::to_string(lobby.seats)
                       + ") — code **" + lobby.code + "**, to type in Gemspire (Play online > Join).";
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
        error = "Account not found.";
        return false;
    }
    if (data.pseudo.empty()) {
        error = "Choose a username first.";
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
    int link_account = 0;
    {
        std::lock_guard<std::mutex> lock(logins_mutex_);
        link_account = logins_[login].link_account;
    }
    if (refused.empty() && link_account != 0) {
        // Ajout d'une connexion à un compte existant : pas de nouvelle session
        refused = store_.link_identity(link_account, provider, provider_id);
        token = refused.empty() ? "linked" : "";
    } else if (refused.empty() && signups_closed() && store_.find_account(provider, provider_id) == 0) {
        refused = "New accounts are closed for now. Try again later.";
    } else if (refused.empty()) {
        int account = store_.find_or_create_account(provider, provider_id);
        Account data;
        if (account != 0 && store_.get_account(account, data) && data.banned()) {
            refused = "Account suspended" + (data.ban_reason.empty() ? std::string() : " (" + data.ban_reason + ")")
                      + (data.banned_until == BAN_FOREVER ? " permanently." : " until " + date_text(data.banned_until) + ".");
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
        pending.error = refused.empty() ? "Server error." : refused;
    }
}

void ServerApp::setup_routes(httplib::Server& server)
{
    server.set_payload_max_length(16 * 1024);

    // Jeu trop ancien (ou sans version) : refusé avec 426, le jeu affiche
    // alors l'écran « Mise à jour nécessaire »
    server.set_pre_routing_handler([this](const httplib::Request& request, httplib::Response& response) {
        // Ancienne adresse du site (ou toute autre que l'adresse publique) : les
        // pages du site redirigent vers l'adresse publique (le jeu n'est pas concerné)
        std::string host = request.get_header_value("Host");
        std::string public_host = config_.public_url.substr(config_.public_url.find("://") + 3);
        bool local = host.compare(0, 9, "127.0.0.1") == 0 || host.compare(0, 9, "localhost") == 0;
        bool page = is_site_page(request.path)
                    || (request.path == "/leaderboard" && request.get_header_value("Accept").find("text/html") != std::string::npos);
        if (request.method == "GET" && !host.empty() && !local && host != public_host && page) {
            response.set_redirect(config_.public_url + request.target, 301);
            return httplib::Server::HandlerResponse::Handled;
        }
        if (!route_needs_version(request.path)) {
            return httplib::Server::HandlerResponse::Unhandled;
        }
        std::string version = request.get_header_value("X-Splendor-Version");
        std::string minimum = min_version();
        if (version_allowed(version, minimum)) {
            return httplib::Server::HandlerResponse::Unhandled;
        }
        reply(response, 426, {{"error", "This version of Gemspire is too old: download version "
                                        + minimum + " or newer."},
                              {"min_version", minimum},
                              {"download", "https://github.com/Pierre-Pln-Bys/Gemspire/releases/latest"}});
        return httplib::Server::HandlerResponse::Handled;
    });

    setup_admin_routes(server);

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
            reply_page(request, response, "Sign-in failed", "This sign-in method isn't available.");
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
            pending.back_to_play = request.get_param_value("from") == "play";
        }
        response.set_redirect(authorize_url(config_, provider, login));
    });

    // Panel d'administration : connexion par Discord (seuls les administrateurs passent)
    server.Get("/panel-admin/login", [this](const httplib::Request& request, httplib::Response& response) {
        if (config_.discord_client_id.empty() || config_.admin_discord_ids.empty()) {
            reply_page(request, response, "Sign-in failed", "This sign-in method isn't available.");
            return;
        }
        std::string login = random_hex(16);
        {
            std::lock_guard<std::mutex> lock(logins_mutex_);
            PendingLogin& pending = logins_[login];
            pending.provider = "discord";
            pending.created = std::time(nullptr);
            pending.admin = true;
        }
        response.set_redirect(authorize_url(config_, "discord", login));
    });

    server.Get(R"(/auth/(discord|google)/callback)", [this](const httplib::Request& request, httplib::Response& response) {
        std::string provider = request.matches[1];
        std::string login = request.get_param_value("state");
        {
            std::lock_guard<std::mutex> lock(logins_mutex_);
            auto found = logins_.find(login);
            if (found == logins_.end() || found->second.provider != provider || found->second.status != "pending") {
                reply_page(request, response, "Sign-in expired", "Start again from the game.");
                return;
            }
        }
        bool back_to_play = false;
        {
            std::lock_guard<std::mutex> lock(logins_mutex_);
            back_to_play = logins_[login].back_to_play;
        }
        bool admin = false;
        {
            std::lock_guard<std::mutex> lock(logins_mutex_);
            admin = logins_[login].admin;
        }
        std::string user_id;
        std::string error = request.has_param("code")
                                ? oauth_identity(config_, provider, request.get_param_value("code"), user_id)
                                : "Sign-in cancelled.";
        if (admin) {
            {
                std::lock_guard<std::mutex> lock(logins_mutex_);
                logins_.erase(login);
            }
            if (!error.empty() || !is_admin(user_id)) {
                std::cout << "Administration : connexion refusée (Discord " << user_id << ")" << std::endl;
                reply_page(request, response, "Access denied", error.empty() ? "This account is not an administrator." : error);
                return;
            }
            response.set_header("Set-Cookie", open_admin_session(user_id));
            response.set_redirect("/panel-admin", 303);
            return;
        }
        finish_login(login, provider, user_id, error);
        // Jeu dans le navigateur : retour au jeu, qui termine la connexion (/auth/poll)
        if (back_to_play) {
            response.set_redirect("/play/?login=" + login);
            return;
        }
        if (error.empty()) {
            reply_page(request, response, "Signed in", "You can close this page and go back to Gemspire.");
        } else {
            reply_page(request, response, "Sign-in failed", error);
        }
    });

    // Connexion de test, sans OAuth (seulement avec SPLENDOR_DEV_LOGIN=1)
    server.Get("/auth/dev/start", [this](const httplib::Request& request, httplib::Response& response) {
        std::string login = request.get_param_value("login");
        std::string name = request.get_param_value("name");
        if (!config_.dev_login || !valid_login_id(login) || name.empty() || name.size() > 32) {
            reply_page(request, response, "Sign-in failed", "Test sign-in is disabled.");
            return;
        }
        // as=discord / as=google : simule ce fournisseur (tests), toujours en mode test
        std::string as = request.get_param_value("as");
        std::string provider = as == "discord" || as == "google" ? as : "dev";
        {
            std::lock_guard<std::mutex> lock(logins_mutex_);
            logins_[login].provider = provider;
            logins_[login].created = std::time(nullptr);
        }
        finish_login(login, provider, name, "");
        reply_page(request, response, "Test sign-in successful", "You can go back to Gemspire.");
    });

    // Activité Discord : le jeu (dans un salon vocal) reçoit un code du SDK de
    // Discord et l'échange ici contre une session, sans passer par le navigateur
    server.Post("/auth/discord/activity", [this](const httplib::Request& request, httplib::Response& response) {
        json body = json::parse(request.body, nullptr, false);
        if (config_.discord_client_id.empty() || body.is_discarded() || !body.is_object() || !body.contains("code")
            || !body["code"].is_string() || body["code"].get<std::string>().empty()
            || body["code"].get<std::string>().size() > 200) {
            reply_error(response, 400, "Malformed request.");
            return;
        }
        std::string code = body["code"].get<std::string>();
        std::string user_id;
        std::string error = oauth_identity(config_, "discord", code, user_id, true);
        std::string login = "activity:" + code;  // clé interne, oubliée juste après
        finish_login(login, "discord", user_id, error);
        PendingLogin result;
        {
            std::lock_guard<std::mutex> lock(logins_mutex_);
            result = logins_[login];
            logins_.erase(login);
        }
        if (result.status != "done") {
            reply_error(response, 401, result.error.empty() ? "Sign-in failed." : result.error);
            return;
        }
        reply(response, 200, {{"token", result.token}});
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
        if (found->second.status == "done" && found->second.link_account != 0) {
            body["linked"] = found->second.provider;  // connexion ajoutée au compte, pas de nouveau jeton
        } else if (found->second.status == "done") {
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
            reply_error(response, 401, "Not signed in.");
            return;
        }
        reply(response, 200, {{"id", account.id}, {"pseudo", account.pseudo}, {"provider", account.provider},
                              {"elo", account.elo}, {"games", account.games}, {"wins", account.wins},
                              {"achievements", store_.achievements(id)}, {"badges", badges_json(store_, id)},
                              {"season", store_.current_season()}, {"providers", store_.providers(id)},
                              {"announcement", store_.setting("announcement")}});
    });

    // v4 : ajouter une connexion (Google ou Discord) au compte. Le jeu ouvre
    // ensuite /auth/<fournisseur>/start?login=<login> dans le navigateur et
    // attend /auth/poll comme pour une connexion (réponse « linked »).
    server.Post("/me/link", [this](const httplib::Request& request, httplib::Response& response) {
        json body;
        int id = authenticate(request);
        if (id == 0) {
            reply_error(response, 401, "Not signed in.");
            return;
        }
        std::string provider = parse_body(request, body) ? body.value("provider", std::string()) : "";
        bool configured = provider == "discord" ? !config_.discord_client_id.empty()
                          : provider == "google" ? !config_.google_client_id.empty() || config_.dev_login
                                                 : false;
        if (!configured) {
            reply_error(response, 400, "This sign-in method isn't available.");
            return;
        }
        std::string login = random_hex(16);
        {
            std::lock_guard<std::mutex> lock(logins_mutex_);
            PendingLogin& pending = logins_[login];
            pending.provider = provider;
            pending.created = std::time(nullptr);
            pending.link_account = id;
        }
        reply(response, 200, {{"login", login}});
    });

    // Notifications du jeu (attente longue) ; elles servent aussi de présence
    server.Get("/me/notifications", [this](const httplib::Request& request, httplib::Response& response) {
        int id = authenticate(request);
        if (id == 0) {
            reply_error(response, 401, "Not signed in.");
            return;
        }
        json body;
        long since = request.has_param("since") ? std::atol(request.get_param_value("since").c_str()) : -1;
        rooms_.notifications_json(id, since, body);
        reply(response, 200, body);
    });

    // Réglages publics utiles au jeu (identifiant public de l'application Discord)
    server.Get("/config", [this](const httplib::Request&, httplib::Response& response) {
        reply(response, 200, {{"discord_app_id", config_.discord_client_id},
                              {"min_version", min_version()},
                              {"announcement", store_.setting("announcement")}});
    });

    server.Post("/me/pseudo", [this](const httplib::Request& request, httplib::Response& response) {
        int id = authenticate(request);
        json body;
        if (id == 0) {
            reply_error(response, 401, "Not signed in.");
            return;
        }
        if (!parse_body(request, body) || !body.contains("pseudo") || !body["pseudo"].is_string()) {
            reply_error(response, 400, "Username missing.");
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
    // --- Amis : liste (avec statut des amis acceptés), demande, retrait
    server.Get("/friends", [this](const httplib::Request& request, httplib::Response& response) {
        int id = authenticate(request);
        if (id == 0) {
            reply_error(response, 401, "Not signed in.");
            return;
        }
        json list = json::array();
        for (const Friend& entry : store_.friends(id)) {
            list.push_back({{"pseudo", entry.pseudo}, {"elo", entry.elo}, {"relation", entry.relation},
                            {"status", entry.relation == "friend" ? rooms_.status_of(entry.account) : "unknown"},
                            {"game_id", entry.relation == "friend" ? rooms_.playing_game(entry.account) : 0}});
        }
        reply(response, 200, {{"friends", list}});
    });

    // --- Recherche de partie (adversaire d'Elo proche)
    server.Post(R"(/matchmaking/(join|leave))", [this](const httplib::Request& request, httplib::Response& response) {
        Seat seat;
        std::string error;
        json body;
        int id = authenticate(request);
        if (id == 0) {
            reply_error(response, 401, "Not signed in.");
            return;
        }
        if (!seat_for(id, seat, error)) {
            reply_error(response, 403, error);
            return;
        }
        if (request.matches[1] == "leave") {
            rooms_.leave_queue(id);
        } else if (games_paused()) {
            reply_error(response, 503, "New games are paused for maintenance. Try again later.");
            return;
        } else {
            Mode mode = parse_body(request, body) && body.is_object()
                                && body.value("mode", std::string("classique")) == "rapide"
                            ? Mode::Quick
                            : Mode::Classic;
            rooms_.join_queue(seat, mode);
        }
        json state;
        rooms_.queue_json(id, state);
        reply(response, 200, state);
    });

    server.Get("/matchmaking", [this](const httplib::Request& request, httplib::Response& response) {
        int id = authenticate(request);
        if (id == 0) {
            reply_error(response, 401, "Not signed in.");
            return;
        }
        json state;
        rooms_.queue_json(id, state);
        reply(response, 200, state);
    });

    // Mes dernières parties (avec rediffusion) et rediffusion d'une partie
    server.Get("/me/games", [this](const httplib::Request& request, httplib::Response& response) {
        int id = authenticate(request);
        if (id == 0) {
            reply_error(response, 401, "Not signed in.");
            return;
        }
        json games = json::array();
        for (const HistoryEntry& entry : store_.history(id, 15)) {
            games.push_back({{"id", entry.game_id}, {"mode", entry.mode}, {"finished_at", entry.finished_at},
                             {"place", entry.place}, {"score", entry.score}, {"elo_change", entry.elo_change},
                             {"cancelled", entry.cancelled}, {"replay", entry.has_replay && !entry.cancelled},
                             {"players", entry.players}});
        }
        reply(response, 200, {{"games", games}});
    });

    server.Get(R"(/replays/(\d{1,9}))", [this](const httplib::Request& request, httplib::Response& response) {
        if (authenticate(request) == 0) {
            reply_error(response, 401, "Not signed in.");
            return;
        }
        std::string replay = store_.replay(std::stoi(request.matches[1]));
        json body = json::parse(replay, nullptr, false);
        if (replay.empty() || body.is_discarded()) {
            reply_error(response, 404, "No replay for this game.");
            return;
        }
        reply(response, 200, body);
    });

    // Parties en cours à regarder (mode spectateur : GET /games/ID sans être assis)
    server.Get("/games/live", [this](const httplib::Request& request, httplib::Response& response) {
        if (authenticate(request) == 0) {
            reply_error(response, 401, "Not signed in.");
            return;
        }
        json games;
        rooms_.live_games_json(games);
        reply(response, 200, {{"games", games}});
    });

    server.Post(R"(/friends/(add|remove))", [this](const httplib::Request& request, httplib::Response& response) {
        Seat seat;
        std::string error;
        json body;
        int id = authenticate(request);
        if (id == 0) {
            reply_error(response, 401, "Not signed in.");
            return;
        }
        if (!seat_for(id, seat, error)) {
            reply_error(response, 403, error);
            return;
        }
        std::string pseudo = parse_body(request, body) && body.contains("pseudo") && body["pseudo"].is_string()
                                 ? body["pseudo"].get<std::string>()
                                 : "";
        int other = pseudo.size() > 64 ? 0 : store_.find_by_pseudo(pseudo);
        if (other == 0) {
            reply_error(response, 404, "Player not found.");
            return;
        }
        if (request.matches[1] == "remove") {
            store_.remove_friend(id, other);
        } else {
            error = store_.add_friend(id, other);
            if (!error.empty()) {
                reply_error(response, 400, error);
                return;
            }
            rooms_.notify(other, {{"type", "friend_request"}, {"pseudo", seat.pseudo}});
        }
        reply(response, 200, {{"ok", true}});
    });

    server.Post("/lobbies", [this](const httplib::Request& request, httplib::Response& response) {
        Seat seat;
        std::string error;
        json body;
        int id = authenticate(request);
        if (id == 0) {
            reply_error(response, 401, "Not signed in.");
            return;
        }
        if (!seat_for(id, seat, error)) {
            reply_error(response, 403, error);
            return;
        }
        if (games_paused()) {
            reply_error(response, 503, "New games are paused for maintenance. Try again later.");
            return;
        }
        if (!parse_body(request, body)) {
            reply_error(response, 400, "Malformed request.");
            return;
        }
        Mode mode = body.value("mode", std::string("classique")) == "rapide" ? Mode::Quick : Mode::Classic;
        int seats = body.contains("seats") && body["seats"].is_number_integer() ? body["seats"].get<int>() : 2;
        // Salon vocal Discord lié (proposition du bot) : seulement des chiffres
        std::string voice = body.contains("voice_channel") && body["voice_channel"].is_string()
                                ? body["voice_channel"].get<std::string>()
                                : "";
        if (voice.size() > 25 || voice.find_first_not_of("0123456789") != std::string::npos) {
            reply_error(response, 400, "Invalid voice channel.");
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

    // Activité Discord : salon partagé par tous ceux qui ont ouvert la même instance
    server.Post("/lobbies/activity", [this](const httplib::Request& request, httplib::Response& response) {
        Seat seat;
        std::string error;
        json body;
        int id = authenticate(request);
        if (id == 0) {
            reply_error(response, 401, "Not signed in.");
            return;
        }
        if (!seat_for(id, seat, error)) {
            reply_error(response, 403, error);
            return;
        }
        if (games_paused()) {
            reply_error(response, 503, "New games are paused for maintenance. Try again later.");
            return;
        }
        std::string instance = parse_body(request, body) && body.contains("instance") && body["instance"].is_string()
                                   ? body["instance"].get<std::string>()
                                   : "";
        if (instance.empty() || instance.size() > 100
            || instance.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_:.")
                   != std::string::npos) {
            reply_error(response, 400, "Malformed request.");
            return;
        }
        std::string code;
        error = rooms_.activity_lobby(seat, instance, code);
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
            reply_error(response, 401, "Not signed in.");
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
        } else if (games_paused()) {
            error = "New games are paused for maintenance. Try again later.";
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
            reply_error(response, 401, "Not signed in.");
            return;
        }
        json body;
        if (!rooms_.lobby_json(request.matches[1], since_parameter(request), body)) {
            reply_error(response, 404, "No lobby with this code.");
            return;
        }
        reply(response, 200, body);
    });

    // --- Parties
    server.Get(R"(/games/(\d{1,9}))", [this](const httplib::Request& request, httplib::Response& response) {
        int id = authenticate(request);
        if (id == 0) {
            reply_error(response, 401, "Not signed in.");
            return;
        }
        json body;
        if (!rooms_.game_json(std::atoi(request.matches[1].str().c_str()), id, since_parameter(request), body)) {
            reply_error(response, 404, "Game not found.");
            return;
        }
        reply(response, 200, body);
    });

    server.Post(R"(/games/(\d{1,9})/move)", [this](const httplib::Request& request, httplib::Response& response) {
        int id = authenticate(request);
        json body;
        Move move;
        if (id == 0) {
            reply_error(response, 401, "Not signed in.");
            return;
        }
        if (!parse_body(request, body)) {
            reply_error(response, 400, "Malformed request.");
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

    server.Post(R"(/games/(\d{1,9})/chat)", [this](const httplib::Request& request, httplib::Response& response) {
        int id = authenticate(request);
        if (id == 0) {
            reply_error(response, 401, "Not signed in.");
            return;
        }
        json body;
        if (!parse_body(request, body) || !body.is_object()) {
            reply_error(response, 400, "Malformed request.");
            return;
        }
        std::string text = body.contains("text") && body["text"].is_string() ? body["text"].get<std::string>() : "";
        int reaction = body.contains("reaction") && body["reaction"].is_number_integer() ? body["reaction"].get<int>() : -1;
        std::string error = text.size() > 2000 ? "Invalid message (1 to 120 characters)."
                                                : rooms_.chat(std::stoi(request.matches[1]), id, text, reaction);
        if (!error.empty()) {
            reply_error(response, 400, error);
            return;
        }
        reply(response, 200, {{"ok", true}});
    });

    server.Post(R"(/games/(\d{1,9})/forfeit)", [this](const httplib::Request& request, httplib::Response& response) {
        int id = authenticate(request);
        if (id == 0) {
            reply_error(response, 401, "Not signed in.");
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
            reply_error(response, 403, "Invalid bot key.");
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
            reply_error(response, 403, "Invalid bot key.");
            return;
        }
        int id = target(request.get_param_value("pseudo"), request.get_param_value("discord_id"));
        reply(response, 200, account_json(store_, id));
    });

    server.Get("/bot/history", [this, target](const httplib::Request& request, httplib::Response& response) {
        if (!bot_allowed(request)) {
            reply_error(response, 403, "Invalid bot key.");
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
            reply_error(response, 403, "Invalid bot key.");
            return;
        }
        if (!parse_body(request, body) || !body.contains("days") || !body["days"].is_number_integer()) {
            reply_error(response, 400, "Malformed request.");
            return;
        }
        int id = target(json_text(body, "pseudo"), json_text(body, "discord_id"));
        int days = body["days"].get<int>();
        std::string reason = json_text(body, "reason").substr(0, 200);
        if (id == 0 || days < 0 || days > 3650) {
            reply_error(response, 400, id == 0 ? "Player not found." : "Invalid duration (0 = permanent).");
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
            reply_error(response, 403, "Invalid bot key.");
            return;
        }
        if (!parse_body(request, body)) {
            reply_error(response, 400, "Malformed request.");
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
            reply_error(response, 403, "Invalid bot key.");
            return;
        }
        if (!parse_body(request, body) || !body.contains("game_id") || !body["game_id"].is_number_integer()) {
            reply_error(response, 400, "Malformed request.");
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
            reply_error(response, 403, "Invalid bot key.");
            return;
        }
        if (!parse_body(request, body)) {
            reply_error(response, 400, "Malformed request.");
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

    // /report du bot : le signalement est gardé pour le panel d'administration
    server.Post("/bot/report", [this, json_text](const httplib::Request& request, httplib::Response& response) {
        json body;
        if (!bot_allowed(request)) {
            reply_error(response, 403, "Invalid bot key.");
            return;
        }
        if (!parse_body(request, body) || json_text(body, "pseudo").empty() || json_text(body, "reason").empty()) {
            reply_error(response, 400, "Malformed request.");
            return;
        }
        store_.add_report(json_text(body, "reporter").substr(0, 100), json_text(body, "pseudo").substr(0, 64),
                          json_text(body, "reason").substr(0, 500));
        reply(response, 200, {{"status", "ok"}});
    });

    server.Post("/bot/offer", [this](const httplib::Request& request, httplib::Response& response) {
        json body;
        if (!bot_allowed(request)) {
            reply_error(response, 403, "Invalid bot key.");
            return;
        }
        if (!parse_body(request, body) || !body.contains("discord_id") || !body["discord_id"].is_string()
            || !body.contains("voice_channel") || !body["voice_channel"].is_string()) {
            reply_error(response, 400, "Malformed request.");
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

    // Fin de saison (/mod new-season) : classement archivé, Elo rapprochés de 1000
    server.Post("/bot/season/end", [this](const httplib::Request& request, httplib::Response& response) {
        if (!bot_allowed(request)) {
            reply_error(response, 403, "Invalid bot key.");
            return;
        }
        int ended = store_.current_season();
        json top = json::array();
        for (const Account& account : store_.end_season()) {
            if (top.size() < 10) {
                top.push_back({{"pseudo", account.pseudo}, {"elo", account.elo}});
            }
        }
        std::cout << "Saison " << ended << " terminée" << std::endl;
        reply(response, 200, {{"ended", ended}, {"season", store_.current_season()}, {"top", top}});
    });

    // Rôles de rang : Elo des joueurs liés à Discord
    server.Get("/bot/ranks", [this](const httplib::Request& request, httplib::Response& response) {
        if (!bot_allowed(request)) {
            reply_error(response, 403, "Invalid bot key.");
            return;
        }
        json players = json::array();
        for (const auto& entry : store_.discord_ranks()) {
            players.push_back({{"discord_id", entry.first}, {"elo", entry.second}});
        }
        reply(response, 200, {{"players", players}});
    });

    // /challenge du bot : salon à 2 places créé pour le joueur qui défie ;
    // les deux jeux sont prévenus (s'ils sont ouverts)
    server.Post("/bot/challenge", [this](const httplib::Request& request, httplib::Response& response) {
        json body;
        if (!bot_allowed(request)) {
            reply_error(response, 403, "Invalid bot key.");
            return;
        }
        if (!parse_body(request, body) || !body.contains("discord_id") || !body["discord_id"].is_string()
            || !body.contains("target_discord_id") || !body["target_discord_id"].is_string()) {
            reply_error(response, 400, "Malformed request.");
            return;
        }
        int challenger = store_.find_account("discord", body["discord_id"].get<std::string>());
        Seat seat;
        std::string error;
        if (challenger == 0 || !seat_for(challenger, seat, error)) {
            reply_error(response, 404, "Account not linked: sign in to Gemspire with Discord first.");
            return;
        }
        if (games_paused()) {
            reply_error(response, 503, "New games are paused for maintenance. Try again later.");
            return;
        }
        Mode mode = body.value("mode", std::string("classique")) == "rapide" ? Mode::Quick : Mode::Classic;
        std::string code;
        error = rooms_.create_lobby(seat, mode, 2, code);
        if (!error.empty()) {
            reply_error(response, 400, error);
            return;
        }
        rooms_.notify(challenger, {{"type", "challenge_lobby"}, {"code", code}});
        int target = store_.find_account("discord", body["target_discord_id"].get<std::string>());
        if (target != 0 && target != challenger) {
            rooms_.notify(target, {{"type", "challenge"}, {"from", seat.pseudo}, {"code", code}});
        }
        reply(response, 200, {{"code", code}, {"pseudo", seat.pseudo}, {"target_linked", target != 0}});
    });

    server.Get("/bot/events", [this](const httplib::Request& request, httplib::Response& response) {
        if (!bot_allowed(request)) {
            reply_error(response, 403, "Invalid bot key.");
            return;
        }
        json body;
        rooms_.bot_events_json(since_parameter(request), body);
        reply(response, 200, body);
    });

    // --- Site web, en anglais et en français (?lang=, sinon langue du navigateur).
    //     Les adresses françaises d'origine restent valables.
    SiteInfo site = {config_.contact_email, config_.discord_invite, false};
    // Jeu dans le navigateur : fichiers de `make web` (index.html, .js, .wasm)
    if (server.set_mount_point("/play/", config_.web_dir)) {
        site.web_game = true;
        server.Get("/play", [](const httplib::Request&, httplib::Response& response) {
            response.set_redirect("/play/");
        });
    }
    auto lang_of = [](const httplib::Request& request, Lang fallback) {
        return page_lang(request.get_param_value("lang"), request.get_header_value("Accept-Language"), fallback);
    };
    server.Get("/", [site, lang_of](const httplib::Request& request, httplib::Response& response) {
        reply_site(response, page_home(site, lang_of(request, Lang::English)));
    });
    auto leaderboard_page = [this, site, lang_of](Lang fallback) {
        return [this, site, lang_of, fallback](const httplib::Request& request, httplib::Response& response) {
            reply_site(response, page_leaderboard(site, lang_of(request, fallback), store_.leaderboard(100),
                                                       store_.current_season()));
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
        reply(response, 200, {{"players", list}, {"season", store_.current_season()}});
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
    // Icônes du site (onglets, favoris, écran d'accueil des téléphones), intégrées au serveur
    server.Get("/favicon.ico", [](const httplib::Request&, httplib::Response& response) {
        response.set_content(reinterpret_cast<const char*>(FAVICON_ICO), sizeof(FAVICON_ICO), "image/x-icon");
        response.set_header("Cache-Control", "public, max-age=604800");
    });
    server.Get("/apple-touch-icon.png", [](const httplib::Request&, httplib::Response& response) {
        response.set_content(reinterpret_cast<const char*>(APPLE_TOUCH_ICON_PNG), sizeof(APPLE_TOUCH_ICON_PNG),
                             "image/png");
        response.set_header("Cache-Control", "public, max-age=604800");
    });
    server.Get("/robots.txt", [](const httplib::Request&, httplib::Response& response) {
        response.set_content("User-agent: *\nDisallow: /auth/\nDisallow: /games/\nDisallow: /bot/\n",
                             "text/plain");
    });
}
