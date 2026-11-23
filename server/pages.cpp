#include "pages.hpp"

#include <sstream>

static const char* const RELEASES = "https://github.com/Pierre-Pln-Bys/Gemspire/releases/latest";
static const char* const DOWNLOAD = "https://github.com/Pierre-Pln-Bys/Gemspire/releases/latest/download/";
static const char* const REPOSITORY = "https://github.com/Pierre-Pln-Bys/Gemspire";

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
        << T(lang, "Student project (ENSEIRB-MATMECA), free and open source (GPL-3.0). Independent game, not "
                   "affiliated with any board game publisher. ",
             "Projet étudiant (ENSEIRB-MATMECA), libre et gratuit (GPL-3.0). Jeu indépendant, sans lien avec un "
             "éditeur de jeux de société. ")
        << "<a href=\"" << REPOSITORY << "\">" << T(lang, "Source code", "Code source") << "</a>";
    if (!site.discord_invite.empty()) {
        out << " · <a href=\"" << html_escape(site.discord_invite) << "\">Discord</a>";
    }
    out << "</footer></body></html>";
    return out.str();
}

// Comment nous contacter (adresse si configurée, Discord, sinon GitHub)
static std::string contact_html(const SiteInfo& site, Lang lang)
{
    std::string out;
    if (!site.contact_email.empty()) {
        std::string email = html_escape(site.contact_email);
        out += T(lang, "by email at ", "par e-mail à ") + "<a href=\"mailto:" + email + "\">" + email + "</a>, ";
    }
    if (!site.discord_invite.empty()) {
        out += T(lang, "on ", "sur ") + "<a href=\"" + html_escape(site.discord_invite) + "\">"
               + T(lang, "the Discord server", "le serveur Discord") + "</a>, ";
    }
    out += T(lang, "or by opening an issue on ", "ou en ouvrant une demande sur ") + "<a href=\"" +
           std::string(REPOSITORY) + "/issues\">GitHub</a>";
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

std::string page_leaderboard(const SiteInfo& site, Lang lang, const std::vector<Account>& players, int season)
{
    std::ostringstream body;
    body << "<h1>" << T(lang, "Leaderboard", "Classement") << "</h1><p class=\"soft\">"
         << T(lang, "Season ", "Saison ") << season << " · "
         << T(lang, "Elo ranking of online games (updated after every finished game).",
              "Classement Elo des parties en ligne (mis à jour à chaque partie terminée).")
         << "</p><div class=\"card\">";
    if (players.empty()) {
        body << "<p>"
             << T(lang, "Nobody has finished an online game yet. Be the first!",
                  "Personne n'a encore terminé de partie en ligne. Soyez le premier !")
             << "</p>";
    } else {
        body << "<table><thead><tr><th>#</th><th>" << T(lang, "Player", "Joueur") << "</th><th class=\"num\">Elo</th>"
             << "<th class=\"num hide-small\">" << T(lang, "Games", "Parties") << "</th><th class=\"num\">"
             << T(lang, "Wins", "Victoires") << "</th></tr></thead><tbody>";
        const char* medals[] = {"🥇", "🥈", "🥉"};
        for (size_t i = 0; i < players.size(); i++) {
            const Account& player = players[i];
            int rate = player.games > 0 ? player.wins * 100 / player.games : 0;
            body << "<tr" << (i < 3 ? " class=\"top\"" : "") << "><td>" << (i < 3 ? medals[i] : std::to_string(i + 1))
                 << "</td><td>" << html_escape(player.pseudo) << "</td><td class=\"num\"><strong>" << player.elo
                 << "</strong></td><td class=\"num hide-small\">" << player.games << "</td><td class=\"num\">"
                 << player.wins << " <span class=\"soft\">(" << rate << T(lang, "%", " %") << ")</span></td></tr>";
        }
        body << "</tbody></table>";
    }
    body << "</div>";
    return layout(site, lang, T(lang, "Leaderboard", "Classement"), "/leaderboard", body.str());
}

std::string page_terms(const SiteInfo& site, Lang lang)
{
    std::ostringstream body;
    body << "<h1>" << T(lang, "Terms of use", "Conditions d'utilisation") << "</h1><p class=\"soft\">"
         << T(lang, "Last updated: November 23, 2026", "Dernière mise à jour : 23 novembre 2026")
         << "</p><div class=\"card\"><h2>" << T(lang, "1. The service", "1. Le service") << "</h2><p>"
         << T(lang, "Gemspire is a free, open-source student project: a game (Windows, Linux, browser), an online "
                    "game server, this website and a Discord bot. It is an independent project, not affiliated with "
                    "any board game author or publisher.",
              "Gemspire est un projet étudiant, gratuit et libre : un jeu (Windows, Linux, navigateur), un serveur "
              "de parties en ligne, ce site et un bot Discord. C'est un projet indépendant, sans lien avec un auteur "
              "ou un éditeur de jeux de société.")
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
    body << "<h1>" << T(lang, "Privacy", "Confidentialité") << "</h1><p class=\"soft\">"
         << T(lang, "Last updated: November 22, 2026", "Dernière mise à jour : 22 novembre 2026")
         << "</p><div class=\"card\"><h2>" << T(lang, "What we store", "Ce que nous enregistrons") << "</h2><ul><li>"
         << T(lang, "<strong>Your Discord and/or Google identifier</strong> (one per linked sign-in, a number, not your name or email), to "
                    "recognize your account. We only ask Discord for <code>identify</code> and Google for "
                    "<code>openid</code>.",
              "<strong>Votre identifiant Discord et/ou Google</strong> (un par connexion liée, un numéro, pas votre nom ni votre e-mail), pour "
              "reconnaître votre compte. Nous demandons à Discord le droit <code>identify</code> et à Google "
              "<code>openid</code> uniquement.")
         << "</li><li>"
         << T(lang, "<strong>Your username</strong>, your <strong>Elo</strong>, your number of games and wins, and "
                    "the history of your online games (date, mode, place, score, Elo).",
              "<strong>Votre pseudo</strong>, votre <strong>Elo</strong>, votre nombre de parties et de victoires, et "
              "l'historique des parties en ligne (date, mode, place, score, Elo).")
         << "</li><li>"
         << T(lang, "<strong>A fingerprint of your session token</strong> (the token itself stays on your device).",
              "<strong>Une empreinte de votre jeton de session</strong> (le jeton lui-même reste sur votre "
              "appareil).")
         << "</li><li>"
         << T(lang, "<strong>Your friend list</strong> (requests you sent or accepted) and the <strong>achievements"
                    "</strong> you unlocked.",
              "<strong>Votre liste d'amis</strong> (demandes envoyées ou acceptées) et les <strong>succès</strong> "
              "que vous avez débloqués.")
         << "</li><li>"
         << T(lang, "<strong>Reports</strong> sent with <code>/report</code> on Discord (who reported, the username "
                    "and the reason), kept for moderation.",
              "<strong>Les signalements</strong> envoyés avec <code>/report</code> sur Discord (qui signale, le pseudo "
              "et le motif), gardés pour la modération.")
         << "</li></ul><h2>" << T(lang, "What we don't store", "Ce que nous n'enregistrons pas") << "</h2><p>"
         << T(lang, "No email address, password or picture, and no chat message in the database. The game shows no "
                    "ads and sells no data.",
              "Ni adresse e-mail, ni mot de passe, ni photo, et aucun message du chat dans la base. Le jeu n'affiche "
              "pas de publicité et ne revend aucune donnée.")
         << "</p><h2>" << T(lang, "Temporary data", "Données temporaires") << "</h2><p>"
         << T(lang, "While you play, the server keeps in memory the time of your last connection (to show « in "
                    "game » to the Discord bot and to your friends), the state of ongoing games and their chat (the last 50 "
                    "messages, forgotten at most an hour after the game ends). Discord Rich Presence goes directly "
                    "from your game to your Discord app.",
              "Pendant que vous jouez, le serveur garde en mémoire l'heure de votre dernière connexion (pour "
              "afficher « en jeu » au bot Discord et à vos amis), l'état des parties en cours et leur chat (les 50 "
              "derniers messages, oubliés au plus une heure après la fin de la partie). La Rich Presence Discord passe "
              "directement de votre jeu à votre application Discord.")
         << "</p><h2>" << T(lang, "Discord bot", "Bot Discord") << "</h2><p>"
         << T(lang, "The bot sees who joins the server's voice channels to create game channels, and keeps the list "
                    "of channels it created. It does not read messages.",
              "Le bot voit qui rejoint les salons vocaux du serveur pour créer les salons de partie, et garde la "
              "liste des salons qu'il a créés. Il ne lit pas les messages.")
         << "</p><h2>" << T(lang, "Hosting", "Hébergement") << "</h2><p>"
         << T(lang, "The server and database are hosted in France on the project's Raspberry Pi. Connections go "
                    "through Cloudflare (HTTPS encryption, attack protection), which sees your IP address like any "
                    "network intermediary.",
              "Le serveur et la base de données sont hébergés en France sur un Raspberry Pi du projet. Les "
              "connexions passent par Cloudflare (chiffrement HTTPS, protection contre les attaques), qui voit votre "
              "adresse IP comme tout intermédiaire réseau.")
         << "</p><h2>" << T(lang, "Retention and deletion", "Durée et suppression") << "</h2><p>"
         << T(lang, "Data is kept as long as the account exists. You can ask for your account to be deleted at any "
                    "time: we erase your account and sessions; your past games remain as games of an anonymous "
                    "player so as not to distort other players' rankings.",
              "Les données sont gardées tant que le compte existe. Vous pouvez demander la suppression de votre "
              "compte à tout moment : nous effaçons votre compte et vos sessions ; vos anciennes parties restent "
              "comme parties d'un joueur anonyme pour ne pas fausser le classement des autres.")
         << "</p><h2>" << T(lang, "Your rights", "Vos droits") << "</h2><p>"
         << T(lang, "Under the GDPR, you can access your data, correct it (username) or have it deleted by "
                    "contacting us ",
              "Conformément au RGPD, vous pouvez accéder à vos données, les rectifier (le pseudo) ou les faire "
              "supprimer, en nous contactant ")
         << contact_html(site, lang) << T(lang, ". You can also contact the ", ". Vous pouvez aussi saisir la ")
         << "<a href=\"https://www.cnil.fr\">CNIL</a>" << T(lang, " (French data protection authority).", ".")
         << "</p><h2>" << T(lang, "Age", "Âge") << "</h2><p>"
         << T(lang, "The service is intended for people aged 15 or over, or younger with a parent's consent, who meet "
                    "the minimum age of Discord or Google.",
              "Le service s'adresse aux personnes d'au moins 15 ans, ou plus jeunes avec l'accord d'un parent, et "
              "qui respectent l'âge minimum de Discord ou Google.")
         << "</p></div>";
    return layout(site, lang, T(lang, "Privacy", "Confidentialité"), "/privacy", body.str());
}

std::string page_not_found(const SiteInfo& site, Lang lang)
{
    return layout(site, lang, T(lang, "Page not found", "Page introuvable"), "",
                  "<h1>" + T(lang, "Page not found", "Page introuvable") + "</h1><p>" +
                      T(lang, "This page doesn't exist. ", "Cette page n'existe pas. ") + "<a href=\"" +
                      href("/", lang) + "\">" + T(lang, "Back to home", "Retour à l'accueil") + "</a>.</p>");
}
