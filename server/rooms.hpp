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

const int MAX_MISSED_TURNS = 3;         // tours manqués d'affilée avant l'abandon
const int FINISHED_GAME_SECONDS = 3600; // une partie finie reste visible 1 heure

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
    std::string voice_channel;  // salon vocal Discord lié (bot « Crée ta partie »), "" sinon
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
    long long turn_start = 0;          // début du tour en cours (secondes)
    std::vector<int> missed;           // tours manqués d'affilée, par place
    long long finished_at = 0;         // fin de la partie (pour l'oublier ensuite)
};

// Tous les salons et parties en cours (en mémoire), protégés par un verrou.
// Les lectures « since » attendent un changement (attente longue) pour que
// les clients voient les coups des autres sans interroger sans arrêt.
class Rooms {
public:
    explicit Rooms(Store& store);

    std::string create_lobby(const Seat& me, Mode mode, int seats, std::string& code,
                             const std::string& voice_channel = "");
    std::string join_lobby(const std::string& code, const Seat& me);
    std::string leave_lobby(const std::string& code, int account);
    std::string start_lobby(const std::string& code, int account, int& game_id);
    // false si le salon n'existe pas
    bool lobby_json(const std::string& code, long since, nlohmann::json& out);

    std::string play(int game_id, int account, const Move& move);
    // Abandon volontaire d'un joueur (classé dernier, Elo perdu)
    std::string forfeit(int game_id, int account);
    // À appeler chaque seconde : temps écoulés (le tour passe ; après
    // MAX_MISSED_TURNS tours manqués d'affilée, abandon) et ménage des
    // parties finies depuis longtemps
    void tick();
    // false si la partie n'existe pas
    bool game_json(int game_id, int account, long since, nlohmann::json& out);

    // --- Présence : un joueur est « en jeu » s'il a contacté le serveur récemment
    void touch(int account);
    bool is_online(int account);

    // --- Notifications vers le jeu d'un joueur (ex. proposition du bot Discord)
    void notify(int account, nlohmann::json notification);
    void notifications_json(int account, long since, nlohmann::json& out);

    // --- Événements pour le bot Discord : salons liés à un salon vocal
    void bot_events_json(long since, nlohmann::json& out);

    void set_wait_seconds(int seconds);
    void set_turn_seconds(int seconds);
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
    int turn_seconds_ = 120;
    std::mt19937 random_;

    std::map<int, long long> last_seen_;
    std::map<int, std::vector<nlohmann::json>> notifications_;
    long next_notification_ = 1;
    std::vector<nlohmann::json> bot_events_;
    long next_bot_event_ = 1;

    std::string new_code();
    void lobby_changed(const Lobby& lobby, bool closed);
    void remove_from_lobbies(int account);
    void finish_game(OnlineGame& online);
    void after_change(OnlineGame& online, size_t events_before);
    void wait_for_change(std::unique_lock<std::mutex>& lock, const std::function<long()>& version, long since);
};

#endif
