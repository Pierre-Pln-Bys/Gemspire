#include "pages.hpp"

#include <sstream>

static const char* const RELEASES = "https://github.com/Pierre-Pln-Bys/Gemspire/releases/latest";
static const char* const DOWNLOAD = "https://github.com/Pierre-Pln-Bys/Gemspire/releases/latest/download/";

// Le même texte en anglais ou en français
static std::string T(Lang lang, const std::string& english, const std::string& french)
{
    return lang == Lang::French ? french : english;
}

Lang page_lang(const std::string& lang_parameter, const std::string& accept_language, Lang fallback)
{
    if (lang_parameter == "fr") {
        return Lang::French;
    }
    if (lang_parameter == "en") {
        return Lang::English;
    }
    if (accept_language.compare(0, 2, "fr") == 0) {
        return Lang::French;
    }
    if (!accept_language.empty()) {
        return Lang::English;
    }
    return fallback;
}

std::string html_escape(const std::string& text)
{
    std::string out;
    for (char c : text) {
        switch (c) {
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '&': out += "&amp;"; break;
        case '"': out += "&quot;"; break;
        case '\'': out += "&#39;"; break;
        default: out += c; break;
        }
    }
    return out;
}

// Lien vers une page du site, en gardant la langue
static std::string href(const std::string& path, Lang lang)
{
    return path + (lang == Lang::French ? "?lang=fr" : "?lang=en");
}

// Les cinq jetons et l'or, dessinés en CSS (pas d'image à servir)
static std::string tokens_html()
{
    return "<div class=\"tokens\" aria-hidden=\"true\">"
           "<span class=\"t white\">●</span><span class=\"t blue\">◆</span><span class=\"t green\">▲</span>"
           "<span class=\"t gold\">✶</span>"
           "<span class=\"t red\">■</span><span class=\"t black\">⬢</span></div>";
}

// Cadre commun : en-tête, menu, choix de la langue, pied de page
static std::string layout(const SiteInfo& site, Lang lang, const std::string& title, const std::string& current,
                          const std::string& body)
{
    std::ostringstream out;
    out << "<!doctype html><html lang=\"" << T(lang, "en", "fr")
        << "\"><head><meta charset=\"utf-8\">"
           "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
           "<link rel=\"icon\" href=\"/favicon.ico\" sizes=\"48x48\">"
           "<link rel=\"apple-touch-icon\" href=\"/apple-touch-icon.png\">"
           "<title>"
        << html_escape(title) << " · Gemspire</title><meta name=\"description\" content=\""
        << T(lang, "Gemspire online: a game of gems and nobles, 2 to 4 players, against the computer or online with "
                   "an Elo leaderboard.",
             "Gemspire en ligne : jeu de gemmes et de nobles, 2 à 4 joueurs, contre l'ordinateur ou en ligne avec "
             "classement Elo.")
        << "\"><style>"
           ":root{--felt:#0f3d28;--felt2:#165636;--gold:#ffd140;--text:#eef3ef;--soft:#b9d2c2}"
           "*{box-sizing:border-box}"
           "body{margin:0;font-family:system-ui,-apple-system,'Segoe UI',Roboto,sans-serif;color:var(--text);"
           "background:radial-gradient(ellipse at top,var(--felt2),var(--felt) 70%) fixed;line-height:1.6}"
           "a{color:var(--gold)}"
           "header{display:flex;flex-wrap:wrap;align-items:center;justify-content:space-between;gap:8px;"
           "padding:14px 20px;max-width:960px;margin:0 auto}"
           ".brand{font-weight:800;font-size:1.4rem;color:var(--gold);text-decoration:none;letter-spacing:.04em}"
           "nav{display:flex;flex-wrap:wrap;gap:4px;align-items:center}"
           "nav a{color:var(--text);text-decoration:none;padding:6px 10px;border-radius:8px}"
           "nav a.on,nav a:hover{background:rgba(255,255,255,.1);color:var(--gold)}"
           "nav a.lang{border:1px solid rgba(255,255,255,.25);font-size:.85rem}"
           "main{max-width:960px;margin:0 auto;padding:8px 20px 40px}"
           "h1{color:var(--gold);font-size:2.2rem;margin:.4em 0}"
           "h2{color:var(--gold);margin-top:1.6em}.card>h2:first-child{margin-top:.2em}"
           ".card{background:rgba(0,0,0,.22);border:1px solid rgba(255,255,255,.08);border-radius:14px;"
           "padding:18px 20px;margin:14px 0}"
           ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:14px}"
           ".button{display:inline-block;background:var(--gold);color:#1d2a1f;font-weight:700;"
           "text-decoration:none;padding:10px 18px;border-radius:10px;margin:4px 6px 4px 0}"
           ".button.big{font-size:1.15rem;padding:14px 26px}"
           ".button.ghost{background:transparent;color:var(--gold);border:2px solid var(--gold)}"
           ".tokens{display:flex;gap:10px;flex-wrap:wrap;margin:10px 0 4px}"
           ".t{width:48px;height:48px;border-radius:50%;display:grid;place-items:center;font-size:20px;"
           "border:4px dashed rgba(255,255,255,.55);box-shadow:0 3px 8px rgba(0,0,0,.35)}"
           ".white{background:#f2f2ed;color:#333;border-color:#aaa}.blue{background:#2961d1;color:#fff}"
           ".green{background:#1a944d;color:#fff}.red{background:#d9292e;color:#fff}"
           ".black{background:#1f1f21;color:#fff}.gold{background:#f5c21f;color:#333}"
           "table{width:100%;border-collapse:collapse}"
           "th,td{padding:10px 8px;text-align:left;border-bottom:1px solid rgba(255,255,255,.08)}"
           "th{color:var(--soft);font-weight:600;font-size:.9rem}"
           "td.num,th.num{text-align:right;font-variant-numeric:tabular-nums}"
           "tr.top td:first-child{font-size:1.2rem}"
           ".soft{color:var(--soft)}"
           "footer{max-width:960px;margin:0 auto;padding:20px;color:var(--soft);font-size:.85rem}"
           "@media (max-width:560px){h1{font-size:1.7rem}.hide-small{display:none}th,td{padding:8px 4px}}"
           "</style></head><body><header><a class=\"brand\" href=\""
        << href("/", lang) << "\">GEMSPIRE</a><nav>";
    struct Link {
        const char* path;
        std::string label;
    };
    std::vector<Link> links = {{"/", T(lang, "Home", "Accueil")}};
    if (site.web_game) {
        links.push_back({"/play", T(lang, "Play", "Jouer")});
    }
    links.push_back({"/leaderboard", T(lang, "Leaderboard", "Classement")});
    links.push_back({"/terms", T(lang, "Terms", "Conditions")});
    links.push_back({"/privacy", T(lang, "Privacy", "Confidentialité")});
    for (const Link& link : links) {
        out << "<a href=\"" << href(link.path, lang) << "\"" << (current == link.path ? " class=\"on\"" : "") << ">"
            << link.label << "</a>";
    }
    // Choix de la langue : la même page dans l'autre langue
    Lang other = lang == Lang::French ? Lang::English : Lang::French;
    out << "<a class=\"lang\" href=\"" << href(current.empty() ? "/" : current, other) << "\" hreflang=\""
        << T(other, "en", "fr") << "\">" << T(lang, "Français", "English") << "</a>";
    out << "</nav></header><main>" << body << "</main><footer>"
        << T(lang, "© 2026 Gemspire. All rights reserved. Student project (ENSEIRB-MATMECA), independent game, not "
                   "affiliated with any board game publisher.",
             "© 2026 Gemspire. Tous droits réservés. Projet étudiant (ENSEIRB-MATMECA), jeu indépendant, sans lien "
             "avec un éditeur de jeux de société.")
        << " · <a href=\"" << href("/terms", lang) << "\">" << T(lang, "Terms", "Conditions") << "</a> · <a href=\""
        << href("/privacy", lang) << "\">" << T(lang, "Privacy", "Confidentialité") << "</a>";
    if (!site.discord_invite.empty()) {
        out << " · <a href=\"" << html_escape(site.discord_invite) << "\">Discord</a>";
    }
    out << "</footer></body></html>";
    return out.str();
}

// Comment nous contacter (adresse e-mail, serveur Discord)
static std::string contact_html(const SiteInfo& site, Lang lang)
{
    std::string out;
    if (!site.contact_email.empty()) {
        std::string email = html_escape(site.contact_email);
        out += T(lang, "by email at ", "par e-mail à ") + "<a href=\"mailto:" + email + "\">" + email + "</a>, ";
    }
    if (!site.discord_invite.empty()) {
        out += T(lang, "or on ", "ou sur ") + "<a href=\"" + html_escape(site.discord_invite) + "\">"
               + T(lang, "the Discord server", "le serveur Discord") + "</a>";
    }
    // Sans serveur Discord : pas de virgule finale après l'adresse
    if (out.size() >= 2 && out.compare(out.size() - 2, 2, ", ") == 0) {
        out.erase(out.size() - 2);
    }
    return out;
}

std::string page_home(const SiteInfo& site, Lang lang)
{
    std::ostringstream body;
    body << "<section class=\"card\">" << tokens_html() << "<h1>Gemspire</h1><p>"
         << T(lang, "Collect gems, buy development cards and attract nobles. The first to reach 15 prestige points "
                    "triggers the last round.",
              "Collectionnez des gemmes, achetez des cartes de développement et attirez les nobles. Le premier à 15 "
              "points de prestige déclenche le dernier tour.")
         << "</p><p>"
         << T(lang, "Play <strong>alone against the computer</strong> or <strong>online</strong> with 2 to 4 players, "
                    "with an Elo leaderboard. Sign in with Discord or Google, no password.",
              "Jouez <strong>seul contre l'ordinateur</strong> ou <strong>en ligne</strong> de 2 à 4 joueurs, avec un "
              "classement Elo. Connexion avec Discord ou Google, sans mot de passe.")
         << "</p>";
    if (site.web_game) {
        body << "<a class=\"button big\" href=\"" << href("/play", lang) << "\">"
             << T(lang, "▶ Play in your browser", "▶ Jouer dans le navigateur") << "</a><br>";
    }
    body << "<a class=\"button\" href=\"" << DOWNLOAD << "gemspire-windows.exe\">"
         << T(lang, "Download for Windows", "Télécharger pour Windows") << "</a>"
         << "<a class=\"button\" href=\"" << DOWNLOAD << "gemspire-linux\">"
         << T(lang, "Download for Linux", "Télécharger pour Linux") << "</a>"
         << "<a class=\"button ghost\" href=\"" << href("/leaderboard", lang) << "\">"
         << T(lang, "See the leaderboard", "Voir le classement") << "</a></section>";
    body << "<section class=\"card\"><h2>" << T(lang, "What Gemspire is", "Qu'est-ce que Gemspire ?") << "</h2><p>"
         << T(lang, "Gemspire is a free strategy board game for 2 to 4 players, made as a student project. You can play "
                    "it <strong>without any account</strong>: download it or open it in your browser and play against "
                    "the computer or with friends on the same screen.",
              "Gemspire est un jeu de plateau de stratégie gratuit, de 2 à 4 joueurs, réalisé dans le cadre d'un projet "
              "étudiant. On peut y jouer <strong>sans aucun compte</strong> : téléchargez-le ou ouvrez-le dans le "
              "navigateur, et jouez contre l'ordinateur ou entre amis sur le même écran.")
         << "</p><p>"
         << T(lang, "An account is only needed to <strong>play online</strong> (games against other players, Elo "
                    "leaderboard, friends). You create it with <strong>Sign in with Google</strong> or with Discord: "
                    "Gemspire only receives an anonymous account identifier from Google (no name, email, photo or "
                    "other Google data), used solely to recognize your account the next time you sign in. Details in "
                    "the ",
              "Un compte n'est utile que pour <strong>jouer en ligne</strong> (parties contre d'autres joueurs, "
              "classement Elo, amis). Il se crée avec <strong>Se connecter avec Google</strong> ou avec Discord : "
              "Gemspire ne reçoit de Google qu'un identifiant de compte anonyme (ni nom, ni e-mail, ni photo, ni "
              "aucune autre donnée Google), utilisé uniquement pour reconnaître votre compte à la connexion "
              "suivante. Détails dans la ")
         << "<a href=\"" << href("/privacy", lang) << "\">" << T(lang, "privacy policy", "politique de confidentialité")
         << "</a>.</p></section>";
    body << "<div class=\"grid\"><div class=\"card\"><h3>" << T(lang, "Two modes", "Deux modes") << "</h3><p>"
         << T(lang, "<strong>Quick</strong>: no gold, no reservations. <strong>Classic</strong>: with gold tokens "
                    "(wild) and card reservations.",
              "<strong>Rapide</strong> : sans or ni réservation. <strong>Classique</strong> : avec les jetons or "
              "(joker) et la réservation de cartes.")
         << "</p></div><div class=\"card\"><h3>" << T(lang, "Online", "En ligne") << "</h3><p>"
         << T(lang, "The server referees every move: no cheating on the rules. 2 minutes per turn; missing 3 turns "
                    "in a row counts as forfeiting.",
              "Le serveur arbitre chaque coup : pas de triche possible sur les règles. 2 minutes par tour ; 3 tours "
              "manqués d'affilée valent un abandon.")
         << "</p></div><div class=\"card\"><h3>Discord</h3><p>"
         << T(lang, "Your game shows on your profile, friends join in one click, and the « Create a game » voice "
                    "channel sets everything up.",
              "Votre partie s'affiche sur votre profil, vos amis la rejoignent en un clic, et le salon vocal « Crée "
              "ta partie » prépare tout.")
         << "</p></div></div>";
    body << "<section class=\"card\"><h2>" << T(lang, "Install", "Installer") << "</h2><p>"
         << T(lang, "A single file, nothing to install: download it and run it. On Windows, if SmartScreen shows a "
                    "warning (unsigned executable), click <em>More info</em> then <em>Run anyway</em>. On Linux: "
                    "<code>chmod +x gemspire-linux</code> then <code>./gemspire-linux</code>.",
              "Un seul fichier, rien à installer : téléchargez-le et lancez-le. Sous Windows, si SmartScreen "
              "affiche un avertissement (exécutable non signé), cliquez sur <em>Informations complémentaires</em> "
              "puis <em>Exécuter quand même</em>. Sous Linux : <code>chmod +x gemspire-linux</code> puis "
              "<code>./gemspire-linux</code>.")
         << "</p><p>" << T(lang, "All versions (including the console version): ", "Toutes les versions (dont la version console) : ")
         << "<a href=\"" << RELEASES << "\">" << T(lang, "releases page", "page des versions") << "</a>.</p></section>";
    if (!site.discord_invite.empty()) {
        body << "<section class=\"card\"><h2>" << T(lang, "Join the community", "Rejoindre la communauté")
             << "</h2><p>"
             << T(lang, "Find players, report a bug or suggest an idea on our Discord server.",
                  "Trouvez des joueurs, signalez un bug ou proposez une idée sur notre serveur Discord.")
             << "</p><a class=\"button\" href=\"" << html_escape(site.discord_invite) << "\">"
             << T(lang, "Join the Discord", "Rejoindre le Discord") << "</a></section>";
    }
    return layout(site, lang, T(lang, "Home", "Accueil"), "/", body.str());
}

// Date (jour, mois, année) d'un instant donné en temps UTC, sans fonction du
// système (algorithme classique « civil from days »)
static void civil_date(long long seconds, int& year, int& month, int& day)
{
    long long days = (seconds >= 0 ? seconds : seconds - 86399) / 86400 + 719468;
    long long era = (days >= 0 ? days : days - 146096) / 146097;
    long long day_of_era = days - era * 146097;
    long long year_of_era = (day_of_era - day_of_era / 1460 + day_of_era / 36524 - day_of_era / 146096) / 365;
    long long day_of_year = day_of_era - (365 * year_of_era + year_of_era / 4 - year_of_era / 100);
    long long mp = (5 * day_of_year + 2) / 153;
    day = static_cast<int>(day_of_year - (153 * mp + 2) / 5 + 1);
    month = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
    year = static_cast<int>(year_of_era + era * 400 + (month <= 2 ? 1 : 0));
}

// Fin d'une saison en toutes lettres : « January 1, 2027 » / « 1er janvier 2027 »
// (le changement a lieu à minuit à Paris : 22 h ou 23 h UTC la veille)
static std::string season_end_text(Lang lang, long long ends_at)
{
    int year = 0;
    int month = 1;
    int day = 1;
    civil_date(ends_at + 3 * 3600, year, month, day);
    const char* months_en[12] = {"January", "February", "March",     "April",   "May",      "June",
                                 "July",    "August",   "September", "October", "November", "December"};
    const char* months_fr[12] = {"janvier", "février", "mars",      "avril",   "mai",      "juin",
                                 "juillet", "août",    "septembre", "octobre", "novembre", "décembre"};
    if (lang == Lang::French) {
        return (day == 1 ? std::string("1er") : std::to_string(day)) + " " + months_fr[month - 1] + " "
               + std::to_string(year);
    }
    return std::string(months_en[month - 1]) + " " + std::to_string(day) + ", " + std::to_string(year);
}

// Un tableau de classement ; `season` : colonnes de la saison (Elo, parties, victoires de saison)
static void ranking_table(std::ostringstream& body, Lang lang, const std::vector<Account>& players, bool season)
{
    body << "<table><thead><tr><th>#</th><th>" << T(lang, "Player", "Joueur") << "</th><th class=\"num\">Elo</th>"
         << "<th class=\"num hide-small\">" << T(lang, "Games", "Parties") << "</th><th class=\"num\">"
         << T(lang, "Wins", "Victoires") << "</th></tr></thead><tbody>";
    const char* medals[] = {"🥇", "🥈", "🥉"};
    for (size_t i = 0; i < players.size(); i++) {
        const Account& player = players[i];
        int elo = season ? player.season_elo : player.elo;
        int games = season ? player.season_games : player.ranked_games;
        int wins = season ? player.season_wins : player.ranked_wins;
        int rate = games > 0 ? wins * 100 / games : 0;
        body << "<tr" << (i < 3 ? " class=\"top\"" : "") << "><td>" << (i < 3 ? medals[i] : std::to_string(i + 1))
             << "</td><td>" << html_escape(player.pseudo) << "</td><td class=\"num\"><strong>" << elo
             << "</strong></td><td class=\"num hide-small\">" << games << "</td><td class=\"num\">" << wins
             << " <span class=\"soft\">(" << rate << T(lang, "%", " %") << ")</span></td></tr>";
    }
    body << "</tbody></table>";
}

std::string page_leaderboard(const SiteInfo& site, Lang lang, const std::vector<Account>& players,
                             const std::vector<Account>& season_players, const SeasonInfo& season)
{
    std::ostringstream body;
    body << "<h1>" << T(lang, "Leaderboard", "Classement") << "</h1>";
    // Saison en cours : file « Saison » de la recherche de partie
    body << "<h2>" << T(lang, "Season ", "Saison ") << season.number << "</h2><p class=\"soft\">"
         << T(lang, "Season games (matchmaking, Season queue). Everyone starts the season at 1000 Elo; at least ",
              "Parties de saison (recherche de partie, file Saison). Tout le monde commence la saison à 1000 Elo ; ")
         << SEASON_MIN_GAMES
         << T(lang, " games to be ranked. The season ends on ", " parties minimum pour être classé. La saison se termine le ")
         << season_end_text(lang, season.ends_at)
         << T(lang, ": the top 10 earn a badge.", " : les 10 premiers gagnent un badge.") << "</p><div class=\"card\">";
    if (season_players.empty()) {
        body << "<p>"
             << T(lang, "Nobody is ranked yet this season. Play season games to get in!",
                  "Personne n'est encore classé cette saison. Jouez des parties de saison pour y entrer !")
             << "</p>";
    } else {
        ranking_table(body, lang, season_players, true);
    }
    body << "</div>";
    // Classement global : file « Classé », jamais remis à zéro
    body << "<h2>" << T(lang, "Global ranking", "Classement global") << "</h2><p class=\"soft\">"
         << T(lang, "Ranked games (matchmaking, Ranked queue). This Elo is never reset. Games in a private lobby are "
                    "friendly and do not count.",
              "Parties classées (recherche de partie, file Classé). Cet Elo n'est jamais remis à zéro. Les parties "
              "en salon privé sont amicales et ne comptent pas.")
         << "</p><div class=\"card\">";
    if (players.empty()) {
        body << "<p>"
             << T(lang, "Nobody has finished a ranked game yet. Be the first!",
                  "Personne n'a encore terminé de partie classée. Soyez le premier !")
             << "</p>";
    } else {
        ranking_table(body, lang, players, false);
    }
    body << "</div>";
    return layout(site, lang, T(lang, "Leaderboard", "Classement"), "/leaderboard", body.str());
}

std::string page_terms(const SiteInfo& site, Lang lang)
{
    std::ostringstream body;
    body << "<h1>" << T(lang, "Terms of use", "Conditions d'utilisation") << "</h1><p class=\"soft\">"
         << T(lang, "Last updated: November 25, 2026", "Dernière mise à jour : 25 novembre 2026")
         << "</p><div class=\"card\"><h2>" << T(lang, "1. The service", "1. Le service") << "</h2><p>"
         << T(lang, "Gemspire is a free-to-play student project: a game (Windows, Linux, browser), an online game "
                    "server, this website and a Discord bot. It is an independent project, not affiliated with any "
                    "board game author or publisher.",
              "Gemspire est un projet étudiant gratuit : un jeu (Windows, Linux, navigateur), un serveur de parties "
              "en ligne, ce site et un bot Discord. C'est un projet indépendant, sans lien avec un auteur ou un "
              "éditeur de jeux de société.")
         << "</p><p>"
         << T(lang, "<strong>Intellectual property.</strong> The game, its source code, graphics, texts and this "
                    "website are protected by copyright: © 2026 Gemspire, all rights reserved. You may download "
                    "and play Gemspire for free for your personal use. Copying, redistributing, modifying, "
                    "decompiling or reselling it, in whole or in part, is forbidden without written permission. "
                    "Third-party components keep their own licenses (raylib: zlib; cpp-httplib, nlohmann/json: MIT; "
                    "SQLite: public domain; DejaVu fonts: free font license).",
              "<strong>Propriété intellectuelle.</strong> Le jeu, son code source, ses graphismes, ses textes et ce "
              "site sont protégés par le droit d'auteur : © 2026 Gemspire, tous droits réservés. Vous pouvez "
              "télécharger Gemspire et y jouer gratuitement pour votre usage personnel. Le copier, le redistribuer, "
              "le modifier, le décompiler ou le revendre, en tout ou partie, est interdit sans autorisation écrite. "
              "Les composants tiers gardent leur propre licence (raylib : zlib ; cpp-httplib, nlohmann/json : MIT ; "
              "SQLite : domaine public ; polices DejaVu : licence de police libre).")
         << "</p><h2>" << T(lang, "2. Account", "2. Compte") << "</h2><p>"
         << T(lang, "You sign in with a Discord or Google account; no password is created or stored. You choose a "
                    "username visible to other players and on the leaderboard. It must not be insulting, "
                    "discriminatory or impersonate someone.",
              "On se connecte avec un compte Discord ou Google ; aucun mot de passe n'est créé ni stocké. Vous "
              "choisissez un pseudo visible par les autres joueurs et dans le classement. Il ne doit pas être "
              "injurieux, discriminatoire, ni usurper l'identité de quelqu'un.")
         << "</p><h2>" << T(lang, "3. Fair play", "3. Règles de bonne conduite") << "</h2><p>"
         << T(lang, "Forbidden: cheating (a program playing for you, collusion between players, several accounts to "
                    "gain points), forfeiting on purpose to avoid a loss, attacks against the server, harassment on "
                    "the Discord server.",
              "Sont interdits : la triche (programme qui joue à votre place, entente entre joueurs, plusieurs "
              "comptes pour gagner des points), l'abandon volontaire pour éviter une défaite, les attaques contre le "
              "serveur, le harcèlement sur le serveur Discord.")
         << "</p><h2>" << T(lang, "4. Turn time and forfeit", "4. Temps de jeu et abandon") << "</h2><p>"
         << T(lang, "Online, each player has 2 minutes per turn. After that, the turn passes. After 3 missed turns "
                    "in a row, or when forfeiting, the player is ranked last and loses Elo points; the game goes on "
                    "between the other players.",
              "En ligne, chaque joueur dispose de 2 minutes par tour. Passé ce délai, son tour passe. Après 3 tours "
              "manqués d'affilée, ou s'il abandonne, il est classé dernier et perd des points Elo ; la partie "
              "continue entre les autres joueurs.")
         << "</p><h2>" << T(lang, "5. Sanctions", "5. Sanctions") << "</h2><p>"
         << T(lang, "In case of breach, moderators may cancel the points of a game, suspend or delete an account, "
                    "and ban from the Discord server.",
              "En cas de manquement, les modérateurs peuvent annuler les points d'une partie, suspendre ou supprimer "
              "un compte, et exclure du serveur Discord.")
         << "</p><h2>" << T(lang, "6. Availability", "6. Disponibilité") << "</h2><p>"
         << T(lang, "The service is provided as is, without warranty: it may be interrupted, changed or stopped at "
                    "any time (the server runs on a Raspberry Pi). Leaderboards may be reset.",
              "Le service est fourni tel quel, sans garantie : il peut être interrompu, modifié ou arrêté à tout "
              "moment (le serveur est hébergé sur un Raspberry Pi). Les classements peuvent être remis à zéro.")
         << "</p><h2>" << T(lang, "7. Personal data", "7. Données personnelles") << "</h2><p>"
         << T(lang, "See the ", "Voir la ") << "<a href=\"" << href("/privacy", lang) << "\">"
         << T(lang, "privacy policy", "politique de confidentialité") << "</a>.</p><h2>"
         << T(lang, "8. Contact", "8. Contact") << "</h2><p>" << T(lang, "For any question: ", "Pour toute question : ")
         << contact_html(site, lang) << ".</p></div>";
    return layout(site, lang, T(lang, "Terms of use", "Conditions d'utilisation"), "/terms", body.str());
}

std::string page_privacy(const SiteInfo& site, Lang lang)
{
    std::ostringstream body;
    body << "<h1>" << T(lang, "Privacy policy", "Politique de confidentialité") << "</h1><p class=\"soft\">"
         << T(lang, "Last updated: November 24, 2026", "Dernière mise à jour : 24 novembre 2026")
         << "</p><div class=\"card\">"

         << "<h2>" << T(lang, "1. Who we are", "1. Qui sommes-nous") << "</h2><p>"
         << T(lang, "Gemspire (the game, the website gemspire.epsilon-forge.fr, the online game server and the "
                    "Discord bot) is a free student project developed at ENSEIRB-MATMECA (Bordeaux, France). It is "
                    "responsible for the data described here. Contact: ",
              "Gemspire (le jeu, le site gemspire.epsilon-forge.fr, le serveur de jeu en ligne et le bot Discord) "
              "est un projet étudiant gratuit développé à l'ENSEIRB-MATMECA (Bordeaux, France). Il est responsable "
              "des données décrites ici. Contact : ")
         << contact_html(site, lang) << ".</p><p>"
         << T(lang, "Playing offline (against the computer or on the same screen) requires no account and sends "
                    "nothing to our server. This policy only concerns online play, the website and the Discord bot.",
              "Jouer hors ligne (contre l'ordinateur ou sur le même écran) ne demande aucun compte et n'envoie rien "
              "à notre serveur. Cette politique concerne seulement le jeu en ligne, le site et le bot Discord.")
         << "</p>"

         << "<h2>" << T(lang, "2. Data we collect", "2. Données que nous collectons") << "</h2><ul><li>"
         << T(lang, "<strong>Your Discord and/or Google account identifier</strong> (one per sign-in you link): a "
                    "number, not your name or email address.",
              "<strong>L'identifiant de votre compte Discord et/ou Google</strong> (un par connexion liée) : un "
              "numéro, pas votre nom ni votre adresse e-mail.")
         << "</li><li>"
         << T(lang, "<strong>Your username</strong> (chosen by you), your <strong>Elo</strong>, number of games and "
                    "wins, the history of your online games (date, mode, place, score, Elo change) and their replays "
                    "(the moves played).",
              "<strong>Votre pseudo</strong> (choisi par vous), votre <strong>Elo</strong>, nombre de parties et de "
              "victoires, l'historique de vos parties en ligne (date, mode, place, score, variation d'Elo) et leurs "
              "rediffusions (les coups joués).")
         << "</li><li>"
         << T(lang, "<strong>Your friend list</strong>, the <strong>achievements</strong> and season "
                    "<strong>badges</strong> you earned.",
              "<strong>Votre liste d'amis</strong>, vos <strong>succès</strong> et <strong>badges</strong> de "
              "saison.")
         << "</li><li>"
         << T(lang, "<strong>A fingerprint (SHA-256) of your session token</strong>; the token itself stays on your "
                    "device.",
              "<strong>Une empreinte (SHA-256) de votre jeton de session</strong> ; le jeton lui-même reste sur "
              "votre appareil.")
         << "</li><li>"
         << T(lang, "<strong>Moderation data</strong>: suspensions (date, reason) and reports sent with "
                    "<code>/report</code> on Discord (who reported, the username and the reason).",
              "<strong>Données de modération</strong> : suspensions (date, motif) et signalements envoyés avec "
              "<code>/report</code> sur Discord (qui signale, le pseudo et le motif).")
         << "</li></ul><p>"
         << T(lang, "We do <strong>not</strong> collect your email address, real name, profile picture, contacts, "
                    "location or password. Chat messages are never written to the database.",
              "Nous ne collectons <strong>pas</strong> votre adresse e-mail, votre nom, votre photo, vos contacts, "
              "votre position ni de mot de passe. Les messages du chat ne sont jamais écrits dans la base.")
         << "</p>"

         << "<h2>" << T(lang, "3. Google user data", "3. Données utilisateur Google") << "</h2><ul><li>"
         << T(lang, "<strong>What we access:</strong> when you choose « Sign in with Google », we only request the "
                    "<code>openid</code> scope. Google then gives us your account's unique identifier "
                    "(<code>sub</code>). We do not request your email, profile, contacts, Drive, Gmail or any other "
                    "Google API.",
              "<strong>Ce que nous obtenons :</strong> quand vous choisissez « Se connecter avec Google », nous ne "
              "demandons que le droit <code>openid</code>. Google nous donne alors l'identifiant unique de votre "
              "compte (<code>sub</code>). Nous ne demandons ni votre e-mail, ni votre profil, ni vos contacts, ni "
              "Drive, Gmail ou toute autre API Google.")
         << "</li><li>"
         << T(lang, "<strong>How we use it:</strong> only to recognize your Gemspire account when you sign in again.",
              "<strong>Ce que nous en faisons :</strong> uniquement reconnaître votre compte Gemspire quand vous vous "
              "reconnectez.")
         << "</li><li>"
         << T(lang, "<strong>Storage and protection:</strong> the identifier is stored in our database, on our "
                    "server, linked to your account. The temporary Google access token is used once to read the "
                    "identifier and is never stored.",
              "<strong>Stockage et protection :</strong> l'identifiant est stocké dans notre base de données, sur "
              "notre serveur, lié à votre compte. Le jeton d'accès Google temporaire sert une seule fois à lire "
              "l'identifiant et n'est jamais stocké.")
         << "</li><li>"
         << T(lang, "<strong>Sharing:</strong> it is never shared, sold or transferred to anyone, never used for "
                    "advertising, and never used to train or improve artificial intelligence or machine learning "
                    "models.",
              "<strong>Partage :</strong> il n'est jamais partagé, vendu ni transmis à qui que ce soit, jamais "
              "utilisé pour de la publicité, et jamais utilisé pour entraîner ou améliorer des modèles "
              "d'intelligence artificielle ou d'apprentissage automatique.")
         << "</li><li>"
         << T(lang, "<strong>Deletion:</strong> it is erased when your account is deleted (see section 7). You can "
                    "also remove Gemspire's access at any time from ",
              "<strong>Suppression :</strong> il est effacé à la suppression de votre compte (voir section 7). Vous "
              "pouvez aussi retirer l'accès de Gemspire à tout moment depuis ")
         << "<a href=\"https://myaccount.google.com/permissions\">myaccount.google.com/permissions</a>.</li></ul><p>"
         << T(lang, "Gemspire's use and transfer of information received from Google APIs adheres to the ",
              "L'utilisation et le transfert par Gemspire des informations reçues des API Google respectent les ")
         << "<a href=\"https://developers.google.com/terms/api-services-user-data-policy\">"
         << T(lang, "Google API Services User Data Policy", "règles des services d'API Google relatives aux données utilisateur")
         << "</a>" << T(lang, ", including the Limited Use requirements.", ", y compris les exigences d'utilisation limitée.")
         << "</p>"

         << "<h2>" << T(lang, "4. Why we use your data", "4. Pourquoi nous utilisons vos données") << "</h2><ul><li>"
         << T(lang, "Sign you in and keep you signed in.", "Vous connecter et garder votre session.")
         << "</li><li>"
         << T(lang, "Run online games: lobbies, matchmaking, the referee of each move, chat, spectators and replays.",
              "Faire fonctionner les parties en ligne : salons, recherche d'adversaire, arbitrage des coups, chat, "
              "spectateurs et rediffusions.")
         << "</li><li>"
         << T(lang, "Compute the Elo, the public leaderboard, seasons, achievements and friend lists.",
              "Calculer l'Elo, le classement public, les saisons, les succès et les listes d'amis.")
         << "</li><li>"
         << T(lang, "Moderation: fight cheating and abuse, handle reports.",
              "La modération : lutter contre la triche et les abus, traiter les signalements.")
         << "</li></ul><p>"
         << T(lang, "Legal basis (GDPR): performing the service you ask for when you create an account, and our "
                    "legitimate interest in keeping fair play for moderation. No advertising, no profiling, no sale "
                    "of data.",
              "Base légale (RGPD) : l'exécution du service que vous demandez en créant un compte, et notre intérêt "
              "légitime à garantir un jeu loyal pour la modération. Pas de publicité, pas de profilage, aucune "
              "vente de données.")
         << "</p>"

         << "<h2>" << T(lang, "5. What others can see", "5. Ce que les autres voient") << "</h2><p>"
         << T(lang, "Your username, Elo, number of games, wins and badges appear on the public leaderboard and to "
                    "players you meet. Your friends see whether you are online or in a game. A small team of "
                    "moderators and administrators can see account data to moderate the service.",
              "Votre pseudo, votre Elo, votre nombre de parties, de victoires et vos badges apparaissent dans le "
              "classement public et aux joueurs que vous rencontrez. Vos amis voient si vous êtes en ligne ou en "
              "partie. Une petite équipe de modérateurs et d'administrateurs peut voir les données des comptes pour "
              "modérer le service.")
         << "</p>"

         << "<h2>" << T(lang, "6. Service providers and temporary data", "6. Prestataires et données temporaires")
         << "</h2><p>"
         << T(lang, "The server and database are hosted in France on the project's own Raspberry Pi. Connections go "
                    "through Cloudflare (HTTPS encryption, attack protection), which sees your IP address like any "
                    "network intermediary. Sign-in goes through Google or Discord, under their own privacy policies.",
              "Le serveur et la base de données sont hébergés en France sur un Raspberry Pi du projet. Les "
              "connexions passent par Cloudflare (chiffrement HTTPS, protection contre les attaques), qui voit votre "
              "adresse IP comme tout intermédiaire réseau. La connexion passe par Google ou Discord, selon leurs "
              "propres politiques de confidentialité.")
         << "</p><p>"
         << T(lang, "While you play, the server keeps in memory only: the time of your last request (to show « in "
                    "game »), the state of ongoing games and their chat (the last 50 messages, forgotten at most an "
                    "hour after the game ends). Discord Rich Presence goes directly from your game to your Discord "
                    "app. The Discord bot sees who joins the server's voice channels to create game channels; it "
                    "does not read messages. The website uses no tracking cookies and no analytics.",
              "Pendant que vous jouez, le serveur ne garde en mémoire que : l'heure de votre dernière requête (pour "
              "afficher « en jeu »), l'état des parties en cours et leur chat (les 50 derniers messages, oubliés au "
              "plus une heure après la fin de la partie). La Rich Presence Discord passe directement de votre jeu à "
              "votre application Discord. Le bot Discord voit qui rejoint les salons vocaux du serveur pour créer "
              "les salons de partie ; il ne lit pas les messages. Le site n'utilise ni cookie de suivi ni outil de "
              "mesure d'audience.")
         << "</p>"

         << "<h2>" << T(lang, "7. Retention and deletion", "7. Durée de conservation et suppression") << "</h2><p>"
         << T(lang, "Data is kept as long as your account exists. You can ask for your account to be deleted at any "
                    "time by contacting us (section 1): we erase your username, your Google and Discord identifiers, "
                    "your sessions and friend list. Your past games remain as games of an anonymous deleted player so "
                    "as not to distort other players' results.",
              "Les données sont gardées tant que votre compte existe. Vous pouvez demander la suppression de votre "
              "compte à tout moment en nous contactant (section 1) : nous effaçons votre pseudo, vos identifiants "
              "Google et Discord, vos sessions et votre liste d'amis. Vos anciennes parties restent comme parties "
              "d'un joueur supprimé anonyme pour ne pas fausser les résultats des autres.")
         << "</p>"

         << "<h2>" << T(lang, "8. Security", "8. Sécurité") << "</h2><p>"
         << T(lang, "All connections are encrypted (HTTPS). Session tokens are stored only as fingerprints, OAuth "
                    "secrets stay on the server, and the server checks every move so that no player can cheat on the "
                    "rules.",
              "Toutes les connexions sont chiffrées (HTTPS). Les jetons de session ne sont stockés que sous forme "
              "d'empreinte, les secrets OAuth restent sur le serveur, et le serveur vérifie chaque coup pour "
              "qu'aucun joueur ne puisse tricher sur les règles.")
         << "</p>"

         << "<h2>" << T(lang, "9. Your rights", "9. Vos droits") << "</h2><p>"
         << T(lang, "Under the GDPR, you can access your data, correct it (username), have it deleted, object to "
                    "its use or ask for a copy, by contacting us ",
              "Conformément au RGPD, vous pouvez accéder à vos données, les rectifier (le pseudo), les faire "
              "supprimer, vous opposer à leur utilisation ou en demander une copie, en nous contactant ")
         << contact_html(site, lang) << T(lang, ". You can also contact the ", ". Vous pouvez aussi saisir la ")
         << "<a href=\"https://www.cnil.fr\">CNIL</a>" << T(lang, " (French data protection authority).", ".")
         << "</p>"

         << "<h2>" << T(lang, "10. Age", "10. Âge") << "</h2><p>"
         << T(lang, "The online service is intended for people aged 15 or over, or younger with a parent's consent, "
                    "who meet the minimum age of Discord or Google.",
              "Le service en ligne s'adresse aux personnes d'au moins 15 ans, ou plus jeunes avec l'accord d'un "
              "parent, et qui respectent l'âge minimum de Discord ou Google.")
         << "</p>"

         << "<h2>" << T(lang, "11. Changes", "11. Modifications") << "</h2><p>"
         << T(lang, "If this policy changes, the new version is published on this page with its date. Important "
                    "changes are also announced in the game and on the Discord server.",
              "Si cette politique change, la nouvelle version est publiée sur cette page avec sa date. Les "
              "changements importants sont aussi annoncés dans le jeu et sur le serveur Discord.")
         << "</p></div>";
    return layout(site, lang, T(lang, "Privacy policy", "Politique de confidentialité"), "/privacy", body.str());
}

std::string page_not_found(const SiteInfo& site, Lang lang)
{
    return layout(site, lang, T(lang, "Page not found", "Page introuvable"), "",
                  "<h1>" + T(lang, "Page not found", "Page introuvable") + "</h1><p>" +
                      T(lang, "This page doesn't exist. ", "Cette page n'existe pas. ") + "<a href=\"" +
                      href("/", lang) + "\">" + T(lang, "Back to home", "Retour à l'accueil") + "</a>.</p>");
}
