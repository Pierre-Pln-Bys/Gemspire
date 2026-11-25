#ifndef SPLENDOR_SERVER_PAGES_HPP
#define SPLENDOR_SERVER_PAGES_HPP

#include <string>
#include <vector>

#include "../src/i18n.hpp"
#include "store.hpp"

// Pages web du site (accueil, classement, conditions, confidentialité), en
// anglais et en français. HTML et CSS simples, sans JavaScript, lisibles sur
// téléphone.

// Réglages affichés dans les pages
struct SiteInfo {
    std::string contact_email;    // SPLENDOR_CONTACT_EMAIL ("" : pas d'adresse affichée)
    std::string discord_invite;   // SPLENDOR_DISCORD_INVITE (lien d'invitation du serveur Discord)
    bool web_game = false;        // le jeu dans le navigateur est installé (/play)
    std::string public_url;       // adresse publique du site (liens absolus : canonical, partage, sitemap)
};

// Plan du site pour les moteurs de recherche (pages en anglais et en français)
std::string sitemap_xml(const SiteInfo& site);

// Langue d'une page : ?lang=fr|en, sinon l'en-tête Accept-Language du
// navigateur (français s'il commence par « fr »), sinon `fallback`
Lang page_lang(const std::string& lang_parameter, const std::string& accept_language, Lang fallback);

// Remplace < > & " ' par leurs entités HTML (pseudos, textes venant des joueurs)
std::string html_escape(const std::string& text);

std::string page_home(const SiteInfo& site, Lang lang);
// `season` : numéro de la saison classée en cours
// Classement : celui de la saison en cours (Elo de saison, au moins
// SEASON_MIN_GAMES parties) puis le classement global
std::string page_leaderboard(const SiteInfo& site, Lang lang, const std::vector<Account>& players,
                             const std::vector<Account>& season_players, const SeasonInfo& season);
std::string page_terms(const SiteInfo& site, Lang lang);
std::string page_privacy(const SiteInfo& site, Lang lang);
std::string page_not_found(const SiteInfo& site, Lang lang);

#endif
