// Administration : panel web /panel-admin et actions communes avec les
// commandes /dev du bot Discord (route /bot/admin).
//
// Sécurité : seuls les comptes Discord listés dans SPLENDOR_ADMIN_DISCORD_IDS
// peuvent se connecter (OAuth Discord). La session est un cookie HttpOnly,
// SameSite=Strict, et chaque formulaire renvoie un jeton anti-CSRF.
#include "app.hpp"

#include <algorithm>
#include <ctime>
#include <iostream>
#include <sstream>

#include "../src/version.hpp"
#include "pages.hpp"

using nlohmann::json;

static const char* const ADMIN_COOKIE = "gemspire_admin";
static const long long ADMIN_SESSION_SECONDS = 8 * 3600;

// ---------------------------------------------------------------- Outils

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

// Entier écrit en chiffres (signe moins permis) entre min et max
static bool parse_int(const std::string& text, int min, int max, int& value)
{
    if (text.empty() || text.size() > 9) {
        return false;
    }
    for (size_t i = 0; i < text.size(); i++) {
        bool digit = text[i] >= '0' && text[i] <= '9';
        if (!digit && !(i == 0 && text[i] == '-' && text.size() > 1)) {
            return false;
        }
    }
    value = std::atoi(text.c_str());
    return value >= min && value <= max;
}

static bool same_text(const std::string& a, const std::string& b)
{
    if (a.size() != b.size()) {
        return false;
    }
    unsigned char difference = 0;
    for (size_t i = 0; i < a.size(); i++) {
        difference |= static_cast<unsigned char>(a[i] ^ b[i]);
    }
    return difference == 0;
}

static std::string param(const AdminParams& params, const char* key)
{
    auto found = params.find(key);
    return found == params.end() ? "" : found->second;
}

static std::string provider_name(const std::string& provider)
{
    return provider == "discord" ? "Discord" : provider == "google" ? "Google" : provider;
}

// ---------------------------------------------------------------- Réglages et actions

bool ServerApp::is_admin(const std::string& discord_id) const
{
    if (discord_id.empty()) {
        return false;
    }
    for (const std::string& id : config_.admin_discord_ids) {
        if (id == discord_id) {
            return true;
        }
    }
    return false;
}

bool ServerApp::signups_closed()
{
    return store_.setting("signups_closed") == "1";
}

std::string ServerApp::min_version()
{
    std::string stored = store_.setting("min_version");
    return stored.empty() ? config_.min_version : stored;
}

// « 4.0.0 » : trois nombres séparés par des points
static bool valid_version(const std::string& text)
{
    int dots = 0;
    bool digit_before = false;
    for (char c : text) {
        if (c == '.' && digit_before) {
            dots++;
            digit_before = false;
        } else if (c >= '0' && c <= '9') {
            digit_before = true;
        } else {
            return false;
        }
    }
    return dots == 2 && digit_before && text.size() <= 14;
}

bool ServerApp::games_paused()
{
    return store_.setting("games_paused") == "1";
}

json ServerApp::admin_status()
{
    StoreStats stored = store_.stats();
    RoomsStats live = rooms_.stats();
    return {{"version", SPLENDOR_VERSION},
            {"min_version", min_version()},
            {"uptime", static_cast<long long>(std::time(nullptr)) - started_at_},
            {"accounts", stored.accounts},
            {"banned", stored.banned},
            {"games_total", stored.games},
            {"games_today", stored.games_today},
            {"online", live.online},
            {"games_live", live.games},
            {"lobbies", live.lobbies},
            {"queue", live.queue},
            {"season", store_.current_season()},
            {"signups_closed", signups_closed()},
            {"games_paused", games_paused()},
            {"announcement", store_.setting("announcement")}};
}

std::string ServerApp::admin_action(const std::string& actor, const AdminParams& params, json& result)
{
    std::string action = param(params, "action");
    std::string done;   // ce qui est écrit dans le journal
    result = json::object();

    if (action == "status") {
        result = admin_status();
        return "";
    }
    // --- Réglages du serveur
    if (action == "signups" || action == "games") {
        std::string value = param(params, "value");
        bool on = value == "closed" || value == "paused";
        if (!on && value != "open" && value != "on") {
            return "Invalid value.";
        }
        store_.set_setting(action == "signups" ? "signups_closed" : "games_paused", on ? "1" : "");
        done = action == "signups" ? (on ? "New accounts closed" : "New accounts open")
                                   : (on ? "New games paused" : "New games allowed");
    } else if (action == "announce") {
        std::string text = param(params, "text");
        if (text.size() > 300) {
            return "The announcement is too long (300 characters at most).";
        }
        for (char& c : text) {
            if (static_cast<unsigned char>(c) < 32) {
                c = ' ';
            }
        }
        store_.set_setting("announcement", text);
        if (!text.empty()) {
            rooms_.notify_online({{"type", "announcement"}, {"text", text}});
        }
        done = text.empty() ? "Announcement removed" : "Announcement: " + text;
    } else if (action == "min_version") {
        std::string value = param(params, "value");
        if (!value.empty() && !valid_version(value)) {
            return "Write the version as 4.0.0 (empty: back to the server setting).";
        }
        store_.set_setting("min_version", value);
        done = "Minimum game version: " + min_version();
    } else if (action == "end_game") {
        int game = 0;
        if (!parse_int(param(params, "game"), 1, 999999999, game)) {
            return "Invalid game number.";
        }
        std::string error = rooms_.end_game(game);
        if (!error.empty()) {
            return error;
        }
        done = "Game in progress " + std::to_string(game) + " ended (no Elo)";
    } else if (action == "handle_report") {
        int report = 0;
        if (!parse_int(param(params, "report"), 1, 999999999, report) || !store_.handle_report(report)) {
            return "Report not found.";
        }
        done = "Report " + std::to_string(report) + " handled";
    } else if (action == "cancel_game") {
        int game = 0;
        if (!parse_int(param(params, "game"), 1, 999999999, game)) {
            return "Invalid game number.";
        }
        std::string error = store_.cancel_game(game);
        if (!error.empty()) {
            return error;
        }
        done = "Game " + std::to_string(game) + " cancelled";
    } else {
        // --- Actions sur un compte : désigné par son numéro ou son pseudo
        int id = 0;
        std::string username = param(params, "username");
        if (!parse_int(param(params, "account_id"), 1, 999999999, id)) {
            id = username.empty() ? 0 : store_.find_by_pseudo(username);
        }
        Account account;
        if (id == 0 || !store_.get_account(id, account) || account.provider == "supprime") {
            return "Player not found.";
        }
        std::string name = account.pseudo.empty() ? "#" + std::to_string(id) : account.pseudo;
        std::string value = param(params, "value");
        int number = 0;
        int rank = 0;
        if (action == "rename") {
            std::string error = store_.set_pseudo(id, value);
            if (!error.empty()) {
                return error;
            }
            done = "Renamed " + name + " to " + value;
        } else if (action == "elo") {
            if (!parse_int(value, 0, 4000, number)) {
                return "The Elo must be between 0 and 4000.";
            }
            store_.set_elo(id, number);
            done = "Elo of " + name + ": " + std::to_string(account.elo) + " -> " + std::to_string(number);
        } else if (action == "badge") {
            if (!parse_int(param(params, "season"), 1, 9999, number)
                || !parse_int(param(params, "rank"), 1, SEASON_BADGES, rank)) {
                return "Season (1 or more) and rank (1 to " + std::to_string(SEASON_BADGES) + ") needed.";
            }
            store_.set_badge(id, number, rank);
            done = "Badge S" + std::to_string(number) + " #" + std::to_string(rank) + " given to " + name;
        } else if (action == "unbadge") {
            if (!parse_int(param(params, "season"), 1, 9999, number)) {
                return "Invalid season.";
            }
            store_.remove_badge(id, number);
            done = "Badge of season " + std::to_string(number) + " removed from " + name;
        } else if (action == "logout") {
            store_.close_sessions(id);
            done = name + " signed out everywhere";
        } else if (action == "ban") {
            if (!parse_int(param(params, "days"), 0, 3650, number)) {
                return "Invalid duration (0 = permanent).";
            }
            long long until = number == 0 ? BAN_FOREVER : static_cast<long long>(std::time(nullptr)) + number * 86400LL;
            store_.ban(id, until, param(params, "reason").substr(0, 200));
            rooms_.remove_account(id);
            done = name + " banned " + (number == 0 ? std::string("permanently") : "for " + std::to_string(number) + " days");
        } else if (action == "unban") {
            store_.unban(id);
            done = name + " unbanned";
        } else if (action == "delete") {
            if (param(params, "confirm") != "DELETE") {
                return "Type DELETE to confirm.";
            }
            rooms_.remove_account(id);
            store_.delete_account(id);
            done = "Account " + name + " (#" + std::to_string(id) + ") deleted";
        } else {
            return "Unknown action.";
        }
    }
    store_.log_admin(actor, done);
    std::cout << "Administration (" << actor << ") : " << done << std::endl;
    result["message"] = done;
    return "";
}

// ---------------------------------------------------------------- Sessions du panel

std::string ServerApp::open_admin_session(const std::string& discord_id)
{
    std::string token = random_hex(32);
    {
        std::lock_guard<std::mutex> lock(logins_mutex_);
        long long now = std::time(nullptr);
        for (auto it = admin_sessions_.begin(); it != admin_sessions_.end();) {
            it = it->second.expires < now ? admin_sessions_.erase(it) : std::next(it);
        }
        AdminSession& session = admin_sessions_[token];
        session.discord_id = discord_id;
        session.csrf = random_hex(16);
        session.expires = now + ADMIN_SESSION_SECONDS;
    }
    store_.log_admin("discord:" + discord_id, "Signed in to the admin panel");
    bool https = config_.public_url.compare(0, 8, "https://") == 0;
    return std::string(ADMIN_COOKIE) + "=" + token + "; Path=/panel-admin; HttpOnly; SameSite=Strict; Max-Age="
           + std::to_string(ADMIN_SESSION_SECONDS) + (https ? "; Secure" : "");
}

bool ServerApp::admin_session(const httplib::Request& request, AdminSession& session, std::string& token)
{
    std::string cookies = request.get_header_value("Cookie");
    std::string key = std::string(ADMIN_COOKIE) + "=";
    size_t start = cookies.find(key);
    if (start == std::string::npos || (start > 0 && cookies[start - 1] != ' ' && cookies[start - 1] != ';')) {
        return false;
    }
    start += key.size();
    size_t end = cookies.find(';', start);
    token = cookies.substr(start, end == std::string::npos ? std::string::npos : end - start);
    std::lock_guard<std::mutex> lock(logins_mutex_);
    auto found = admin_sessions_.find(token);
    if (found == admin_sessions_.end() || found->second.expires < static_cast<long long>(std::time(nullptr))
        || !is_admin(found->second.discord_id)) {
        return false;
    }
    session = found->second;
    return true;
}

// ---------------------------------------------------------------- Pages du panel

static void reply_admin(httplib::Response& response, const std::string& html, int status = 200)
{
    response.status = status;
    response.set_header("Content-Security-Policy", "default-src 'none'; style-src 'unsafe-inline'; form-action 'self';"
                                                   " frame-ancestors 'none'; base-uri 'none'");
    response.set_header("X-Content-Type-Options", "nosniff");
    response.set_header("Referrer-Policy", "no-referrer");
    response.set_header("Cache-Control", "no-store");
    response.set_header("X-Robots-Tag", "noindex, nofollow");
    // HSTS : le navigateur reste en HTTPS (ignoré sur une connexion en clair)
    response.set_header("Strict-Transport-Security", "max-age=15552000; includeSubDomains");
    response.set_content(html, "text/html; charset=utf-8");
}

static std::string admin_page(const std::string& title, const std::string& body)
{
    return "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\"><link rel=\"icon\" href=\"/favicon.ico\">"
           "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
           "<meta name=\"robots\" content=\"noindex, nofollow\"><title>"
           + html_escape(title)
           + " · Gemspire admin</title><style>"
             "body{margin:0;font:15px/1.5 system-ui,sans-serif;background:#081d13;color:#e6efe9}"
             "main{max-width:1100px;margin:0 auto;padding:20px 16px 60px}"
             "a{color:#ffd140}h1{color:#ffd140;margin:.2em 0}h2{margin:1.6em 0 .5em;font-size:1.15em;color:#ffd140}"
             "header{display:flex;justify-content:space-between;align-items:center;gap:12px;flex-wrap:wrap}"
             ".grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));gap:10px}"
             ".box{background:#0f2e20;border:1px solid #1f4a35;border-radius:10px;padding:12px 14px}"
             ".num{font-size:1.6em;font-weight:700}.soft{color:#9fb8aa;font-size:.9em}"
             "table{width:100%;border-collapse:collapse}td,th{text-align:left;padding:6px 8px;border-bottom:1px solid #1f4a35;"
             "vertical-align:top}th{color:#9fb8aa;font-weight:600}"
             "form{display:inline-flex;gap:6px;flex-wrap:wrap;align-items:center;margin:4px 0}"
             "input{background:#06150e;color:#e6efe9;border:1px solid #2d5e45;border-radius:6px;padding:6px 8px;font:inherit}"
             "button{background:#ffd140;color:#222;border:0;border-radius:6px;padding:7px 12px;font:inherit;font-weight:600;"
             "cursor:pointer}button.grey{background:#2d5e45;color:#e6efe9}button.red{background:#c9473f;color:#fff}"
             ".flash{background:#1f4a35;border-left:4px solid #ffd140;padding:8px 12px;border-radius:6px;margin:12px 0}"
             ".on{color:#ff8a80;font-weight:700}.off{color:#8be0a4;font-weight:700}"
             "</style></head><body><main>"
           + body + "</main></body></html>";
}

// Formulaire qui envoie une action (champs cachés : jeton, action, retour)
static std::string action_form(const AdminSession& session, const std::string& action, const std::string& back,
                               const std::string& fields, const std::string& button, const std::string& style = "",
                               const std::string& hidden = "")
{
    return "<form method=\"post\" action=\"/panel-admin/action\"><input type=\"hidden\" name=\"csrf\" value=\""
           + session.csrf + "\"><input type=\"hidden\" name=\"action\" value=\"" + action
           + "\"><input type=\"hidden\" name=\"back\" value=\"" + html_escape(back) + "\">" + hidden + fields
           + "<button" + (style.empty() ? "" : " class=\"" + style + "\"") + ">" + html_escape(button)
           + "</button></form>";
}

static std::string hidden_account(int id)
{
    return "<input type=\"hidden\" name=\"account_id\" value=\"" + std::to_string(id) + "\">";
}

static std::string header_html(const AdminSession& session, const std::string& title)
{
    return "<header><div><h1>" + html_escape(title) + "</h1><div class=\"soft\"><a href=\"/panel-admin\">Dashboard</a> · "
           "Discord " + html_escape(session.discord_id) + "</div></div><form method=\"post\" action=\"/panel-admin/logout\">"
           "<input type=\"hidden\" name=\"csrf\" value=\"" + session.csrf + "\"><button class=\"grey\">Sign out</button></form></header>"
           + (session.flash.empty() ? "" : "<div class=\"flash\">" + html_escape(session.flash) + "</div>");
}

static std::string stat_box(const std::string& label, const std::string& value)
{
    return "<div class=\"box\"><div class=\"num\">" + html_escape(value) + "</div><div class=\"soft\">" + html_escape(label)
           + "</div></div>";
}

void ServerApp::setup_admin_routes(httplib::Server& server)
{
    started_at_ = std::time(nullptr);

    // Connexion de test (SPLENDOR_DEV_LOGIN=1 seulement) : ?id=<identifiant Discord administrateur>
    server.Get("/panel-admin/dev-login", [this](const httplib::Request& request, httplib::Response& response) {
        std::string id = request.get_param_value("id");
        if (!config_.dev_login || !is_admin(id)) {
            reply_admin(response, admin_page("Access denied", "<h1>Access denied</h1>"), 403);
            return;
        }
        response.set_header("Set-Cookie", open_admin_session(id));
        response.set_redirect("/panel-admin", 303);
    });

    // Tableau de bord ; ?q= cherche un compte
    server.Get("/panel-admin", [this](const httplib::Request& request, httplib::Response& response) {
        AdminSession session;
        std::string token;
        if (!admin_session(request, session, token)) {
            std::string body = "<h1>Gemspire admin</h1><p>Restricted area.</p>";
            if (config_.admin_discord_ids.empty() || config_.discord_client_id.empty()) {
                body += "<p class=\"soft\">Administration is not configured on this server.</p>";
            } else {
                body += "<p><a href=\"/panel-admin/login\"><button>Sign in with Discord</button></a></p>";
            }
            reply_admin(response, admin_page("Sign in", body));
            return;
        }
        {
            std::lock_guard<std::mutex> lock(logins_mutex_);
            admin_sessions_[token].flash.clear();  // affiché une seule fois
        }
        json status = admin_status();
        std::ostringstream out;
        out << header_html(session, "Gemspire admin");
        long long uptime = status["uptime"].get<long long>();
        out << "<h2>Server</h2><div class=\"grid\">"
            << stat_box("version (min. " + min_version() + ")", SPLENDOR_VERSION)
            << stat_box("uptime", std::to_string(uptime / 3600) + " h " + std::to_string(uptime % 3600 / 60) + " min")
            << stat_box("players online", std::to_string(status["online"].get<int>()))
            << stat_box("games in progress", std::to_string(status["games_live"].get<int>()))
            << stat_box("open lobbies", std::to_string(status["lobbies"].get<int>()))
            << stat_box("looking for a match", std::to_string(status["queue"].get<int>()))
            << stat_box("accounts", std::to_string(status["accounts"].get<int>()))
            << stat_box("games (last 24 h)", std::to_string(status["games_total"].get<int>()) + " ("
                                                + std::to_string(status["games_today"].get<int>()) + ")")
            << stat_box("banned accounts", std::to_string(status["banned"].get<int>()))
            << stat_box("season", std::to_string(status["season"].get<int>())) << "</div>";

        // Parties par jour (30 jours) : barres en SVG, sans script
        std::vector<int> days = store_.games_per_day(30);
        int highest = 1;
        for (int count : days) {
            highest = std::max(highest, count);
        }
        out << "<h2>Games per day (30 days, UTC)</h2><div class=\"box\"><svg viewBox=\"0 0 600 150\" width=\"100%\" height=\"170\" preserveAspectRatio=\"xMinYMid meet\" "
               "role=\"img\" aria-label=\"Games per day\"><line x1=\"0\" y1=\"130\" x2=\"600\" y2=\"130\" "
               "stroke=\"#2d5e45\"/>";
        for (int i = 0; i < static_cast<int>(days.size()); i++) {
            int height = days[i] * 110 / highest;
            out << "<rect x=\"" << i * 20 + 3 << "\" y=\"" << 130 - height << "\" width=\"14\" height=\"" << height
                << "\" rx=\"2\" fill=\"" << (i + 1 == static_cast<int>(days.size()) ? "#ffd140" : "#8be0a4")
                << "\"><title>" << days[i] << "</title></rect>";
            if (days[i] > 0) {
                out << "<text x=\"" << i * 20 + 10 << "\" y=\"" << 125 - height << "\" fill=\"#9fb8aa\" font-size=\"10\" "
                       "text-anchor=\"middle\">" << days[i] << "</text>";
            }
        }
        out << "<text x=\"0\" y=\"146\" fill=\"#9fb8aa\" font-size=\"10\">-29 d</text><text x=\"600\" y=\"146\" "
               "fill=\"#9fb8aa\" font-size=\"10\" text-anchor=\"end\">today</text></svg></div>";

        out << "<h2>Minimum game version</h2><p class=\"soft\">Older games get the « Update required » screen. "
               "Empty: back to SPLENDOR_MIN_VERSION (" << html_escape(config_.min_version) << ").</p>"
            << action_form(session, "min_version", "/panel-admin",
                           "<input name=\"value\" size=\"10\" placeholder=\"4.0.0\" value=\""
                               + html_escape(store_.setting("min_version")) + "\">",
                           "Set");

        // Signalements /report (non traités d'abord)
        out << "<h2>Reports</h2>";
        std::vector<Report> reports = store_.reports(30);
        if (reports.empty()) {
            out << "<p class=\"soft\">No report.</p>";
        } else {
            out << "<table><tr><th>Date</th><th>By</th><th>Player</th><th>Reason</th><th></th></tr>";
            for (const Report& report : reports) {
                int target = store_.find_by_pseudo(report.pseudo);
                std::string player = html_escape(report.pseudo);
                if (target != 0) {
                    player = "<a href=\"/panel-admin/account/" + std::to_string(target) + "\">" + player + "</a>";
                }
                out << "<tr><td>" << date_text(report.at) << "</td><td>" << html_escape(report.reporter) << "</td><td>"
                    << player << "</td><td>" << html_escape(report.reason) << "</td><td>"
                    << (report.handled ? "<span class=\"soft\">handled</span>"
                                       : action_form(session, "handle_report", "/panel-admin", "", "Mark handled", "grey",
                                                     "<input type=\"hidden\" name=\"report\" value=\""
                                                         + std::to_string(report.id) + "\">"))
                    << "</td></tr>";
            }
            out << "</table>";
        }

        bool closed = status["signups_closed"].get<bool>();
        bool paused = status["games_paused"].get<bool>();
        out << "<h2>Switches</h2><table>"
            << "<tr><td>New accounts</td><td class=\"" << (closed ? "on\">closed" : "off\">open") << "</td><td>"
            << action_form(session, "signups", "/panel-admin", "", closed ? "Open sign-ups" : "Close sign-ups",
                           closed ? "" : "red",
                           "<input type=\"hidden\" name=\"value\" value=\"" + std::string(closed ? "open" : "closed") + "\">")
            << "</td></tr><tr><td>New games (lobbies, matchmaking, /challenge)</td><td class=\""
            << (paused ? "on\">paused" : "off\">allowed") << "</td><td>"
            << action_form(session, "games", "/panel-admin", "", paused ? "Allow new games" : "Pause new games",
                           paused ? "" : "red",
                           "<input type=\"hidden\" name=\"value\" value=\"" + std::string(paused ? "on" : "paused") + "\">")
            << "</td></tr></table>";

        out << "<h2>Announcement</h2><p class=\"soft\">Shown in the game (home and online menu) and sent at once to "
               "connected players. Leave empty to remove it.</p>"
            << action_form(session, "announce", "/panel-admin",
                           "<input name=\"text\" maxlength=\"300\" size=\"70\" value=\""
                               + html_escape(status["announcement"].get<std::string>()) + "\">",
                           "Publish");

        std::string query = request.get_param_value("q");
        out << "<h2>Accounts</h2><form method=\"get\" action=\"/panel-admin\"><input name=\"q\" placeholder=\"username, "
               "number or Discord ID\" size=\"36\" value=\""
            << html_escape(query) << "\"><button>Search</button></form>";
        if (!query.empty()) {
            std::vector<Account> found = store_.search_accounts(query.substr(0, 64), 30);
            out << "<table><tr><th>#</th><th>Username</th><th>Sign-in</th><th>Elo</th><th>Games</th><th>Status</th></tr>";
            for (const Account& account : found) {
                std::string providers;
                for (const std::string& provider : store_.providers(account.id)) {
                    providers += (providers.empty() ? "" : ", ") + provider_name(provider);
                }
                out << "<tr><td>" << account.id << "</td><td><a href=\"/panel-admin/account/" << account.id << "\">"
                    << html_escape(account.pseudo.empty() ? "(no username)" : account.pseudo) << "</a></td><td>"
                    << html_escape(providers) << "</td><td>" << account.elo << "</td><td>" << account.games << "</td><td>"
                    << (account.banned() ? "<span class=\"on\">banned</span>" : "") << "</td></tr>";
            }
            out << "</table>" << (found.empty() ? "<p class=\"soft\">No account found.</p>" : "");
        }

        json live;
        rooms_.live_games_json(live);
        out << "<h2>Games in progress</h2>";
        if (live.empty()) {
            out << "<p class=\"soft\">None.</p>";
        } else {
            out << "<table><tr><th>#</th><th>Mode</th><th>Round</th><th>Players</th><th></th></tr>";
            for (const json& game : live) {
                std::string players;
                for (const json& player : game["players"]) {
                    players += (players.empty() ? "" : " · ") + player["pseudo"].get<std::string>() + " ("
                               + std::to_string(player["score"].get<int>()) + " pts)";
                }
                out << "<tr><td>" << game["id"].get<int>() << "</td><td>" << html_escape(game["mode"].get<std::string>())
                    << "</td><td>" << game["round"].get<int>() << "</td><td>" << html_escape(players) << "</td><td>"
                    << action_form(session, "end_game", "/panel-admin", "", "End (no Elo)", "red",
                                   "<input type=\"hidden\" name=\"game\" value=\"" + std::to_string(game["id"].get<int>()) + "\">")
                    << "</td></tr>";
            }
            out << "</table>";
        }

        out << "<h2>Admin log</h2><table><tr><th>Date</th><th>By</th><th>Action</th></tr>";
        for (const AdminLogEntry& entry : store_.admin_log(40)) {
            out << "<tr><td>" << date_text(entry.at) << "</td><td>" << html_escape(entry.actor) << "</td><td>"
                << html_escape(entry.action) << "</td></tr>";
        }
        out << "</table>";
        reply_admin(response, admin_page("Dashboard", out.str()));
    });

    // Fiche d'un compte
    server.Get(R"(/panel-admin/account/(\d{1,9}))", [this](const httplib::Request& request, httplib::Response& response) {
        AdminSession session;
        std::string token;
        if (!admin_session(request, session, token)) {
            response.set_redirect("/panel-admin", 303);
            return;
        }
        {
            std::lock_guard<std::mutex> lock(logins_mutex_);
            admin_sessions_[token].flash.clear();
        }
        int id = std::atoi(request.matches[1].str().c_str());
        Account account;
        if (!store_.get_account(id, account) || account.provider == "supprime") {
            reply_admin(response, admin_page("Not found", header_html(session, "Account not found")), 404);
            return;
        }
        std::string back = "/panel-admin/account/" + std::to_string(id);
        std::string hidden = hidden_account(id);
        std::ostringstream out;
        out << header_html(session, account.pseudo.empty() ? "Account #" + std::to_string(id) : account.pseudo);
        out << "<h2>Profile</h2><table>";
        out << "<tr><th>Number</th><td>" << id << "</td></tr>";
        for (const std::string& provider : store_.providers(id)) {
            out << "<tr><th>" << html_escape(provider_name(provider)) << "</th><td>"
                << html_escape(store_.identity(id, provider)) << "</td></tr>";
        }
        int rank = store_.rank_of(id);
        out << "<tr><th>Elo</th><td>" << account.elo << (rank > 0 ? " (rank " + std::to_string(rank) + ")" : "")
            << "</td></tr><tr><th>Games / wins</th><td>" << account.games << " / " << account.wins << "</td></tr>"
            << "<tr><th>Achievements</th><td>" << store_.achievements(id).size() << "</td></tr><tr><th>Badges</th><td>";
        for (const SeasonBadge& badge : store_.badges(id)) {
            out << "S" << badge.season << " #" << badge.rank << " ";
        }
        out << "</td></tr><tr><th>Status</th><td>";
        if (account.banned()) {
            out << "<span class=\"on\">banned "
                << (account.banned_until == BAN_FOREVER ? std::string("permanently") : "until " + date_text(account.banned_until))
                << "</span> " << html_escape(account.ban_reason);
        } else {
            out << html_escape(rooms_.status_of(id));
        }
        out << "</td></tr></table>";

        out << "<h2>Manage</h2><table>"
            << "<tr><td>Username</td><td>"
            << action_form(session, "rename", back, "<input name=\"value\" maxlength=\"32\" value=\"" + html_escape(account.pseudo) + "\">",
                           "Rename", "", hidden)
            << "</td></tr><tr><td>Elo</td><td>"
            << action_form(session, "elo", back, "<input name=\"value\" size=\"6\" value=\"" + std::to_string(account.elo) + "\">",
                           "Set Elo", "", hidden)
            << "</td></tr><tr><td>Season badge</td><td>"
            << action_form(session, "badge", back,
                           "<input name=\"season\" size=\"4\" placeholder=\"season\"><input name=\"rank\" size=\"4\" placeholder=\"rank\">",
                           "Give badge", "", hidden)
            << action_form(session, "unbadge", back, "<input name=\"season\" size=\"4\" placeholder=\"season\">",
                           "Remove badge", "grey", hidden)
            << "</td></tr><tr><td>Sessions</td><td>"
            << action_form(session, "logout", back, "", "Sign out everywhere", "grey", hidden)
            << "</td></tr><tr><td>Ban</td><td>";
        if (account.banned()) {
            out << action_form(session, "unban", back, "", "Unban", "", hidden);
        } else {
            out << action_form(session, "ban", back,
                               "<input name=\"days\" size=\"4\" placeholder=\"days\" value=\"7\"><input name=\"reason\" "
                               "maxlength=\"200\" size=\"30\" placeholder=\"reason\">",
                               "Ban (0 days = permanent)", "red", hidden);
        }
        out << "</td></tr><tr><td>Delete</td><td>"
            << action_form(session, "delete", "/panel-admin", "<input name=\"confirm\" size=\"10\" placeholder=\"type DELETE\">",
                           "Delete the account", "red", hidden)
            << "</td></tr></table>";

        out << "<h2>Last games</h2><table><tr><th>#</th><th>Date</th><th>Mode</th><th>Place</th><th>Elo</th><th>Players</th><th></th></tr>";
        for (const HistoryEntry& entry : store_.history(id, 15)) {
            std::string players;
            for (const std::string& player : entry.players) {
                players += (players.empty() ? "" : ", ") + player;
            }
            out << "<tr><td>" << entry.game_id << "</td><td>" << date_text(entry.finished_at) << "</td><td>"
                << html_escape(entry.mode) << "</td><td>" << entry.place + 1 << "</td><td>"
                << (entry.elo_change >= 0 ? "+" : "") << entry.elo_change << "</td><td>" << html_escape(players) << "</td><td>"
                << (entry.cancelled ? "<span class=\"soft\">cancelled</span>"
                                    : action_form(session, "cancel_game", back, "", "Cancel Elo", "grey",
                                                  "<input type=\"hidden\" name=\"game\" value=\"" + std::to_string(entry.game_id) + "\">"))
                << "</td></tr>";
        }
        out << "</table>";
        reply_admin(response, admin_page(account.pseudo, out.str()));
    });

    // Une action du panel : jeton anti-CSRF vérifié, puis retour à la page d'où elle vient
    server.Post("/panel-admin/action", [this](const httplib::Request& request, httplib::Response& response) {
        AdminSession session;
        std::string token;
        if (!admin_session(request, session, token) || !same_text(request.get_param_value("csrf"), session.csrf)) {
            reply_admin(response, admin_page("Refused", "<h1>Refused</h1><p><a href=\"/panel-admin\">Back</a></p>"), 403);
            return;
        }
        AdminParams params;
        for (const auto& entry : request.params) {
            if (entry.first != "csrf" && entry.first != "back" && entry.second.size() <= 400) {
                params[entry.first] = entry.second;
            }
        }
        json result;
        std::string error = admin_action("discord:" + session.discord_id, params, result);
        {
            std::lock_guard<std::mutex> lock(logins_mutex_);
            admin_sessions_[token].flash = error.empty() ? "Done: " + result.value("message", std::string()) : "Error: " + error;
        }
        // Retour seulement vers une page du panel (pas de redirection ouverte)
        std::string back = request.get_param_value("back");
        bool safe = back.compare(0, 12, "/panel-admin") == 0 && back.find("//") == std::string::npos
                    && back.find('\\') == std::string::npos;
        response.set_redirect(safe ? back : "/panel-admin", 303);
    });

    server.Post("/panel-admin/logout", [this](const httplib::Request& request, httplib::Response& response) {
        AdminSession session;
        std::string token;
        if (admin_session(request, session, token) && same_text(request.get_param_value("csrf"), session.csrf)) {
            std::lock_guard<std::mutex> lock(logins_mutex_);
            admin_sessions_.erase(token);
        }
        response.set_header("Set-Cookie", std::string(ADMIN_COOKIE) + "=; Path=/panel-admin; HttpOnly; SameSite=Strict; Max-Age=0");
        response.set_redirect("/panel-admin", 303);
    });

    // Commandes /dev du bot : clé du bot ET administrateur Discord
    server.Post("/bot/admin", [this](const httplib::Request& request, httplib::Response& response) {
        response.set_header("Cache-Control", "no-store");
        if (!bot_allowed(request)) {
            response.status = 403;
            response.set_content(json({{"error", "Invalid bot key."}}).dump(), "application/json");
            return;
        }
        json body = json::parse(request.body, nullptr, false);
        if (body.is_discarded() || !body.is_object() || !body.contains("admin_discord_id")
            || !body["admin_discord_id"].is_string()) {
            response.status = 400;
            response.set_content(json({{"error", "Malformed request."}}).dump(), "application/json");
            return;
        }
        std::string admin = body["admin_discord_id"].get<std::string>();
        if (!is_admin(admin)) {
            std::cout << "Administration : commande /dev refusée pour Discord " << admin << std::endl;
            response.status = 403;
            response.set_content(json({{"error", "You are not a Gemspire administrator."}}).dump(), "application/json");
            return;
        }
        AdminParams params;
        for (auto it = body.begin(); it != body.end(); ++it) {
            if (it.value().is_string() && it.value().get<std::string>().size() <= 400) {
                params[it.key()] = it.value().get<std::string>();
            } else if (it.value().is_number_integer()) {
                params[it.key()] = std::to_string(it.value().get<long long>());
            }
        }
        json result;
        std::string error = admin_action("discord:" + admin + " (bot)", params, result);
        response.status = error.empty() ? 200 : 400;
        response.set_content((error.empty() ? result : json({{"error", error}})).dump(), "application/json; charset=utf-8");
    });
}
