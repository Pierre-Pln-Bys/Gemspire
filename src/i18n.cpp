#include "i18n.hpp"

#include <cstdlib>
#include <map>

// Traduction française de chaque texte anglais du jeu. Un texte absent de la
// table s'affiche tel quel (même mot dans les deux langues, nom propre...).
// {0}, {1}... sont remplacés après la traduction.
static const std::map<std::string, std::string>& french_table()
{
    static const std::map<std::string, std::string> table = {
        // --- Couleurs des gemmes et des joueurs
        {"White", "Blanc"},
        {"Blue", "Bleu"},
        {"Green", "Vert"},
        {"Red", "Rouge"},
        {"Black", "Noir"},
        {"Gold", "Or"},
        {"Purple", "Violet"},
        {"Pink", "Rose"},
        {"Brown", "Marron"},
        {"classic", "classique"},
        {"quick", "rapide"},

        // --- Historique (describe_event)
        {"{0} takes {1}.", "{0} prend {1}."},
        {"{0} buys {1}.", "{0} achète {1}."},
        {"{0} buys their reserved card, {1}.", "{0} achète sa réservée, {1}."},
        {"{0} reserves {1}.", "{0} réserve {1}."},
        {"{0} reserves {1} and takes a gold token.", "{0} réserve {1} et prend un jeton or."},
        {"a card from the deck", "une carte de la pioche"},
        {"a {0} card", "une carte {0}"},
        {"{0} discards {1}.", "{0} défausse {1}."},
        {"{0} visits {1} (+3 pts).", "{0} rend visite à {1} (+3 pts)."},
        {"{0} can't do anything and passes.", "{0} ne peut rien faire et passe son tour."},
        {"{0} didn't play in time and passes.", "{0} n'a pas joué à temps et passe son tour."},
        {"{0} forfeits the game (ranked last).", "{0} abandonne la partie (classé dernier)."},

        // --- Erreurs des règles (game, rules, ai, move)
        {"It's not the time for this action.", "Ce n'est pas le moment de faire cette action."},
        {"This card doesn't exist.", "Cette carte n'existe pas."},
        {"Not enough resources to buy this card.", "Pas assez de ressources pour acheter cette carte."},
        {"You have no reserved card at this position.", "Vous n'avez pas de carte réservée à cette position."},
        {"Reservations don't exist in a quick game.", "La réservation n'existe pas en partie rapide."},
        {"You already have 3 reserved cards.", "Vous avez déjà 3 cartes réservées."},
        {"This level doesn't exist.", "Ce niveau n'existe pas."},
        {"This level's deck is empty.", "La pioche de ce niveau est vide."},
        {"You have no token of this colour.", "Vous n'avez pas de jeton de cette couleur."},
        {"You have no gold token.", "Vous n'avez pas de jeton or."},
        {"This noble can't visit you.", "Ce noble ne peut pas vous rendre visite."},
        {"You can only pass during the Action step.", "On ne peut passer le tour qu'à l'étape Action."},
        {"Unknown player.", "Joueur inconnu."},
        {"You can't forfeit in the middle of a turn (discard or noble choice).",
         "Abandon impossible au milieu d'un tour (défausse ou choix du noble)."},
        {"Invalid token amount.", "Quantité de jetons invalide."},
        {"There aren't enough tokens of this colour in the bank.",
         "Il n'y a pas assez de jetons de cette couleur dans la réserve."},
        {"No token requested.", "Aucun jeton demandé."},
        {"You can't take 2 identical tokens and other tokens as well.",
         "On ne peut pas prendre 2 jetons identiques et d'autres jetons en plus."},
        {"There must be at least 4 tokens in the stack to take 2 identical ones.",
         "Il faut au moins 4 jetons dans la pile pour en prendre 2 identiques."},
        {"You must take 1 token.", "Il faut prendre 1 jeton."},
        {"You must take 2 tokens of different colours.", "Il faut prendre 2 jetons de couleurs différentes."},
        {"You must take 3 tokens of different colours.", "Il faut prendre 3 jetons de couleurs différentes."},
        {"It's not the computer's turn.", "Ce n'est pas à l'ordinateur de jouer."},
        {"The game is over.", "La partie est terminée."},
        {"Unknown colour.", "Couleur inconnue."},
        {"Unknown move.", "Coup inconnu."},
        {"Malformed move.", "Coup mal formé."},
        {"Malformed tokens.", "Jetons mal formés."},
        {"Badly specified card.", "Carte mal désignée."},
        {"Badly specified reserved card.", "Carte réservée mal désignée."},
        {"Badly specified token.", "Jeton mal désigné."},
        {"Badly specified noble.", "Noble mal désigné."},
        {"Unknown move type.", "Type de coup inconnu."},

        // --- Erreurs du serveur (affichées par le jeu)
        {"A game is played with 2, 3 or 4 players.", "Une partie se joue à 2, 3 ou 4."},
        {"No lobby with this code.", "Aucun salon avec ce code."},
        {"The game has already started.", "La partie a déjà commencé."},
        {"The lobby is full.", "Le salon est complet."},
        {"Only the host can start the game.", "Seul l'hôte peut lancer la partie."},
        {"At least 2 players are needed.", "Il faut au moins 2 joueurs."},
        {"Game not found.", "Partie introuvable."},
        {"It's not your turn.", "Ce n'est pas votre tour."},
        {"You're not playing in this game.", "Vous ne jouez pas dans cette partie."},
        {"The username can only contain letters, digits, spaces, - and _.",
         "Le pseudo ne peut contenir que des lettres, des chiffres, des espaces, - et _."},
        {"The username must be 3 to 16 characters long.", "Le pseudo doit faire entre 3 et 16 caractères."},
        {"The username can't start or end with a space.", "Le pseudo ne peut pas commencer ni finir par un espace."},
        {"This username is already taken.", "Ce pseudo est déjà pris."},
        {"Not signed in.", "Non connecté."},
        {"Username missing.", "Pseudo manquant."},
        {"Malformed request.", "Requête mal formée."},
        {"Invalid voice channel.", "Salon vocal invalide."},
        {"Account not found.", "Compte introuvable."},
        {"Choose a username first.", "Choisissez d'abord un pseudo."},
        {"Server error.", "Erreur du serveur."},
        {"Sign-in cancelled.", "Connexion annulée."},

        // --- Pages de connexion du serveur (navigateur)
        {"Sign-in failed", "Connexion impossible"},
        {"This sign-in method isn't available.", "Ce mode de connexion n'est pas disponible."},
        {"Sign-in expired", "Connexion expirée"},
        {"Start again from the game.", "Recommencez depuis le jeu."},
        {"Signed in", "Connexion réussie"},
        {"You can close this page and go back to Gemspire.",
         "Vous pouvez fermer cette page et revenir dans Gemspire."},
        {"Test sign-in is disabled.", "La connexion de test est désactivée."},
        {"Test sign-in successful", "Connexion de test réussie"},
        {"You can go back to Gemspire.", "Vous pouvez revenir dans Gemspire."},
        {"The provider refused the sign-in.", "Le fournisseur a refusé la connexion."},
        {"Unexpected reply from the provider.", "Réponse inattendue du fournisseur."},
        {"Couldn't read the account from the provider.", "Impossible de lire le compte chez le fournisseur."},

        // --- Fenêtre : boutons
        {"New game", "Nouvelle partie"},
        {"Play online", "Jouer en ligne"},
        {"Resume saved game", "Reprendre la sauvegarde"},
        {"Quit", "Quitter"},
        {"Quick", "Rapide"},
        {"Classic", "Classique"},
        {"Human", "Humain"},
        {"Easy AI", "Ordi facile"},
        {"Medium AI", "Ordi moyen"},
        {"Back", "Retour"},
        {"Play", "Jouer"},
        {"Back to online menu", "Retour au menu en ligne"},
        {"Play again", "Rejouer"},
        {"Online menu", "Menu en ligne"},
        {"Confirm?", "Confirmer ?"},
        {"Forfeit", "Abandonner"},
        {"Save", "Sauvegarder"},
        {"Log", "Journal"},
        {"Take", "Prendre"},
        {"Cancel", "Annuler"},
        {"Buy", "Acheter"},
        {"Reserve", "Réserver"},
        {"Sign in with Discord", "Se connecter avec Discord"},
        {"Sign in with Google", "Se connecter avec Google"},
        {"Test sign-in", "Connexion de test"},
        {"Confirm", "Valider"},
        {"Sign out", "Se déconnecter"},
        {"{0} players", "{0} joueurs"},
        {"Create game", "Créer la partie"},
        {"Ignore", "Ignorer"},
        {"Create lobby", "Créer le salon"},
        {"Join", "Rejoindre"},
        {"Back to the current game", "Revenir à la partie en cours"},
        {"Leaderboard", "Classement"},
        {"Home", "Accueil"},
        {"Copy code", "Copier le code"},
        {"Copy an invite", "Copier une invitation"},
        {"Start game", "Lancer la partie"},
        {"Leave lobby", "Quitter le salon"},
        {"Download the update", "Télécharger la mise à jour"},
        {"Français", "English"},  // bouton de langue : affiche l'autre langue

        // --- Fenêtre : plateau
        {"empty", "vide"},
        {"cards", "cartes"},
        {"Classic game", "Partie classique"},
        {"Quick game", "Partie rapide"},
        {"Round {0}", "Tour {0}"},
        {"Time: {0}", "Temps : {0}"},
        {"Last round!", "Dernier tour !"},
        {"Bank", "Réserve"},
        {" (you)", " (vous)"},
        {"(easy AI)", "(ordinateur facile)"},
        {"(medium AI)", "(ordinateur moyen)"},
        {"Tokens", "Jetons"},
        {"{0}/10 tokens   {1} cards   {2} noble(s)", "{0}/10 jetons   {1} cartes   {2} noble(s)"},
        {"   {0} reserved", "   {0} réservée(s)"},
        {"Reserved", "Réservées"},
        {"by", "de"},
        {"Sending move…", "Envoi du coup…"},
        {"{0}'s turn", "Au tour de {0}"},
        {"{0} is thinking…", "{0} réfléchit…"},
        {"Discard your extra tokens", "Défaussez vos jetons en trop"},
        {"Choose a noble", "Choisissez un noble"},
        {"Selected tokens:", "Jetons choisis :"},
        {"Selected card", "Carte choisie"},
        {"Level {0} deck", "Pioche du niveau {0}"},
        {"Choose an action", "Choisissez une action"},
        {"Click tokens (keys 1 to 5),", "Cliquez des jetons (touches 1 à 5),"},
        {"a card or a deck.", "une carte ou une pioche."},
        {"Enter confirms, Esc cancels.", "Entrée valide, Échap annule."},
        {"Gold outline = card you can buy", "Contour doré = carte achetable"},
        {"You can buy this card.", "Vous pouvez acheter cette carte."},
        {"Level {0} deck: {1} cards", "Pioche du niveau {0} : {1} cartes"},
        {"Level {0} deck: {1} cards (click to reserve the top card)",
         "Pioche du niveau {0} : {1} cartes (cliquez pour réserver le dessus)"},
        {"This noble will visit you.", "Ce noble vous rendra visite."},
        {"Missing bonuses: {0}", "Bonus manquants : {0}"},
        {"Missing {0}", "Il manque {0}"},
        {" (you have {0} gold)", " (vous avez {0} or)"},
        {"Game log", "Journal de la partie"},
        {"J or click to close", "J ou clic pour fermer"},
        {"Nothing yet.", "Rien pour l'instant."},
        {"Next up", "Au tour de"},
        {"Click or press Enter to show the board", "Cliquez ou appuyez sur Entrée pour afficher le plateau"},
        {"Game over", "Fin de partie"},
        {"(no player could move anymore)", "(plus aucun joueur ne pouvait jouer)"},
        {"{0} wins!", "Victoire de {0} !"},
        {"Perfect tie!", "Égalité parfaite !"},
        {"{0} cards", "{0} cartes"},
        {"{0} visits {1}", "{0} rend visite à {1}"},
        {"+{0} prestige points", "+{0} points de prestige"},
        {"{0} reached {1} points: the round is finished.", "{0} a atteint {1} points : on termine le tour de table."},

        // --- Fenêtre : accueil, nouvelle partie
        {"(no saved game found)", "(aucune sauvegarde trouvée)"},
        {"With gold and reservations (full rules)", "Avec or et réservation (règles complètes)"},
        {"No gold, no reservations (shorter)", "Sans or ni réservation (plus court)"},
        {"Players", "Joueurs"},
        {"Name", "Nom"},
        {"Colour", "Couleur"},
        {"Played by", "Joué par"},
        {"Type the name, then Enter.", "Tapez le nom, puis Entrée."},
        {"Player {0}", "Joueur {0}"},
        {"Player {0} name:", "Nom du joueur {0} :"},

        // --- Fenêtre : en ligne
        {"Sign in with Discord or Google: no password is asked or stored.",
         "Connectez-vous avec Discord ou Google : aucun mot de passe n'est demandé ni stocké."},
        {"Server: {0}", "Serveur : {0}"},
        {"Finish signing in in your browser…", "Terminez la connexion dans votre navigateur…"},
        {"Choose your username", "Choisissez votre pseudo"},
        {"3 to 16 characters: letters, digits, spaces, - and _. It will be shown on the leaderboard.",
         "3 à 16 caractères : lettres, chiffres, espaces, - et _. Il sera visible dans le classement."},
        {"Online", "En ligne"},
        {"{0} games, {1} wins", "{0} parties, {1} victoires"},
        {"Voice channel “{0}”", "Salon vocal « {0} »"},
        {"Choose the game: the channel will be limited to the number of players.",
         "Choisissez la partie : le salon sera limité au nombre de joueurs."},
        {"Create a game", "Créer une partie"},
        {"Join with a code", "Rejoindre avec un code"},
        {"Lobby", "Salon"},
        {"{0} — {1} / {2} players. Give this code to your friends.",
         "{0} — {1} / {2} joueurs. Donnez ce code à vos amis."},
        {"  (host)", "  (hôte)"},
        {"free seat", "place libre"},
        {"Waiting for the host…", "En attente de l'hôte…"},
        {"Update required", "Mise à jour nécessaire"},
        {"This version of Gemspire ({0}) is no longer accepted",
         "Cette version de Gemspire ({0}) n'est plus acceptée"},
        {"by the server. Download version {0} or newer.", "par le serveur. Téléchargez la version {0} ou plus récente."},
        {"Your current local game is saved.", "Votre partie locale en cours est sauvegardée."},
        {"Player", "Joueur"},
        {"Games", "Parties"},
        {"Wins", "Victoires"},
        {"Loading…", "Chargement…"},
        {"Nobody has played online yet.", "Personne n'a encore joué en ligne."},

        // --- Fenêtre : messages
        {"{0}'s turn.", "À {0} de jouer."},
        {"Game over.", "Partie terminée."},
        {"More than 10 tokens: click your tokens (or keys 1 to 6) to discard {0}.",
         "Plus de 10 jetons : cliquez sur vos jetons (ou touches 1 à 6) pour en défausser {0}."},
        {"Several nobles want to visit: click the one you choose.",
         "Plusieurs nobles veulent venir : cliquez sur celui de votre choix."},
        {"Game resumed. {0}'s turn.", "Partie reprise. À {0} de jouer."},
        {"Connecting to the server…", "Connexion au serveur…"},
        {"Loading the game…", "Chargement de la partie…"},
        {"Discard your extra tokens.", "Défaussez vos jetons en trop."},
        {"Your turn!", "À vous de jouer !"},
        {"Server unreachable ({0}).", "Serveur injoignable ({0})."},
        {"Server error ({0}).", "Erreur du serveur ({0})."},
        {"Session expired: please sign in again.", "Session expirée : reconnectez-vous."},
        {"Sign-in failed.", "Connexion impossible."},
        {"Signing in with Discord…", "Connexion avec Discord…"},
        {"Discord refused the sign-in.", "Discord a refusé la connexion."},
        {"The lobby no longer exists.", "Le salon n'existe plus."},
        {"This game no longer exists.", "Cette partie n'existe plus."},
        {"Retrying…", "Nouvelle tentative…"},
        {"Username (3 to 16 characters):", "Pseudo (3 à 16 caractères) :"},
        {"Lobby code (5 characters):", "Code du salon (5 caractères) :"},
        {"You are signed out.", "Vous êtes déconnecté."},
        {"Code copied.", "Code copié."},
        {"Join my Gemspire game! Code: {0} (Play online > Join). Download the game: {1}",
         "Rejoins ma partie de Gemspire ! Code : {0} (Jouer en ligne > Rejoindre). Télécharger le jeu : {1}"},
        {"Invite copied: paste it in Discord.", "Invitation copiée : collez-la dans Discord."},
        {"Game saved ({0}).", "Partie sauvegardée ({0})."},
        {"Forfeiting counts as a loss (Elo lost). Click again to confirm.",
         "Abandonner compte comme une défaite (Elo perdu). Cliquez à nouveau pour confirmer."},
        {"Gold can't be taken: you get it by reserving a card.",
         "L'or ne se prend pas : on le reçoit en réservant une carte."},
        {"No {0} tokens left.", "Il n'y a plus de jetons {0}."},
        {"At most 3 tokens of different colours.", "3 jetons de couleurs différentes au maximum."},
        {"Decks are only for reserving (classic game).", "La pioche ne sert qu'à réserver (partie classique)."},

        // --- Rich Presence Discord
        {"In a lobby ({0})", "Dans un salon ({0})"},
        {"Waiting for players", "En attente de joueurs"},
        {"Online game ({0})", "Partie en ligne ({0})"},
        {"Local game ({0})", "Partie locale ({0})"},
        {"Round {0} · {1} pts", "Tour {0} · {1} pts"},
        {"Viewing the leaderboard", "Consulte le classement"},
        {"Looking for an online game", "Cherche une partie en ligne"},
        {"In the menus", "Dans les menus"},

        // --- Console : plateau
        {"none", "aucun"},
        {"(empty)", "(vide)"},
        {"LEVEL", "NIVEAU"},
        {"deck", "pioche"},
        {"BANK", "RÉSERVE"},
        {"  (3 pts, require bonuses ◆ = owned cards)", "  (3 pts, demandent des bonus ◆ = cartes possédées)"},
        {"   no nobles left", "   plus aucun noble"},
        {" PLAYERS", " JOUEURS"},
        {" tokens", " jetons"},
        {"{0} cards  {1} nobles", "{0} cartes  {1} nobles"},
        {"  {0} reserved", "  {0} réservée(s)"},
        {" YOUR RESERVED CARDS", " VOS CARTES RÉSERVÉES"},
        {"  (b r <n> to buy)", "  (a r <n> pour acheter)"},
        {" GEMSPIRE ({0}) - round {1} - turn of ", " GEMSPIRE ({0}) - tour {1} - au tour de "},
        {" LAST ROUND!", " DERNIER TOUR !"},
        {"\n * = card you can buy    ? = help", "\n * = carte achetable    ? = aide"},
        {" GAME OVER", " FIN DE PARTIE"},
        {"Game blocked: no player can move anymore.", "Partie bloquée : plus aucun joueur ne peut jouer."},
        {"{0} pts, {1} cards", "{0} pts, {1} cartes"},
        {"Perfect tie between:", "Égalité parfaite entre :"},

        // --- Console : commandes
        {"Invalid answer: enter a number between {0} and {1}.", "Réponse invalide : entrez un nombre entre {0} et {1}."},
        {"Unknown colour: use the numbers 1 to 5.", "Couleur inconnue : utilisez les numéros 1 à 5."},
        {"Colours must be numbers (e.g. t 1 3 5).", "Les couleurs doivent être des numéros (ex : j 1 3 5)."},
        {"No reserved cards in a quick game.", "Pas de cartes réservées en partie rapide."},
        {"Usage: b r <n>, for example b r 1.", "Usage : a r <n>, par exemple a r 1."},
        {"Usage: b <level> <position>, for example b 1 3.", "Usage : a <niveau> <position>, par exemple a 1 3."},
        {"Usage: r <level> <position> or r <level> d, for example r 2 4.",
         "Usage : r <niveau> <position> ou r <niveau> p, par exemple r 2 4."},
        {"{0}, your action: ", "{0}, votre action : "},
        {"Game saved in {0} (resume with --load {0}).", "Partie sauvegardée dans {0} (reprendre avec --charger {0})."},
        {"Unknown command (type ? for help).", "Commande inconnue (tapez ? pour l'aide)."},
        {"Error: {0}", "Erreur : {0}"},
        {"Token to discard (1-5, 6 = Gold): ", "Jeton à défausser (1-5, 6 = Or) : "},
        {"Colour to discard (1-5): ", "Couleur à défausser (1-5) : "},
        {"More than 10 tokens, {0} more to discard: ", "Plus de 10 jetons, encore {0} à défausser : "},
        {"Several nobles want to visit you:", "Plusieurs nobles veulent vous rendre visite :"},
        {"Which one do you choose? ", "Lequel choisissez-vous ? "},
        {"\nPress Enter to quit...", "\nAppuyez sur Entrée pour quitter..."},
        {"  1) Quick game   (no gold, no reservations)\n  2) Classic game (gold and reservations)\n",
         "  1) Partie rapide   (sans or ni réservation)\n  2) Partie classique (avec or et réservation)\n"},
        {"Game mode (1-2): ", "Mode de jeu (1-2) : "},
        {"Number of players (2-4): ", "Nombre de joueurs (2-4) : "},
        {"Name of player {0}: ", "Nom du joueur {0} : "},
        {"  Played by: Enter = human, e = easy computer, m = medium computer: ",
         "  Joué par : Entrée = humain, f = ordinateur facile, m = ordinateur moyen : "},
        {"Game loaded from {0}.", "Partie chargée depuis {0}."},
        {"Computer error: {0}", "Erreur de l'ordinateur : {0}"},
        {"Game abandoned.", "Partie abandonnée."},
    };
    return table;
}

std::string tr(Lang lang, const std::string& english)
{
    if (lang == Lang::English) {
        return english;
    }
    const std::map<std::string, std::string>& table = french_table();
    auto found = table.find(english);
    return found == table.end() ? english : found->second;
}

std::string tr(Lang lang, const std::string& english, const std::vector<std::string>& values)
{
    std::string text = tr(lang, english);
    for (int i = 0; i < static_cast<int>(values.size()); i++) {
        std::string mark = "{" + std::to_string(i) + "}";
        for (size_t at = text.find(mark); at != std::string::npos; at = text.find(mark, at + values[i].size())) {
            text.replace(at, mark.size(), values[i]);
        }
    }
    return text;
}

Lang lang_from_code(const std::string& code)
{
    return code.compare(0, 2, "fr") == 0 || code.compare(0, 2, "FR") == 0 ? Lang::French : Lang::English;
}

std::string lang_code(Lang lang)
{
    return lang == Lang::French ? "fr" : "en";
}

Lang system_lang()
{
    const char* names[] = {"LANGUAGE", "LC_ALL", "LC_MESSAGES", "LANG"};
    for (const char* name : names) {
        const char* value = std::getenv(name);
        if (value != nullptr && value[0] != '\0' && std::string(value) != "C" && std::string(value) != "POSIX") {
            return lang_from_code(value);
        }
    }
    return Lang::English;
}
