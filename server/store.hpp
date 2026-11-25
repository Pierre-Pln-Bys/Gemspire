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
    int elo = 1000;            // Elo global (parties « Classé »), jamais remis à zéro
    int games = 0;             // toutes les parties en ligne (amicales comprises)
    int wins = 0;
    int ranked_games = 0;      // parties « Classé » (classement global)
    int ranked_wins = 0;
    int season_elo = 1000;     // Elo de la saison en cours (parties « Saison »)
    int season_games = 0;
    int season_wins = 0;
    long long banned_until = 0;   // 0 : pas suspendu ; BAN_FOREVER : définitif ; sinon date de fin
    std::string ban_reason;

    bool banned() const;          // suspendu en ce moment
};

const long long BAN_FOREVER = -1;

// Type d'une partie en ligne : amicale (salons, défis : pas d'Elo), classée
// (recherche de partie, Elo global) ou de saison (recherche de partie, Elo de saison)
enum class GameKind { Friendly, Ranked, Season };

std::string kind_name(GameKind kind);                 // "friendly", "ranked", "season"
bool parse_kind(const std::string& text, GameKind& kind);

// Une partie de l'historique d'un joueur (modération)
struct HistoryEntry {
    int game_id;                        // numéro dans la base (pour l'annuler)
    std::string mode;
    std::string kind;                   // "friendly", "ranked" ou "season"
    long long finished_at;
    int place;
    int score;
    int elo_change;
    bool cancelled;
    bool has_replay;                    // rediffusion enregistrée (parties depuis la v3.9)
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

// Saisons classées (v4.1) : une saison dure deux mois et change toute seule
// le 1er janvier, mars, mai, juillet, septembre et novembre à minuit (heure de
// Paris). À la fin, les SEASON_ARCHIVED premiers du classement de la saison
// (au moins SEASON_MIN_GAMES parties) sont archivés, les SEASON_BADGES premiers
// ont un badge, et l'Elo de saison de chacun repart à 1000. L'Elo global ne
// change pas.
const int SEASON_ARCHIVED = 50;
const int SEASON_BADGES = 10;
const int SEASON_MIN_GAMES = 10;

// Prochain changement de saison strictement après `now` (secondes depuis 1970)
long long next_season_change(long long now);

struct SeasonInfo {
    int number = 0;
    long long started_at = 0;
    long long ends_at = 0;     // changement automatique prévu
};

struct SeasonBadge {
    int season;
    int rank;     // 1 = premier
};

// Une action d'administration (panel admin ou commande /dev du bot)
struct AdminLogEntry {
    long long at;
    std::string actor;    // « discord:ID » de l'administrateur
    std::string action;   // ex. « elo Pierre 1200 »
};

// Un signalement envoyé avec /report sur Discord
struct Report {
    int id;
    long long at;
    std::string reporter;   // membre Discord qui signale (« nom (identifiant) »)
    std::string pseudo;     // joueur signalé (tel qu'écrit)
    std::string reason;
    bool handled;
};

// Chiffres du tableau de bord d'administration
struct StoreStats {
    int accounts = 0;
    int games = 0;          // parties terminées (non annulées)
    int games_today = 0;    // ces dernières 24 heures
    int banned = 0;
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
    // --- Identités (v4) : un compte peut avoir une connexion Discord ET une Google
    // Ajoute la connexion `provider` au compte. "" ou l'erreur.
    std::string link_identity(int id, const std::string& provider, const std::string& provider_id);
    std::vector<std::string> providers(int id);                      // ex. {"discord", "google"}
    std::string identity(int id, const std::string& provider);       // identifiant chez ce fournisseur ("" : aucun)
    // Compte associé à cette identité, sans le créer (0 si aucun)
    int find_account(const std::string& provider, const std::string& provider_id);
    bool get_account(int id, Account& account);
    // Choisit le pseudo (unique, 3 à 16 caractères). Retourne "" ou l'erreur.
    std::string set_pseudo(int id, const std::string& pseudo);

    // Sessions : un jeton aléatoire ; seule son empreinte SHA-256 est stockée
    std::string create_session(int account);
    int account_for_token(const std::string& token);  // 0 si inconnu
    void delete_session(const std::string& token);

    // Enregistre une partie terminée et met à jour parties, victoires et l'Elo
    // de son type (aucun pour une amicale) ; `elo_before` / `elo_after` des
    // résultats sont cet Elo-là. `replay` : de quoi la rejouer (JSON : graine,
    // noms, actions). Renvoie son numéro.
    int record_game(Mode mode, GameKind kind, const std::vector<GameResult>& results, const std::string& replay = "");
    // Rediffusion d'une partie ("" si aucune)
    std::string replay(int game_id);
    // Classement global (au moins une partie « Classé », sans les comptes suspendus)
    std::vector<Account> leaderboard(int limit);
    int rank_of(int id);                         // place au classement global (0 : pas classé)
    // Classement de la saison en cours (au moins SEASON_MIN_GAMES parties « Saison »)
    std::vector<Account> season_leaderboard(int limit);
    int season_rank_of(int id);                  // place au classement de la saison (0 : pas classé)

    // --- Amis : une demande devient une amitié quand l'autre l'accepte (ou
    //     demande aussi). "" ou l'erreur.
    std::string add_friend(int id, int other);
    void remove_friend(int id, int other);   // retire l'amitié ou la demande, dans les deux sens
    std::vector<Friend> friends(int id);

    // --- Succès : true si `code` est nouveau pour ce compte (il est alors enregistré)
    bool unlock_achievement(int id, const std::string& code);
    std::vector<std::string> achievements(int id);

    // --- Saisons
    int current_season();                       // numéro de la saison en cours (la 1 est créée au besoin)
    SeasonInfo season_info();                   // saison en cours, avec sa date de fin prévue
    // Termine la saison : archive son classement, remet les Elo de saison à
    // 1000, commence la suivante (fin au prochain changement du calendrier).
    // Renvoie le classement archivé (les premiers).
    std::vector<Account> end_season();
    std::vector<SeasonBadge> badges(int id);    // places dans le top 10 des saisons passées

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
    // --- Administration (panel admin, commandes /dev)
    std::string setting(const std::string& key);                     // "" si absent
    void set_setting(const std::string& key, const std::string& value);
    void log_admin(const std::string& actor, const std::string& action);
    std::vector<AdminLogEntry> admin_log(int limit);
    void set_elo(int id, int elo);
    // Badge de saison donné à la main (rang 1 à SEASON_BADGES) ou retiré
    void set_badge(int id, int season, int rank);
    void remove_badge(int id, int season);
    void close_sessions(int id);                                     // déconnecte le compte partout
    // Comptes dont le pseudo contient `text`, ou de numéro / identifiant Discord `text`
    std::vector<Account> search_accounts(const std::string& text, int limit);
    StoreStats stats();
    // Parties terminées (non annulées) chacun des `days` derniers jours (UTC), du plus ancien à aujourd'hui
    std::vector<int> games_per_day(int days);
    // Signalements (/report du bot)
    void add_report(const std::string& reporter, const std::string& pseudo, const std::string& reason);
    std::vector<Report> reports(int limit);       // non traités d'abord, puis les plus récents
    bool handle_report(int id);                    // marque comme traité (false s'il n'existe pas)

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
