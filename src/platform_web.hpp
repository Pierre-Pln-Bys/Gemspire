#ifndef SPLENDOR_PLATFORM_WEB_HPP
#define SPLENDOR_PLATFORM_WEB_HPP

#include <string>

// Ce qui change dans le navigateur (compilation WebAssembly seulement,
// fichier platform_web.cpp) :
// - la page agrandit le dessin (1280 x 800) à sa taille, en CSS (shell.html) ;
// - la saisie de texte passe par une boîte du navigateur, qui ouvre le
//   clavier du téléphone (le dessin raylib ne le peut pas) ;
// - les fichiers (session, sauvegarde) sont recopiés dans le stockage du
//   navigateur pour survivre à un rechargement.

// Demande un texte au joueur ; true s'il a validé (`value` est alors remplacé)
bool web_prompt(const std::string& question, std::string& value);

// Enregistre les fichiers dans le stockage du navigateur
void web_save_files();

// Adresse du site (https://gemspire.epsilon-forge.fr) : le jeu parle au serveur qui l'a servi
std::string web_origin();

// Quitte le jeu pour cette adresse (connexion Discord / Google dans le même onglet)
void web_go_to(const std::string& url);

// Activité Discord (jeu ouvert dans un salon vocal) : vrai si la page a été
// ouverte par Discord. web_activity_start demande à Discord un code de
// connexion pour l'application `client_id` ; web_activity_take renvoie
// "code:...", "error:..." ou "" (pas encore de réponse).
bool web_in_activity();
void web_activity_start(const std::string& client_id);
std::string web_activity_take();
// Instance de l'Activité (la même pour tous les joueurs du salon vocal)
std::string web_activity_instance();

// Langue du navigateur ("fr-FR", "en-US"...)
std::string web_language();

// Connexion en cours au retour de Discord / Google (?login=... dans l'adresse),
// "" sinon ; l'adresse est nettoyée
std::string web_login_from_address();

#endif
