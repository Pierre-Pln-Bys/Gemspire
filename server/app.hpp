#ifndef SPLENDOR_SERVER_APP_HPP
#define SPLENDOR_SERVER_APP_HPP

#include <map>
#include <mutex>
#include <string>
#include <vector>

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
    std::string web_dir = "/opt/splendor/web";  // jeu dans le navigateur (make web), servi sur /play
    std::string min_version = "0.0.0";  // version minimale du jeu (SPLENDOR_MIN_VERSION, défaut : celle du serveur)
    std::string contact_email;     // affichée sur le site (SPLENDOR_CONTACT_EMAIL), facultative
    std::string discord_invite;    // invitation au serveur Discord (SPLENDOR_DISCORD_INVITE), facultative
    // Comptes Discord administrateurs (SPLENDOR_ADMIN_DISCORD_IDS, séparés par des virgules) :
    // panel /panel-admin et commandes /dev du bot. Vide : administration fermée.
    std::vector<std::string> admin_discord_ids;
};

ServerConfig config_from_environment();

// Connexion en cours : le jeu attend que le navigateur ait fini l'OAuth
struct PendingLogin {
    std::string provider;
    std::string status = "pending";  // pending, done, error
    std::string token;
    std::string error;
    // Secret détenu par le jeu, jamais transmis dans l'URL du navigateur : il
    // faut le présenter à /auth/poll pour récupérer le jeton (schéma type PKCE).
    std::string verifier;
    long long created = 0;
    bool back_to_play = false;       // commencée depuis le jeu dans le navigateur : y revenir après
    int link_account = 0;            // v4 : ajoute cette connexion à ce compte au lieu de se connecter
    bool admin = false;              // connexion au panel d'administration
};

// Session du panel d'administration (en mémoire : un redémarrage déconnecte)
struct AdminSession {
    std::string discord_id;
    std::string csrf;                // jeton à renvoyer dans chaque formulaire
    long long expires = 0;
    std::string flash;               // résultat de la dernière action, affiché une fois
};

// Paramètres d'une action d'administration (formulaire du panel ou JSON du bot)
using AdminParams = std::map<std::string, std::string>;

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
    std::map<std::string, AdminSession> admin_sessions_;  // protégé par logins_mutex_
    long long started_at_ = 0;

    int authenticate(const httplib::Request& request);
    bool bot_allowed(const httplib::Request& request) const;
    void announce_lobby(const Lobby& lobby);
    bool seat_for(int account, Seat& seat, std::string& error);
    void finish_login(const std::string& login, const std::string& provider, const std::string& provider_id,
                      const std::string& error);

    // --- Administration (admin.cpp)
    void setup_admin_routes(httplib::Server& server);
    bool is_admin(const std::string& discord_id) const;
    // Ouvre une session du panel ; renvoie l'en-tête Set-Cookie à envoyer
    std::string open_admin_session(const std::string& discord_id);
    // Session du panel de cette requête (false si absente ou expirée)
    bool admin_session(const httplib::Request& request, AdminSession& session, std::string& token);
    // Applique une action ; "" ou l'erreur. `result` : {"message": ...} ou l'état (action "status")
    std::string admin_action(const std::string& actor, const AdminParams& params, nlohmann::json& result);
    nlohmann::json admin_status();
    bool signups_closed();
    // Version minimale du jeu : réglage du panel, sinon SPLENDOR_MIN_VERSION
    std::string min_version();
    bool games_paused();
};

#endif
