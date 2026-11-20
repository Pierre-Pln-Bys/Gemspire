// Serveur de Splendor en ligne : comptes (Google / Discord, sans mot de
// passe), salons, parties arbitrées par le serveur, classement Elo.
// Réglages par variables d'environnement : voir server/README.md.
#include <iostream>

#include "app.hpp"

int main()
{
    ServerConfig config = config_from_environment();
    ServerApp app(config);
    if (!app.ok()) {
        std::cerr << "Impossible d'ouvrir la base " << config.database << "\n";
        return 1;
    }
    httplib::Server server;
    // Assez de fils pour les attentes longues des joueurs connectés
    server.new_task_queue = [] { return new httplib::ThreadPool(64); };
    app.setup_routes(server);
    std::cout << "Serveur Splendor sur " << config.host << ":" << config.port << " (adresse publique "
              << config.public_url << ")" << (config.dev_login ? " [connexion de test activée]" : "") << std::endl;
    if (!server.listen(config.host, config.port)) {
        std::cerr << "Impossible d'écouter sur le port " << config.port << "\n";
        return 1;
    }
    return 0;
}
