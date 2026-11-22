#ifndef SPLENDOR_NET_HPP
#define SPLENDOR_NET_HPP

#include <memory>
#include <string>
#include <vector>

// Requêtes HTTP vers le serveur en ligne, sans bloquer la fenêtre : chaque
// requête part dans un fil d'exécution, et la boucle de jeu récupère les
// réponses avec poll(). Ce fichier n'inclut ni raylib ni la bibliothèque
// réseau (sous Windows, windows.h entre en conflit avec raylib).

struct NetResponse {
    int tag;             // repère choisi par l'appelant (type de requête)
    int status;          // code HTTP, 0 si le serveur n'a pas répondu
    std::string body;
};

struct NetQueue;  // file des réponses, partagée avec les fils

class NetClient {
public:
    NetClient();
    void set_server(const std::string& url);   // ex. https://splendor.exemple.fr
    void set_token(const std::string& token);  // jeton de session ("" : aucun)
    const std::string& server() const;

    // Envoie une requête (method = "GET" ou "POST", body = JSON) ; la réponse
    // arrivera plus tard dans poll() avec le même `tag`. Une requête de fond
    // (attente longue, rafraîchissement) ne compte pas dans pending() : elle
    // ne doit pas griser les boutons.
    void send(int tag, const std::string& method, const std::string& path, const std::string& body = "",
              bool background = false);

    // Réponses arrivées depuis le dernier appel
    std::vector<NetResponse> poll();

    int pending() const;  // requêtes en cours lancées par le joueur (hors requêtes de fond)

private:
    std::string server_;
    std::string token_;
    std::shared_ptr<NetQueue> queue_;
};

// Chaîne aléatoire de lettres et chiffres (identifiant de connexion)
std::string random_login_id();

#endif
