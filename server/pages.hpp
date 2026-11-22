#ifndef SPLENDOR_SERVER_PAGES_HPP
#define SPLENDOR_SERVER_PAGES_HPP

#include <string>
#include <vector>

#include "store.hpp"

// Pages web du site (accueil, classement, conditions, confidentialité).
// HTML et CSS simples, sans JavaScript, lisibles sur téléphone.

// Réglages affichés dans les pages
struct SiteInfo {
    std::string contact_email;    // SPLENDOR_CONTACT_EMAIL ("" : pas d'adresse affichée)
    std::string discord_invite;   // SPLENDOR_DISCORD_INVITE (lien d'invitation du serveur Discord)
};

// Remplace < > & " ' par leurs entités HTML (pseudos, textes venant des joueurs)
std::string html_escape(const std::string& text);

std::string page_home(const SiteInfo& site);
std::string page_leaderboard(const SiteInfo& site, const std::vector<Account>& players);
std::string page_terms(const SiteInfo& site);
std::string page_privacy(const SiteInfo& site);
std::string page_not_found(const SiteInfo& site);

#endif
