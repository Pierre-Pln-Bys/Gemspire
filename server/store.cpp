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
            return "Le pseudo ne peut contenir que des lettres, des chiffres, des espaces, - et _.";
        }
    }
    if (characters < 3 || characters > 16) {
        return "Le pseudo doit faire entre 3 et 16 caractères.";
    }
    if (pseudo.front() == ' ' || pseudo.back() == ' ') {
        return "Le pseudo ne peut pas commencer ni finir par un espace.";
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
}

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

bool Store::get_account(int id, Account& account)
{
    std::lock_guard<std::mutex> lock(mutex_);
    Statement get(db_, "SELECT id, provider, IFNULL(pseudo, ''), elo, games, wins FROM accounts WHERE id = ?");
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
        return "Ce pseudo est déjà pris.";
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
    Statement list(db_, "SELECT id, provider, pseudo, elo, games, wins FROM accounts"
                        " WHERE pseudo IS NOT NULL AND games > 0 ORDER BY elo DESC, wins DESC LIMIT ?");
    list.bind(1, static_cast<long long>(limit));
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
