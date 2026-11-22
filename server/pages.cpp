#include "pages.hpp"

#include <sstream>

static const char* const RELEASES = "https://github.com/Pierre-Pln-Bys/ProjectSplendor/releases/latest";
static const char* const DOWNLOAD = "https://github.com/Pierre-Pln-Bys/ProjectSplendor/releases/latest/download/";
static const char* const REPOSITORY = "https://github.com/Pierre-Pln-Bys/ProjectSplendor";
static const char* const UPDATED = "22 novembre 2026";  // date des conditions et de la politique

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

// Les cinq jetons et l'or, dessinés en CSS (pas d'image à servir)
static std::string tokens_html()
{
    return "<div class=\"tokens\" aria-hidden=\"true\">"
           "<span class=\"t white\">●</span><span class=\"t blue\">◆</span><span class=\"t green\">▲</span>"
           "<span class=\"t gold\">✶</span>"
           "<span class=\"t red\">■</span><span class=\"t black\">⬢</span></div>";
}

// Cadre commun : en-tête, menu, pied de page
static std::string layout(const SiteInfo& site, const std::string& title, const std::string& current,
                          const std::string& body)
{
    std::ostringstream out;
    out << "<!doctype html><html lang=\"fr\"><head><meta charset=\"utf-8\">"
           "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
           "<title>"
        << html_escape(title)
        << " · Splendor</title>"
           "<meta name=\"description\" content=\"Splendor en ligne : jeu de gemmes et de nobles, 2 à 4 joueurs, "
           "contre l'ordinateur ou en ligne avec classement Elo.\">"
           "<style>"
           ":root{--felt:#0f3d28;--felt2:#165636;--panel:#12452e;--gold:#ffd140;--text:#eef3ef;--soft:#b9d2c2}"
           "*{box-sizing:border-box}"
           "body{margin:0;font-family:system-ui,-apple-system,'Segoe UI',Roboto,sans-serif;color:var(--text);"
           "background:radial-gradient(ellipse at top,var(--felt2),var(--felt) 70%) fixed;line-height:1.6}"
           "a{color:var(--gold)}"
           "header{display:flex;flex-wrap:wrap;align-items:center;justify-content:space-between;gap:8px;"
           "padding:14px 20px;max-width:960px;margin:0 auto}"
           ".brand{font-weight:800;font-size:1.4rem;color:var(--gold);text-decoration:none;letter-spacing:.04em}"
           "nav{display:flex;flex-wrap:wrap;gap:4px}"
           "nav a{color:var(--text);text-decoration:none;padding:6px 10px;border-radius:8px}"
           "nav a.on,nav a:hover{background:rgba(255,255,255,.1);color:var(--gold)}"
           "main{max-width:960px;margin:0 auto;padding:8px 20px 40px}"
           "h1{color:var(--gold);font-size:2.2rem;margin:.4em 0}"
           "h2{color:var(--gold);margin-top:1.6em}"
           ".card{background:rgba(0,0,0,.22);border:1px solid rgba(255,255,255,.08);border-radius:14px;"
           "padding:18px 20px;margin:14px 0}"
           ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:14px}"
           ".button{display:inline-block;background:var(--gold);color:#1d2a1f;font-weight:700;"
           "text-decoration:none;padding:10px 18px;border-radius:10px;margin:4px 6px 4px 0}"
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
           "</style></head><body><header><a class=\"brand\" href=\"/\">SPLENDOR</a><nav>";
    const char* links[][2] = {{"/", "Accueil"}, {"/classement", "Classement"}, {"/conditions", "Conditions"},
                              {"/confidentialite", "Confidentialité"}};
    for (const auto& link : links) {
        out << "<a href=\"" << link[0] << "\"" << (current == link[0] ? " class=\"on\"" : "") << ">" << link[1]
            << "</a>";
    }
    out << "</nav></header><main>" << body << "</main><footer>Projet étudiant (ENSEIRB-MATMECA), libre et gratuit. "
        << "Adaptation non officielle du jeu de société Splendor, sans lien avec ses éditeurs. "
        << "<a href=\"" << REPOSITORY << "\">Code source</a>";
    if (!site.discord_invite.empty()) {
        out << " · <a href=\"" << html_escape(site.discord_invite) << "\">Discord</a>";
    }
    out << "</footer></body></html>";
    return out.str();
}

// Comment nous contacter (adresse si configurée, sinon GitHub)
static std::string contact_html(const SiteInfo& site)
{
    std::string out;
    if (!site.contact_email.empty()) {
        std::string email = html_escape(site.contact_email);
        out += "par e-mail à <a href=\"mailto:" + email + "\">" + email + "</a>, ";
    }
    if (!site.discord_invite.empty()) {
        out += "sur <a href=\"" + html_escape(site.discord_invite) + "\">le serveur Discord</a>, ";
    }
    out += "ou en ouvrant une demande sur <a href=\"" + std::string(REPOSITORY) + "/issues\">GitHub</a>";
    return out;
}

std::string page_home(const SiteInfo& site)
{
    std::ostringstream body;
    body << "<section class=\"card\">" << tokens_html()
         << "<h1>Splendor</h1>"
            "<p>Collectionnez des gemmes, achetez des cartes de développement et attirez les nobles. "
            "Le premier à 15 points de prestige déclenche le dernier tour.</p>"
            "<p>Jouez <strong>seul contre l'ordinateur</strong> ou <strong>en ligne</strong> de 2 à 4 joueurs, "
            "avec un classement Elo. Connexion avec Discord ou Google, sans mot de passe.</p>"
            "<a class=\"button\" href=\""
         << DOWNLOAD << "splendor-windows.exe\">Télécharger pour Windows</a>"
         << "<a class=\"button\" href=\"" << DOWNLOAD << "splendor-linux\">Télécharger pour Linux</a>"
         << "<a class=\"button ghost\" href=\"/classement\">Voir le classement</a></section>";
    body << "<div class=\"grid\">"
            "<div class=\"card\"><h3>Deux modes</h3><p><strong>Rapide</strong> : sans or ni réservation. "
            "<strong>Classique</strong> : avec les jetons or (joker) et la réservation de cartes.</p></div>"
            "<div class=\"card\"><h3>En ligne</h3><p>Le serveur arbitre chaque coup : pas de triche possible "
            "sur les règles. 2 minutes par tour ; 3 tours manqués d'affilée valent un abandon.</p></div>"
            "<div class=\"card\"><h3>Discord</h3><p>Votre partie s'affiche sur votre profil, vos amis la "
            "rejoignent en un clic, et le salon vocal « Crée ta partie » prépare tout.</p></div></div>";
    body << "<section class=\"card\"><h2>Installer</h2><p>Un seul fichier, rien à installer : téléchargez-le "
            "et lancez-le. Sous Windows, si SmartScreen affiche un avertissement (exécutable non signé), "
            "cliquez sur <em>Informations complémentaires</em> puis <em>Exécuter quand même</em>. "
            "Sous Linux : <code>chmod +x splendor-linux</code> puis <code>./splendor-linux</code>.</p>"
            "<p>Toutes les versions (dont la version console) : <a href=\""
         << RELEASES << "\">page des versions</a>.</p></section>";
    if (!site.discord_invite.empty()) {
        body << "<section class=\"card\"><h2>Rejoindre la communauté</h2><p>Trouvez des joueurs, signalez un "
                "bug ou proposez une idée sur notre serveur Discord.</p><a class=\"button\" href=\""
             << html_escape(site.discord_invite) << "\">Rejoindre le Discord</a></section>";
    }
    return layout(site, "Accueil", "/", body.str());
}

std::string page_leaderboard(const SiteInfo& site, const std::vector<Account>& players)
{
    std::ostringstream body;
    body << "<h1>Classement</h1><p class=\"soft\">Classement Elo des parties en ligne (mis à jour à chaque "
            "partie terminée).</p><div class=\"card\">";
    if (players.empty()) {
        body << "<p>Personne n'a encore terminé de partie en ligne. Soyez le premier !</p>";
    } else {
        body << "<table><thead><tr><th>#</th><th>Joueur</th><th class=\"num\">Elo</th>"
                "<th class=\"num hide-small\">Parties</th><th class=\"num\">Victoires</th></tr></thead><tbody>";
        const char* medals[] = {"🥇", "🥈", "🥉"};
        for (size_t i = 0; i < players.size(); i++) {
            const Account& player = players[i];
            int rate = player.games > 0 ? player.wins * 100 / player.games : 0;
            body << "<tr" << (i < 3 ? " class=\"top\"" : "") << "><td>" << (i < 3 ? medals[i] : std::to_string(i + 1))
                 << "</td><td>" << html_escape(player.pseudo) << "</td><td class=\"num\"><strong>" << player.elo
                 << "</strong></td><td class=\"num hide-small\">" << player.games << "</td><td class=\"num\">"
                 << player.wins << " <span class=\"soft\">(" << rate << " %)</span></td></tr>";
        }
        body << "</tbody></table>";
    }
    body << "</div>";
    return layout(site, "Classement", "/classement", body.str());
}

std::string page_terms(const SiteInfo& site)
{
    std::ostringstream body;
    body << "<h1>Conditions d'utilisation</h1><p class=\"soft\">Dernière mise à jour : " << UPDATED << "</p>"
         << "<div class=\"card\">"
            "<h2>1. Le service</h2><p>Splendor est un projet étudiant, gratuit et libre : un jeu (Windows, Linux), "
            "un serveur de parties en ligne, ce site et un bot Discord. C'est une adaptation non officielle du jeu "
            "de société Splendor, sans lien avec ses auteurs ni ses éditeurs.</p>"
            "<h2>2. Compte</h2><p>On se connecte avec un compte Discord ou Google ; aucun mot de passe n'est "
            "créé ni stocké. Vous choisissez un pseudo visible par les autres joueurs et dans le classement. "
            "Il ne doit pas être injurieux, discriminatoire, ni usurper l'identité de quelqu'un.</p>"
            "<h2>3. Règles de bonne conduite</h2><p>Sont interdits : la triche (programme qui joue à votre "
            "place, entente entre joueurs, plusieurs comptes pour gagner des points), l'abandon volontaire pour "
            "éviter une défaite, les attaques contre le serveur, le harcèlement sur le serveur Discord.</p>"
            "<h2>4. Temps de jeu et abandon</h2><p>En ligne, chaque joueur dispose de 2 minutes par tour. Passé "
            "ce délai, son tour passe. Après 3 tours manqués d'affilée, ou s'il abandonne, il est classé dernier "
            "et perd des points Elo ; la partie continue entre les autres joueurs.</p>"
            "<h2>5. Sanctions</h2><p>En cas de manquement, les modérateurs peuvent annuler les points d'une "
            "partie, suspendre ou supprimer un compte, et exclure du serveur Discord.</p>"
            "<h2>6. Disponibilité</h2><p>Le service est fourni tel quel, sans garantie : il peut être "
            "interrompu, modifié ou arrêté à tout moment (le serveur est hébergé sur un Raspberry Pi). Les "
            "classements peuvent être remis à zéro.</p>"
            "<h2>7. Données personnelles</h2><p>Voir la <a href=\"/confidentialite\">politique de "
            "confidentialité</a>.</p>"
            "<h2>8. Contact</h2><p>Pour toute question : "
         << contact_html(site) << ".</p></div>";
    return layout(site, "Conditions d'utilisation", "/conditions", body.str());
}

std::string page_privacy(const SiteInfo& site)
{
    std::ostringstream body;
    body << "<h1>Confidentialité</h1><p class=\"soft\">Dernière mise à jour : " << UPDATED << "</p>"
         << "<div class=\"card\">"
            "<h2>Ce que nous enregistrons</h2><ul>"
            "<li><strong>Votre identifiant Discord ou Google</strong> (un numéro, pas votre nom ni votre e-mail), "
            "pour reconnaître votre compte. Nous demandons à Discord le droit <code>identify</code> et à Google "
            "<code>openid</code> uniquement.</li>"
            "<li><strong>Votre pseudo</strong>, votre <strong>Elo</strong>, votre nombre de parties et de "
            "victoires, et l'historique des parties en ligne (date, mode, place, score, Elo).</li>"
            "<li><strong>Une empreinte de votre jeton de session</strong> (le jeton lui-même reste sur votre "
            "ordinateur, dans <code>session.txt</code>).</li></ul>"
            "<h2>Ce que nous n'enregistrons pas</h2><p>Ni adresse e-mail, ni mot de passe, ni photo, ni liste "
            "d'amis, ni messages. Le jeu n'affiche pas de publicité et ne revend aucune donnée.</p>"
            "<h2>Données temporaires</h2><p>Pendant que vous jouez, le serveur garde en mémoire l'heure de votre "
            "dernière connexion (pour afficher « en jeu » au bot Discord) et l'état des parties en cours. La Rich "
            "Presence Discord passe directement de votre jeu à votre application Discord.</p>"
            "<h2>Bot Discord</h2><p>Le bot voit qui rejoint les salons vocaux du serveur pour créer les salons "
            "« Partie de … », et garde la liste des salons qu'il a créés. Il ne lit pas les messages.</p>"
            "<h2>Hébergement</h2><p>Le serveur et la base de données sont hébergés en France sur un Raspberry Pi "
            "du projet. Les connexions passent par Cloudflare (chiffrement HTTPS, protection contre les "
            "attaques), qui voit votre adresse IP comme tout intermédiaire réseau.</p>"
            "<h2>Durée et suppression</h2><p>Les données sont gardées tant que le compte existe. Vous pouvez "
            "demander la suppression de votre compte à tout moment : nous effaçons votre compte et vos sessions ; "
            "vos anciennes parties restent comme parties d'un joueur anonyme pour ne pas fausser le classement "
            "des autres.</p>"
            "<h2>Vos droits</h2><p>Conformément au RGPD, vous pouvez accéder à vos données, les rectifier (le "
            "pseudo) ou les faire supprimer, en nous contactant "
         << contact_html(site)
         << ". Vous pouvez aussi saisir la <a href=\"https://www.cnil.fr\">CNIL</a>.</p>"
            "<h2>Âge</h2><p>Le service s'adresse aux personnes d'au moins 15 ans, ou plus jeunes avec l'accord "
            "d'un parent, et qui respectent l'âge minimum de Discord ou Google.</p></div>";
    return layout(site, "Confidentialité", "/confidentialite", body.str());
}

std::string page_not_found(const SiteInfo& site)
{
    return layout(site, "Page introuvable", "",
                  "<h1>Page introuvable</h1><p>Cette page n'existe pas. <a href=\"/\">Retour à l'accueil</a>.</p>");
}
