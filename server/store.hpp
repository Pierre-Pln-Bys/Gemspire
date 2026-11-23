#ifndef SPLENDOR_SERVER_STORE_HPP
#define SPLENDOR_SERVER_STORE_HPP

#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "../src/game.hpp"

struct sqlite3;

// Un compte joueur. Pas de mot de passe : on se connecte avec Google ou
// Discord (provider + identifiant chez ce fournisseur).
struct Account {
    int id = 0;
    std::string provider;     // "discord", "google" (ou "dev" en local)
    std::string pseudo;       // vide tant que le joueur ne l'a pas choisi
    int elo = 1000;
    int games = 0;
    int wins = 0;
    long long banned_until = 0;   // 0 : pas suspendu ; BAN_FOREVER : définitif ; sinon date de fin
    std::string ban_reason;

    bool banned() const;          // suspendu en ce moment
};

const long long BAN_FOREVER = -1;

// Une partie de l'historique d'un joueur (modération)
struct HistoryEntry {
    int game_id;                        // numéro dans la base (pour l'annuler)
    std::string mode;
    long long finished_at;
    int place;
    int score;
    int elo_change;
    bool cancelled;
    std::vector<std::string> players;   // pseudos de tous les joueurs, dans l'ordre des places
};

// Résultat d'un joueur dans une partie en ligne terminée
struct GameResult {
    int account;
    int place;        // 0 = premier
    int score;
    int elo_before;
    int elo_after;
};

// Un lien d'amitié vu par un joueur
struct Friend {
    int account;
    std::string pseudo;
    int elo;
    std::string relation;   // "friend" (accepté des deux côtés), "incoming" (demande reçue), "outgoing" (envoyée)
};

const int MAX_FRIENDS = 100;

// Stockage SQLite : comptes, sessions, historique des parties.
// Toutes les méthodes sont protégées par un verrou (serveur multi-thread).
class Store {
public:
    explicit Store(const std::string& path);
    ~Store();
    Store(const Store&) = delete;
    Store& operator=(const Store&) = delete;

    bool ok() const;  // la base est ouverte

    // Compte associé à cette identité ; il est créé s'il n'existe pas
    int find_or_create_account(const std::string& provider, const std::string& provider_id);
    // Compte associé à cette identité, sans le créer (0 si aucun)
    int find_account(const std::string& provider, const std::string& provider_id);
    bool get_account(int id, Account& account);
    // Choisit le pseudo (unique, 3 à 16 caractères). Retourne "" ou l'erreur.
    std::string set_pseudo(int id, const std::string& pseudo);

    // Sessions : un jeton aléatoire ; seule son empreinte SHA-256 est stockée
    std::string create_session(int account);
    int account_for_token(const std::string& token);  // 0 si inconnu
    void delete_session(const std::string& token);

    // Enregistre une partie terminée et met à jour Elo, parties et victoires
    void record_game(Mode mode, const std::vector<GameResult>& results);
    // Classement (sans les comptes suspendus)
    std::vector<Account> leaderboard(int limit);
    int rank_of(int id);                         // place au classement (0 : pas classé)

    // --- Amis : une demande devient une amitié quand l'autre l'accepte (ou
    //     demande aussi). "" ou l'erreur.
    std::string add_friend(int id, int other);
    void remove_friend(int id, int other);   // retire l'amitié ou la demande, dans les deux sens
    std::vector<Friend> friends(int id);

    // --- Succès : true si `code` est nouveau pour ce compte (il est alors enregistré)
    bool unlock_achievement(int id, const std::string& code);
    std::vector<std::string> achievements(int id);

    // Comptes Discord classés (au moins une partie, non suspendus) :
    // identifiant Discord et Elo, pour les rôles de rang du bot
    std::vector<std::pair<std::string, int>> discord_ranks();

    // --- Modération (commandes du bot Discord)
    int find_by_pseudo(const std::string& pseudo);  // 0 si aucun (sans casse)
    // Suspend jusqu'à `until` (BAN_FOREVER : définitif) et ferme ses sessions
    void ban(int id, long long until, const std::string& reason);
    void unban(int id);
    std::vector<HistoryEntry> history(int id, int limit);
    // Annule une partie : rend l'Elo, les parties et victoires. "" ou l'erreur.
    std::string cancel_game(int game_id);
    // Supprime le compte (RGPD) : pseudo et identité effacés, sessions fermées ;
    // ses anciennes parties restent, au nom d'un joueur supprimé
    void delete_account(int id);

private:
    sqlite3* db_ = nullptr;
    std::mutex mutex_;

    bool exec(const std::string& sql);
};

// Vérifie un pseudo : 3 à 16 caractères, lettres (accents compris), chiffres,
// espace, tiret, souligné ; pas d'espace au début ni à la fin.
std::string check_pseudo(const std::string& pseudo);

// Chaîne aléatoire (hexadécimal) de `bytes` octets, tirée par OpenSSL
std::string random_hex(int bytes);

#endif
