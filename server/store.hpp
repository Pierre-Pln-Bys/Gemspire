#ifndef SPLENDOR_SERVER_STORE_HPP
#define SPLENDOR_SERVER_STORE_HPP

#include <mutex>
#include <string>
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
};

// Résultat d'un joueur dans une partie en ligne terminée
struct GameResult {
    int account;
    int place;        // 0 = premier
    int score;
    int elo_before;
    int elo_after;
};

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
    std::vector<Account> leaderboard(int limit);

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
