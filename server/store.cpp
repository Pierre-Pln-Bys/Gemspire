#include "store.hpp"

#include <ctime>

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <sqlite3.h>

// ---------------------------------------------------------------- Outils

std::string random_hex(int bytes)
{
    std::vector<unsigned char> data(bytes);
    RAND_bytes(data.data(), bytes);
    const char* digits = "0123456789abcdef";
    std::string hex;
    for (unsigned char byte : data) {
        hex += digits[byte >> 4];
        hex += digits[byte & 15];
    }
    return hex;
}

// Empreinte SHA-256 (hexadécimal) : les jetons ne sont jamais stockés en clair
static std::string sha256_hex(const std::string& text)
{
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int length = 0;
    EVP_Digest(text.data(), text.size(), digest, &length, EVP_sha256(), nullptr);
    const char* digits = "0123456789abcdef";
    std::string hex;
    for (unsigned int i = 0; i < length; i++) {
        hex += digits[digest[i] >> 4];
        hex += digits[digest[i] & 15];
    }
    return hex;
}

std::string check_pseudo(const std::string& pseudo)
{
    int characters = 0;
    for (unsigned char c : pseudo) {
        if ((c & 0xC0) != 0x80) {
            characters++;
        }
        bool ascii_ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'
                        || c == '-' || c == ' ';
        if (c < 0x80 && !ascii_ok) {
            return "The username can only contain letters, digits, spaces, - and _.";
        }
    }
    if (characters < 3 || characters > 16) {
        return "The username must be 3 to 16 characters long.";
    }
    if (pseudo.front() == ' ' || pseudo.back() == ' ') {
        return "The username can't start or end with a space.";
    }
    return "";
}

// Petite aide pour les requêtes préparées
struct Statement {
    sqlite3_stmt* stmt = nullptr;

    Statement(sqlite3* db, const char* sql) { sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr); }
    ~Statement() { sqlite3_finalize(stmt); }
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    void bind(int index, const std::string& value) { sqlite3_bind_text(stmt, index, value.c_str(), -1, SQLITE_TRANSIENT); }
    void bind(int index, long long value) { sqlite3_bind_int64(stmt, index, value); }
    bool row() { return sqlite3_step(stmt) == SQLITE_ROW; }
    bool done() { return sqlite3_step(stmt) == SQLITE_DONE; }
    int integer(int column) { return sqlite3_column_int(stmt, column); }
    long long big(int column) { return sqlite3_column_int64(stmt, column); }
    std::string text(int column)
    {
        const unsigned char* value = sqlite3_column_text(stmt, column);
        return value == nullptr ? "" : reinterpret_cast<const char*>(value);
    }
};

// ---------------------------------------------------------------- Store

Store::Store(const std::string& path)
{
    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
        sqlite3_close(db_);
        db_ = nullptr;
        return;
    }
    exec("PRAGMA journal_mode=WAL;");
    exec("PRAGMA foreign_keys=ON;");
    exec("CREATE TABLE IF NOT EXISTS accounts ("
         " id INTEGER PRIMARY KEY,"
         " provider TEXT NOT NULL,"
         " provider_id TEXT NOT NULL,"
         " pseudo TEXT UNIQUE COLLATE NOCASE,"
         " elo INTEGER NOT NULL DEFAULT 1000,"
         " games INTEGER NOT NULL DEFAULT 0,"
         " wins INTEGER NOT NULL DEFAULT 0,"
         " created_at INTEGER NOT NULL,"
         " UNIQUE(provider, provider_id));");
    exec("CREATE TABLE IF NOT EXISTS sessions ("
         " token_hash TEXT PRIMARY KEY,"
         " account_id INTEGER NOT NULL REFERENCES accounts(id),"
         " created_at INTEGER NOT NULL);");
    exec("CREATE TABLE IF NOT EXISTS games ("
         " id INTEGER PRIMARY KEY,"
         " mode TEXT NOT NULL,"
         " finished_at INTEGER NOT NULL);");
    exec("CREATE TABLE IF NOT EXISTS game_players ("
         " game_id INTEGER NOT NULL REFERENCES games(id),"
         " account_id INTEGER NOT NULL REFERENCES accounts(id),"
         " place INTEGER NOT NULL, score INTEGER NOT NULL,"
         " elo_before INTEGER NOT NULL, elo_after INTEGER NOT NULL);");
    // Amis (v3.8) : une ligne = « account_id demande / accepte friend_id »
    exec("CREATE TABLE IF NOT EXISTS friends ("
         " account_id INTEGER NOT NULL REFERENCES accounts(id),"
         " friend_id INTEGER NOT NULL REFERENCES accounts(id),"
         " created_at INTEGER NOT NULL,"
         " PRIMARY KEY (account_id, friend_id));");
    // Succès (v3.8)
    exec("CREATE TABLE IF NOT EXISTS achievements ("
         " account_id INTEGER NOT NULL REFERENCES accounts(id),"
         " code TEXT NOT NULL,"
         " unlocked_at INTEGER NOT NULL,"
         " PRIMARY KEY (account_id, code));");
    // Colonnes ajoutées en v3.5 (sans effet si elles existent déjà)
    exec("ALTER TABLE accounts ADD COLUMN banned_until INTEGER NOT NULL DEFAULT 0;");
    exec("ALTER TABLE accounts ADD COLUMN ban_reason TEXT NOT NULL DEFAULT '';");
    exec("ALTER TABLE games ADD COLUMN cancelled INTEGER NOT NULL DEFAULT 0;");
}

bool Account::banned() const
{
    return banned_until == BAN_FOREVER || banned_until > static_cast<long long>(std::time(nullptr));
}

// Condition SQL : compte pas suspendu en ce moment (paramètre : maintenant)
static const char* const NOT_BANNED = "(banned_until = 0 OR (banned_until > 0 AND banned_until <= ?))";

Store::~Store()
{
    if (db_ != nullptr) {
        sqlite3_close(db_);
    }
}

bool Store::ok() const
{
    return db_ != nullptr;
}

bool Store::exec(const std::string& sql)
{
    return sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK;
}

int Store::find_or_create_account(const std::string& provider, const std::string& provider_id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    {
        Statement find(db_, "SELECT id FROM accounts WHERE provider = ? AND provider_id = ?");
        find.bind(1, provider);
        find.bind(2, provider_id);
        if (find.row()) {
            return find.integer(0);
        }
    }
    Statement insert(db_, "INSERT INTO accounts (provider, provider_id, created_at) VALUES (?, ?, ?)");
    insert.bind(1, provider);
    insert.bind(2, provider_id);
    insert.bind(3, static_cast<long long>(std::time(nullptr)));
    if (!insert.done()) {
        return 0;
    }
    return static_cast<int>(sqlite3_last_insert_rowid(db_));
}

int Store::find_account(const std::string& provider, const std::string& provider_id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    Statement find(db_, "SELECT id FROM accounts WHERE provider = ? AND provider_id = ?");
    find.bind(1, provider);
    find.bind(2, provider_id);
    return find.row() ? find.integer(0) : 0;
}

bool Store::get_account(int id, Account& account)
{
    std::lock_guard<std::mutex> lock(mutex_);
    Statement get(db_, "SELECT id, provider, IFNULL(pseudo, ''), elo, games, wins, banned_until, ban_reason"
                       " FROM accounts WHERE id = ?");
    get.bind(1, static_cast<long long>(id));
    if (!get.row()) {
        return false;
    }
    account.id = get.integer(0);
    account.provider = get.text(1);
    account.pseudo = get.text(2);
    account.elo = get.integer(3);
    account.games = get.integer(4);
    account.wins = get.integer(5);
    account.banned_until = get.big(6);
    account.ban_reason = get.text(7);
    return true;
}

std::string Store::set_pseudo(int id, const std::string& pseudo)
{
    std::string error = check_pseudo(pseudo);
    if (!error.empty()) {
        return error;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    Statement update(db_, "UPDATE accounts SET pseudo = ? WHERE id = ?");
    update.bind(1, pseudo);
    update.bind(2, static_cast<long long>(id));
    if (!update.done()) {
        return "This username is already taken.";
    }
    return "";
}

std::string Store::create_session(int account)
{
    std::string token = random_hex(32);
    std::lock_guard<std::mutex> lock(mutex_);
    Statement insert(db_, "INSERT INTO sessions (token_hash, account_id, created_at) VALUES (?, ?, ?)");
    insert.bind(1, sha256_hex(token));
    insert.bind(2, static_cast<long long>(account));
    insert.bind(3, static_cast<long long>(std::time(nullptr)));
    insert.done();
    return token;
}

int Store::account_for_token(const std::string& token)
{
    if (token.empty()) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    Statement find(db_, "SELECT account_id FROM sessions WHERE token_hash = ?");
    find.bind(1, sha256_hex(token));
    return find.row() ? find.integer(0) : 0;
}

void Store::delete_session(const std::string& token)
{
    std::lock_guard<std::mutex> lock(mutex_);
    Statement remove(db_, "DELETE FROM sessions WHERE token_hash = ?");
    remove.bind(1, sha256_hex(token));
    remove.done();
}

void Store::record_game(Mode mode, const std::vector<GameResult>& results)
{
    std::lock_guard<std::mutex> lock(mutex_);
    exec("BEGIN;");
    Statement game(db_, "INSERT INTO games (mode, finished_at) VALUES (?, ?)");
    game.bind(1, std::string(mode == Mode::Classic ? "classique" : "rapide"));
    game.bind(2, static_cast<long long>(std::time(nullptr)));
    game.done();
    long long game_id = sqlite3_last_insert_rowid(db_);
    for (const GameResult& result : results) {
        Statement player(db_, "INSERT INTO game_players VALUES (?, ?, ?, ?, ?, ?)");
        player.bind(1, game_id);
        player.bind(2, static_cast<long long>(result.account));
        player.bind(3, static_cast<long long>(result.place));
        player.bind(4, static_cast<long long>(result.score));
        player.bind(5, static_cast<long long>(result.elo_before));
        player.bind(6, static_cast<long long>(result.elo_after));
        player.done();
        Statement update(db_, "UPDATE accounts SET elo = ?, games = games + 1, wins = wins + ? WHERE id = ?");
        update.bind(1, static_cast<long long>(result.elo_after));
        update.bind(2, static_cast<long long>(result.place == 0 ? 1 : 0));
        update.bind(3, static_cast<long long>(result.account));
        update.done();
    }
    exec("COMMIT;");
}

std::vector<Account> Store::leaderboard(int limit)
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::string sql = std::string("SELECT id, provider, pseudo, elo, games, wins FROM accounts"
                                  " WHERE pseudo IS NOT NULL AND games > 0 AND ")
                      + NOT_BANNED + " ORDER BY elo DESC, wins DESC LIMIT ?";
    Statement list(db_, sql.c_str());
    list.bind(1, static_cast<long long>(std::time(nullptr)));
    list.bind(2, static_cast<long long>(limit));
    std::vector<Account> accounts;
    while (list.row()) {
        Account account;
        account.id = list.integer(0);
        account.provider = list.text(1);
        account.pseudo = list.text(2);
        account.elo = list.integer(3);
        account.games = list.integer(4);
        account.wins = list.integer(5);
        accounts.push_back(account);
    }
    return accounts;
}

std::vector<std::pair<std::string, int>> Store::discord_ranks()
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::string sql = std::string("SELECT provider_id, elo FROM accounts WHERE provider = 'discord'"
                                  " AND pseudo IS NOT NULL AND games > 0 AND ")
                      + NOT_BANNED;
    Statement list(db_, sql.c_str());
    list.bind(1, static_cast<long long>(std::time(nullptr)));
    std::vector<std::pair<std::string, int>> ranks;
    while (list.row()) {
        ranks.push_back({list.text(0), list.integer(1)});
    }
    return ranks;
}

int Store::rank_of(int id)
{
    Account account;
    if (!get_account(id, account) || account.games == 0 || account.banned() || account.pseudo.empty()) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    std::string sql = std::string("SELECT COUNT(*) FROM accounts WHERE pseudo IS NOT NULL AND games > 0 AND ")
                      + NOT_BANNED + " AND (elo > ? OR (elo = ? AND wins > ?))";
    Statement count(db_, sql.c_str());
    count.bind(1, static_cast<long long>(std::time(nullptr)));
    count.bind(2, static_cast<long long>(account.elo));
    count.bind(3, static_cast<long long>(account.elo));
    count.bind(4, static_cast<long long>(account.wins));
    return count.row() ? count.integer(0) + 1 : 0;
}

int Store::find_by_pseudo(const std::string& pseudo)
{
    std::lock_guard<std::mutex> lock(mutex_);
    Statement find(db_, "SELECT id FROM accounts WHERE pseudo = ? COLLATE NOCASE");
    find.bind(1, pseudo);
    return find.row() ? find.integer(0) : 0;
}

void Store::ban(int id, long long until, const std::string& reason)
{
    std::lock_guard<std::mutex> lock(mutex_);
    Statement update(db_, "UPDATE accounts SET banned_until = ?, ban_reason = ? WHERE id = ?");
    update.bind(1, until);
    update.bind(2, reason);
    update.bind(3, static_cast<long long>(id));
    update.done();
    Statement sessions(db_, "DELETE FROM sessions WHERE account_id = ?");
    sessions.bind(1, static_cast<long long>(id));
    sessions.done();
}

void Store::unban(int id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    Statement update(db_, "UPDATE accounts SET banned_until = 0, ban_reason = '' WHERE id = ?");
    update.bind(1, static_cast<long long>(id));
    update.done();
}

std::vector<HistoryEntry> Store::history(int id, int limit)
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<HistoryEntry> entries;
    Statement list(db_, "SELECT g.id, g.mode, g.finished_at, p.place, p.score, p.elo_after - p.elo_before,"
                        " g.cancelled FROM game_players p JOIN games g ON g.id = p.game_id"
                        " WHERE p.account_id = ? ORDER BY g.id DESC LIMIT ?");
    list.bind(1, static_cast<long long>(id));
    list.bind(2, static_cast<long long>(limit));
    while (list.row()) {
        HistoryEntry entry;
        entry.game_id = list.integer(0);
        entry.mode = list.text(1);
        entry.finished_at = list.big(2);
        entry.place = list.integer(3);
        entry.score = list.integer(4);
        entry.elo_change = list.integer(5);
        entry.cancelled = list.integer(6) != 0;
        entries.push_back(entry);
    }
    for (HistoryEntry& entry : entries) {
        Statement players(db_, "SELECT IFNULL(a.pseudo, '(deleted)') FROM game_players p"
                               " JOIN accounts a ON a.id = p.account_id WHERE p.game_id = ? ORDER BY p.place");
        players.bind(1, static_cast<long long>(entry.game_id));
        while (players.row()) {
            entry.players.push_back(players.text(0));
        }
    }
    return entries;
}

std::string Store::cancel_game(int game_id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    Statement find(db_, "SELECT cancelled FROM games WHERE id = ?");
    find.bind(1, static_cast<long long>(game_id));
    if (!find.row()) {
        return "Game not found.";
    }
    if (find.integer(0) != 0) {
        return "This game is already cancelled.";
    }
    exec("BEGIN;");
    Statement undo(db_, "UPDATE accounts SET"
                        " elo = elo - (SELECT p.elo_after - p.elo_before FROM game_players p"
                        "              WHERE p.game_id = ?1 AND p.account_id = accounts.id),"
                        " games = games - 1,"
                        " wins = wins - (SELECT CASE WHEN p.place = 0 THEN 1 ELSE 0 END FROM game_players p"
                        "                WHERE p.game_id = ?1 AND p.account_id = accounts.id)"
                        " WHERE id IN (SELECT account_id FROM game_players WHERE game_id = ?1)");
    undo.bind(1, static_cast<long long>(game_id));
    undo.done();
    Statement mark(db_, "UPDATE games SET cancelled = 1 WHERE id = ?");
    mark.bind(1, static_cast<long long>(game_id));
    mark.done();
    exec("COMMIT;");
    return "";
}

std::string Store::add_friend(int id, int other)
{
    if (id == other) {
        return "You can't add yourself.";
    }
    std::lock_guard<std::mutex> lock(mutex_);
    Statement count(db_, "SELECT COUNT(*) FROM friends WHERE account_id = ?");
    count.bind(1, static_cast<long long>(id));
    if (count.row() && count.integer(0) >= MAX_FRIENDS) {
        return "Too many friends (100 at most).";
    }
    Statement insert(db_, "INSERT OR IGNORE INTO friends (account_id, friend_id, created_at) VALUES (?, ?, ?)");
    insert.bind(1, static_cast<long long>(id));
    insert.bind(2, static_cast<long long>(other));
    insert.bind(3, static_cast<long long>(std::time(nullptr)));
    insert.done();
    return "";
}

void Store::remove_friend(int id, int other)
{
    std::lock_guard<std::mutex> lock(mutex_);
    Statement remove(db_, "DELETE FROM friends WHERE (account_id = ? AND friend_id = ?) OR (account_id = ? AND friend_id = ?)");
    remove.bind(1, static_cast<long long>(id));
    remove.bind(2, static_cast<long long>(other));
    remove.bind(3, static_cast<long long>(other));
    remove.bind(4, static_cast<long long>(id));
    remove.done();
}

std::vector<Friend> Store::friends(int id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    // Toutes les personnes liées dans un sens ou dans l'autre, avec le sens du lien
    Statement list(db_, "SELECT a.id, a.pseudo, a.elo,"
                        " EXISTS(SELECT 1 FROM friends f WHERE f.account_id = ? AND f.friend_id = a.id),"
                        " EXISTS(SELECT 1 FROM friends f WHERE f.account_id = a.id AND f.friend_id = ?)"
                        " FROM accounts a WHERE a.pseudo IS NOT NULL AND a.id IN"
                        " (SELECT friend_id FROM friends WHERE account_id = ?"
                        "  UNION SELECT account_id FROM friends WHERE friend_id = ?)"
                        " ORDER BY a.pseudo COLLATE NOCASE");
    for (int i = 1; i <= 4; i++) {
        list.bind(i, static_cast<long long>(id));
    }
    std::vector<Friend> result;
    while (list.row()) {
        bool mine = list.integer(3) != 0;
        bool theirs = list.integer(4) != 0;
        result.push_back({list.integer(0), list.text(1), list.integer(2),
                          mine && theirs ? "friend" : mine ? "outgoing" : "incoming"});
    }
    return result;
}

bool Store::unlock_achievement(int id, const std::string& code)
{
    std::lock_guard<std::mutex> lock(mutex_);
    Statement insert(db_, "INSERT OR IGNORE INTO achievements (account_id, code, unlocked_at) VALUES (?, ?, ?)");
    insert.bind(1, static_cast<long long>(id));
    insert.bind(2, code);
    insert.bind(3, static_cast<long long>(std::time(nullptr)));
    insert.done();
    return sqlite3_changes(db_) > 0;
}

std::vector<std::string> Store::achievements(int id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    Statement list(db_, "SELECT code FROM achievements WHERE account_id = ? ORDER BY unlocked_at, code");
    list.bind(1, static_cast<long long>(id));
    std::vector<std::string> codes;
    while (list.row()) {
        codes.push_back(list.text(0));
    }
    return codes;
}

void Store::delete_account(int id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    Statement links(db_, "DELETE FROM friends WHERE account_id = ? OR friend_id = ?");
    links.bind(1, static_cast<long long>(id));
    links.bind(2, static_cast<long long>(id));
    links.done();
    Statement sessions(db_, "DELETE FROM sessions WHERE account_id = ?");
    sessions.bind(1, static_cast<long long>(id));
    sessions.done();
    // L'identité Discord / Google est effacée : une nouvelle connexion crée un nouveau compte
    Statement update(db_, "UPDATE accounts SET pseudo = NULL, provider = 'supprime',"
                          " provider_id = 'supprime-' || id WHERE id = ?");
    update.bind(1, static_cast<long long>(id));
    update.done();
}
