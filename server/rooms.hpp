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
const int CHAT_MAX_CHARS = 120;         // longueur d'un message du chat
const int CHAT_KEPT = 50;               // messages gardés par partie
const int REACTION_COUNT = 6;           // réactions rapides (« gg », « bonne chance »...)
const int MATCH_BASE_GAP = 100;         // recherche de partie : écart d'Elo accepté au départ...
const int MATCH_GAP_PER_SECOND = 20;    // ... qui s'élargit chaque seconde d'attente


// Un message du chat d'une partie (gardé en mémoire avec la partie seulement)
struct ChatMessage {
    long id;
    int seat;
    std::string text;      // texte libre, ou "" pour une réaction
    int reaction = -1;     // numéro de la réaction rapide, -1 pour un texte
    long long time;
};

// Une place dans un salon ou une partie en ligne
struct Seat {
    int account;
    std::string pseudo;
    int elo;
};

// Un joueur qui cherche un adversaire
struct QueueEntry {
    Seat seat;
    Mode mode;
    long long since;   // début de l'attente (secondes)
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
    std::string activity;       // instance de l'Activité Discord liée, "" sinon
};

// Partie en ligne : le serveur garde la seule vraie partie et arbitre les coups
struct OnlineGame {
    int id = 0;
    std::unique_ptr<Game> game;
    std::vector<Seat> seats;
    std::vector<std::string> log;      // historique en phrases (anglais)
    std::vector<std::string> log_fr;   // le même en français
    long version = 1;
    std::vector<int> elo_changes;      // rempli à la fin de la partie
    bool recorded = false;
    long long turn_start = 0;          // début du tour en cours (secondes)
    std::vector<int> missed;           // tours manqués d'affilée, par place
    long long finished_at = 0;         // fin de la partie (pour l'oublier ensuite)
    unsigned seed = 0;                 // graine de la partie (rediffusion)
    nlohmann::json actions = nlohmann::json::array();  // tout ce qui a changé la partie, dans l'ordre
    int record_id = 0;                 // numéro dans la base une fois enregistrée (rediffusion)
    std::vector<std::vector<std::string>> new_achievements;  // succès débloqués à la fin, par place
    std::vector<ChatMessage> chat;     // derniers messages du chat
    long next_chat = 1;
};

// Tous les salons et parties en cours (en mémoire), protégés par un verrou.
// Les lectures « since » attendent un changement (attente longue) pour que
// les clients voient les coups des autres sans interroger sans arrêt.
// Ce qui se passe en ce moment (tableau de bord d'administration)
struct RoomsStats {
    int online = 0;     // joueurs dont le jeu est ouvert
    int games = 0;      // parties en cours
    int lobbies = 0;    // salons qui attendent des joueurs
    int queue = 0;      // joueurs qui cherchent un adversaire
};

class Rooms {
public:
    explicit Rooms(Store& store);

    std::string create_lobby(const Seat& me, Mode mode, int seats, std::string& code,
                             const std::string& voice_channel = "");
    std::string join_lobby(const std::string& code, const Seat& me);
    // Activité Discord : rejoint le salon de cette instance d'Activité (même
    // salon vocal), ou le crée (4 places, partie classique) s'il n'y en a pas
    std::string activity_lobby(const Seat& me, const std::string& instance, std::string& code);
    std::string leave_lobby(const std::string& code, int account);
    std::string start_lobby(const std::string& code, int account, int& game_id);
    // false si le salon n'existe pas
    bool lobby_json(const std::string& code, long since, nlohmann::json& out);

    std::string play(int game_id, int account, const Move& move);
    // --- Recherche de partie : deux joueurs du même mode, d'Elo proche (l'écart
    //     accepté grandit avec l'attente), partie à 2 lancée tout de suite
    void join_queue(const Seat& me, Mode mode);
    void leave_queue(int account);
    // {"status": "idle" | "waiting" (+ "waited", "players_waiting") | "matched" (+ "game_id")}
    void queue_json(int account, nlohmann::json& out);

    // Chat de la partie : texte libre (`reaction` = -1) ou réaction rapide.
    // Anti-spam : 1 message par seconde, 6 par tranche de 20 secondes.
    std::string chat(int game_id, int account, const std::string& text, int reaction);
    // Abandon volontaire d'un joueur (classé dernier, Elo perdu)
    std::string forfeit(int game_id, int account);
    // Compte suspendu ou supprimé : il quitte ses salons et abandonne ses parties
    void remove_account(int account);
    // À appeler chaque seconde : temps écoulés (le tour passe ; après
    // MAX_MISSED_TURNS tours manqués d'affilée, abandon) et ménage des
    // parties finies depuis longtemps
    void tick();
    // false si la partie n'existe pas
    bool game_json(int game_id, int account, long since, nlohmann::json& out);

    // --- Présence : un joueur est « en jeu » s'il a contacté le serveur récemment
    void touch(int account);
    bool is_online(int account);
    // "playing" (partie en cours), "online" ou "offline" (liste d'amis)
    std::string status_of(int account);
    // Partie en cours de ce joueur (0 : aucune), pour la regarder
    int playing_game(int account);
    // Parties en cours qu'on peut regarder (spectateurs), les mieux classées d'abord
    void live_games_json(nlohmann::json& out);

    // --- Notifications vers le jeu d'un joueur (ex. proposition du bot Discord)
    void notify(int account, nlohmann::json notification);
    // La même notification à tous les joueurs dont le jeu est ouvert (annonce)
    void notify_online(const nlohmann::json& notification);
    RoomsStats stats();
    // Administration : retire une partie en cours bloquée, sans Elo ni
    // enregistrement (les jeux des joueurs reviennent au menu en ligne)
    std::string end_game(int game_id);
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
    std::map<int, std::vector<long long>> chat_times_;  // derniers messages de chaque compte (anti-spam)
    std::map<int, std::vector<nlohmann::json>> notifications_;
    long next_notification_ = 1;
    std::vector<nlohmann::json> bot_events_;
    long next_bot_event_ = 1;

    std::vector<QueueEntry> queue_;     // recherche de partie
    std::map<int, int> matched_;        // compte -> partie trouvée (pas encore vue par le jeu)

    std::string new_code();
    int new_game(const std::vector<Seat>& seats, Mode mode);  // verrou déjà pris
    void try_match();                                          // verrou déjà pris
    void lobby_changed(const Lobby& lobby, bool closed);
    void remove_from_lobbies(int account);
    void finish_game(OnlineGame& online);
    void after_change(OnlineGame& online, size_t events_before);
    void wait_for_change(std::unique_lock<std::mutex>& lock, const std::function<long()>& version, long since);
};

#endif
