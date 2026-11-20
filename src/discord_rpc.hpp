#ifndef SPLENDOR_DISCORD_RPC_HPP
#define SPLENDOR_DISCORD_RPC_HPP

#include <memory>
#include <string>

// Rich Presence Discord : affiche ce que fait le joueur sur son profil
// Discord (« Partie classique — tour 7 ») et permet à ses amis de rejoindre
// son salon depuis Discord. Passe par l'IPC local de l'application Discord
// (socket Unix sous Linux, tube nommé sous Windows) : aucun SDK à installer.
// Tout se fait dans un fil ; si Discord n'est pas lancé, rien ne se passe.
// Ce fichier n'inclut pas raylib (sous Windows, windows.h est incompatible).

struct PresenceInfo {
    std::string details;      // première ligne (« Partie en ligne (classique) »)
    std::string state;        // deuxième ligne (« Tour 7 · 12 pts »)
    long long start = 0;      // début (secondes depuis 1970), 0 : aucun
    std::string party_id;     // salon en ligne (code), "" : aucun
    int party_size = 0;
    int party_max = 0;
    std::string join_secret;  // permet de rejoindre depuis Discord (code du salon)

    bool operator==(const PresenceInfo& other) const;
};

struct DiscordState;  // partagé avec le fil

class DiscordPresence {
public:
    DiscordPresence();
    ~DiscordPresence();
    DiscordPresence(const DiscordPresence&) = delete;
    DiscordPresence& operator=(const DiscordPresence&) = delete;

    // Démarre (une fois) avec l'identifiant public de l'application Discord
    void start(const std::string& application_id);
    // Nouvelle présence (envoyée seulement si elle a changé)
    void update(const PresenceInfo& info);
    // Un ami a cliqué « Rejoindre » dans Discord : renvoie true et le secret
    bool take_join(std::string& secret);
    bool connected() const;

private:
    std::shared_ptr<DiscordState> state_;
};

#endif
