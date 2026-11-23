#ifndef SPLENDOR_GUI_HPP
#define SPLENDOR_GUI_HPP

#include <memory>
#include <string>
#include <vector>

#include "raylib.h"

#include "discord_rpc.hpp"
#include "game.hpp"
#include "i18n.hpp"
#include "net.hpp"
#include "types.hpp"

// Interface graphique (raylib).
// Tout est dessiné dans un repère virtuel de BOARD_WIDTH x BOARD_HEIGHT, mis
// à l'échelle de la fenêtre. Les mêmes fonctions de placement (rectangles)
// servent au dessin et à la détection des clics.

const float BOARD_WIDTH = 1280;
const float BOARD_HEIGHT = 800;
const int GOLD_INDEX = COLOR_COUNT;  // indice de l'or à côté des 5 gemmes
const char* const SAVE_FILE = "partie.splendor";
const char* const DEFAULT_SERVER = "https://gemspire.epsilon-forge.fr";  // serveur officiel (Raspberry Pi)

enum class Screen { Home, Setup, Board, Login, Pseudo, Online, Lobby, Leaderboard, Update, Friends, Achievements };

// Ce que le joueur a sélectionné à la souris
enum class Target { None, Visible, Deck, Reserved };

// Tous les éléments cliquables. `value` précise lequel (joueur, couleur...).
enum class ButtonId {
    // Accueil
    NewGame, Resume, Quit, PlayOnline,
    // En ligne
    LoginDiscord, LoginGoogle, LoginDev, PseudoBox, PseudoOk, CodeBox, JoinLobby, CreateLobby, CreateMode,
    CreateSeats, ShowLeaderboard, Logout, BackHome, BackOnline, ReturnToGame, LobbyStart, LobbyLeave, CopyCode,
    Invite, OfferCreate, OfferIgnore, Download, Language,
    // Chat d'une partie en ligne
    Chat, ChatSend, ChatBox, ChatReaction, ChatMute,
    // Amis
    ShowFriends, FriendBox, FriendAdd, FriendAccept, FriendRemove, ShowAchievements,
    // Nouvelle partie
    Mode, Count, NameBox, Swatch, Kind, Start, Back,
    // Plateau
    Take, Buy, Reserve, Cancel, Save, Journal, Menu, Forfeit,
    // Fin de partie
    Replay
};

struct Button {
    ButtonId id;
    std::string label;
    bool enabled;
    Rectangle rect;
    int value;
    bool selected;   // bouton « allumé » (choix courant)
};

// Couleurs que les joueurs peuvent choisir (différentes de celles des gemmes)
struct PlayerColor {
    const char* name;
    Color color;
};
const int PLAYER_COLOR_COUNT = 5;
const PlayerColor PLAYER_COLORS[PLAYER_COLOR_COUNT] = {
    {"Purple", {168, 97, 235, 255}},
    {"Orange", {255, 140, 26, 255}},
    {"Turquoise", {51, 217, 204, 255}},
    {"Pink", {255, 115, 184, 255}},
    {"Brown", {158, 102, 56, 255}},
};

// Réglages de l'écran « Nouvelle partie »
struct Setup {
    Mode mode = Mode::Classic;
    int count = 2;
    std::string names[4] = {"Player 1", "Player 2", "Player 3", "Player 4"};  // traduits au lancement
    int colors[4] = {0, 1, 2, 3};
    Controller kinds[4] = {Controller::Human, Controller::AiMedium, Controller::AiMedium, Controller::AiMedium};
    int focused_name = -1;   // champ de nom en cours de saisie (-1 : aucun)
};

// Une petite animation : jeton qui glisse, ou carte qui part vers un joueur
struct Animation {
    double start;
    double duration;
    Vector2 from;
    Vector2 to;
    int gem;         // jeton : indice de gemme (GOLD_INDEX pour l'or) ; -1 pour une carte
    Card card;
};

// Une carte qui vient d'apparaître sur le plateau (elle se retourne)
struct Flip {
    double start;
    int level;
    int index;
};

// Annonce affichée quelques secondes au milieu du plateau (visite d'un noble,
// dernier tour). Les annonces passent l'une après l'autre.
struct Banner {
    std::string title;
    std::string text;
    int player;          // joueur concerné (couleur du cadre), -1 : aucun
    double start = -1;   // heure d'affichage (-1 : pas encore affichée)
};
const double BANNER_SECONDS = 3.0;

// Un message du chat d'une partie en ligne
struct ChatLine {
    long id;
    int seat;
    std::string text;   // texte libre, "" pour une réaction
    int reaction;       // numéro de réaction rapide, -1 pour un texte
};

// Réactions rapides du chat (même ordre que le serveur), traduites à l'affichage
const int REACTION_COUNT = 6;
const char* const REACTIONS[REACTION_COUNT] = {"GG", "Good luck!", "Nice move!", "Oops!", "Thanks!", "Hello!"};
const int CHAT_MAX_CHARS = 120;
const Rectangle CHAT_PANEL = {300, 110, 680, 580};  // panneau du chat sur le plateau

// Une ligne de la liste d'amis
struct FriendLine {
    std::string pseudo;
    int elo;
    std::string relation;   // "friend", "incoming" (demande reçue), "outgoing" (demande envoyée)
    std::string status;     // amis : "playing", "online", "offline"
};
const int FRIENDS_SHOWN = 9;

// Tout ce qui concerne le jeu en ligne
struct OnlineState {
    bool in_game = false;            // le plateau affiche une partie en ligne
    bool dev_login = false;          // bouton « connexion de test » (SPLENDOR_DEV=1)
    std::string login_id;            // connexion en cours dans le navigateur
    double next_login_poll = 0;
    std::string pseudo;              // compte connecté
    int account = 0;
    int elo = 1000;
    int games = 0;
    int wins = 0;
    std::string pseudo_input;        // champs de saisie
    std::string code_input;
    int focus = 0;                   // 1 : pseudo, 2 : code
    Mode create_mode = Mode::Classic;
    int create_seats = 2;
    std::string lobby_code;          // salon actuel
    std::string lobby_mode;
    int lobby_seats = 2;
    int lobby_host = 0;
    long lobby_version = 0;
    std::vector<std::string> lobby_members;
    std::vector<int> lobby_elos;
    int game_id = 0;                 // partie en ligne en cours
    long game_version = 0;
    int my_seat = -1;
    bool game_over = false;
    bool move_pending = false;       // coup envoyé, réponse attendue
    double turn_deadline = 0;        // heure (GetTime) où le tour en cours passe
    double forfeit_confirm = 0;      // « Abandonner » cliqué une fois : confirmer avant cette heure
    double next_game_poll = 0;       // nouvelle tentative après une coupure
    bool game_poll_running = false;
    bool lobby_poll_running = false;
    std::vector<std::string> log;    // historique envoyé par le serveur (anglais)
    std::vector<std::string> log_fr; // le même en français
    std::vector<ChatLine> chat;      // chat de la partie (envoyé par le serveur)
    long chat_seen = 0;              // dernier message lu (panneau du chat ouvert)
    bool chat_open = false;
    std::string chat_input;
    bool muted[4] = {false, false, false, false};  // joueurs mis en sourdine (par place)
    std::string chat_toast;          // dernier message reçu, montré quelques secondes
    double chat_toast_until = 0;
    std::vector<int> seat_elos;
    std::vector<int> elo_changes;
    long notification_last = -1;     // dernière notification vue (-1 : pas encore demandé)
    bool notification_poll_running = false;
    std::string offer_channel;       // proposition du bot Discord : salon vocal à lier
    std::string offer_name;
    bool activity_lobby_asked = false; // Activité Discord : salon partagé déjà demandé
    std::string pending_join;        // code reçu de Discord (« Rejoindre ») en attente de connexion
    std::vector<std::string> achievements;      // succès du compte (codes, envoyés par /me)
    std::vector<std::string> new_achievements;  // succès débloqués par la partie qui vient de finir
    std::vector<FriendLine> friends;       // liste d'amis (écran Amis)
    std::string friend_input;
    double next_friends_poll = 0;
    std::vector<std::string> board_names;  // classement
    std::vector<int> board_elos;
    std::vector<int> board_games;
    std::vector<int> board_wins;
};

// Tout l'état de l'interface
struct App {
    Screen screen = Screen::Home;
    Lang lang = Lang::English;              // langue des textes (bouton sur l'accueil, fichier langue.txt)
    Setup setup;
    std::unique_ptr<Game> game;
    std::vector<int> player_colors;         // couleur de chaque joueur (indice dans PLAYER_COLORS)
    Resources selection = {0, 0, 0, 0, 0};  // jetons choisis dans la réserve
    Target target = Target::None;
    int target_level = 0;
    int target_index = 0;                   // position, à partir de 0
    int shown_events = 0;                   // événements de Game déjà affichés
    std::string message;                    // ligne d'état
    bool message_is_error = false;
    bool journal_open = false;
    bool curtain = false;                   // écran de passage entre deux joueurs
    int last_player = 0;                    // pour détecter le changement de joueur
    double ai_next_time = 0;                // heure du prochain coup de l'ordinateur
    std::vector<Animation> animations;
    std::vector<Flip> flips;
    std::vector<Banner> banners;            // annonces en attente (la première est affichée)
    bool quit = false;
    NetClient net;
    OnlineState online;
    DiscordPresence discord;
    std::string min_version;               // version exigée par le serveur (écran « Mise à jour »)
    std::string download_url;
    long long screen_start = 0;            // pour le chronomètre de la Rich Presence
    Screen presence_screen = Screen::Home; // écran affiché lors du dernier calcul de présence

    Font font = {};
    Font bold = {};
};

// --- Placement des éléments du plateau (repère virtuel) ---
Rectangle noble_rect(int index);
Rectangle deck_rect(int level);
Rectangle card_rect(int level, int index);
Rectangle reserved_rect(int index);
Rectangle player_rect(int player);
Vector2 bank_token_center(int gem);
Vector2 player_token_center(int player, int gem);
const float BANK_TOKEN_RADIUS = 28;
const float PLAYER_TOKEN_RADIUS = 18;

bool point_in(Vector2 point, Rectangle rect);

// Le joueur dont on montre la main (cartes réservées) : en ligne, toujours
// le sien ; en local, le joueur dont c'est le tour
int viewer_seat(const App& app);

// Messages du chat non lus (ni les siens, ni ceux des joueurs en sourdine)
int unread_chat(const OnlineState& online);

// Texte d'un message du chat dans la langue choisie (réactions traduites)
std::string chat_text(const App& app, const ChatLine& line);

// Boutons visibles selon l'écran et l'état
std::vector<Button> visible_buttons(const App& app);

// Dessine l'écran courant, dans le repère virtuel
void draw_app(const App& app, Vector2 mouse);

// « Missing 2 Red, 1 Blue » pour une carte, ou "" si elle est achetable
std::string missing_text(const Game& game, const Card& card, Lang lang);

#endif
