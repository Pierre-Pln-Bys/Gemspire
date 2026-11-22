#ifndef SPLENDOR_SERVER_APP_HPP
#define SPLENDOR_SERVER_APP_HPP

#include <map>
#include <mutex>
#include <string>

#include "httplib.h"

#include "rooms.hpp"
#include "store.hpp"

// Réglages du serveur, lus dans les variables d'environnement (voir
// server/README.md). Les secrets ne sont jamais écrits dans le code.
struct ServerConfig {
    std::string host = "127.0.0.1";
    int port = 8080;
    std::string database = "splendor.db";
    std::string public_url = "http://localhost:8080";  // adresse vue par les navigateurs
    std::string discord_client_id;
    std::string discord_client_secret;
    std::string google_client_id;
    std::string google_client_secret;
    std::string discord_webhook;   // annonces de salons dans un salon Discord
    std::string bot_key;           // clé partagée avec le bot Discord (SPLENDOR_BOT_KEY)
    bool dev_login = false;        // connexion sans OAuth, pour les tests en local
    int wait_seconds = 25;         // durée maximale d'une attente longue
    int turn_seconds = 120;        // temps pour jouer son tour (SPLENDOR_TURN_SECONDS)
    std::string contact_email;     // affichée sur le site (SPLENDOR_CONTACT_EMAIL), facultative
    std::string discord_invite;    // invitation au serveur Discord (SPLENDOR_DISCORD_INVITE), facultative
};

ServerConfig config_from_environment();

// Connexion en cours : le jeu attend que le navigateur ait fini l'OAuth
struct PendingLogin {
    std::string provider;
    std::string status = "pending";  // pending, done, error
    std::string token;
    std::string error;
    long long created = 0;
};

// Le serveur : stockage, salons et parties, routes HTTP
class ServerApp {
public:
    explicit ServerApp(const ServerConfig& config);
    bool ok() const;
    void setup_routes(httplib::Server& server);
    void tick();  // chaque seconde : temps écoulés dans les parties

private:
    ServerConfig config_;
    Store store_;
    Rooms rooms_;
    std::mutex logins_mutex_;
    std::map<std::string, PendingLogin> logins_;

    int authenticate(const httplib::Request& request);
    bool bot_allowed(const httplib::Request& request) const;
    void announce_lobby(const Lobby& lobby);
    bool seat_for(int account, Seat& seat, std::string& error);
    void finish_login(const std::string& login, const std::string& provider, const std::string& provider_id,
                      const std::string& error);
};

#endif
