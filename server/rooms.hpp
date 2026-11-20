#ifndef SPLENDOR_SERVER_ROOMS_HPP
#define SPLENDOR_SERVER_ROOMS_HPP

#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <vector>

#include "json.hpp"

#include "../src/game.hpp"
#include "../src/move.hpp"
#include "store.hpp"

// Une place dans un salon ou une partie en ligne
struct Seat {
    int account;
    std::string pseudo;
    int elo;
};

// Salon d'attente : on le rejoint avec son code, l'hôte lance la partie
struct Lobby {
    std::string code;
    int host = 0;
    Mode mode = Mode::Classic;
    int seats = 2;
    std::vector<Seat> members;
    int game_id = 0;      // partie lancée (0 : pas encore)
    long version = 1;     // augmente à chaque changement (attente longue)
};

// Partie en ligne : le serveur garde la seule vraie partie et arbitre les coups
struct OnlineGame {
    int id = 0;
    std::unique_ptr<Game> game;
    std::vector<Seat> seats;
    std::vector<std::string> log;      // historique en phrases
    long version = 1;
    std::vector<int> elo_changes;      // rempli à la fin de la partie
    bool recorded = false;
};

// Tous les salons et parties en cours (en mémoire), protégés par un verrou.
// Les lectures « since » attendent un changement (attente longue) pour que
// les clients voient les coups des autres sans interroger sans arrêt.
class Rooms {
public:
    explicit Rooms(Store& store);

    std::string create_lobby(const Seat& me, Mode mode, int seats, std::string& code);
    std::string join_lobby(const std::string& code, const Seat& me);
    std::string leave_lobby(const std::string& code, int account);
    std::string start_lobby(const std::string& code, int account, int& game_id);
    // false si le salon n'existe pas
    bool lobby_json(const std::string& code, long since, nlohmann::json& out);

    std::string play(int game_id, int account, const Move& move);
    // false si la partie n'existe pas
    bool game_json(int game_id, int account, long since, nlohmann::json& out);

    void set_wait_seconds(int seconds);
    // Appelée quand un salon est créé (annonce Discord, v3.2)
    std::function<void(const Lobby&)> on_lobby_created;

private:
    Store& store_;
    std::mutex mutex_;
    std::condition_variable changed_;
    std::map<std::string, Lobby> lobbies_;
    std::map<int, OnlineGame> games_;
    int next_game_ = 1;
    int wait_seconds_ = 25;
    std::mt19937 random_;

    std::string new_code();
    void remove_from_lobbies(int account);
    void finish_game(OnlineGame& online);
    void wait_for_change(std::unique_lock<std::mutex>& lock, const std::function<long()>& version, long since);
};

#endif
