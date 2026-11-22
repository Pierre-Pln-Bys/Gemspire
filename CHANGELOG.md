# Changelog

Toutes les évolutions notables du projet. Format inspiré de
[Keep a Changelog](https://keepachangelog.com/fr/1.1.0/), versions selon
[SemVer](https://semver.org/lang/fr/).

## [3.6.0] - 2026-11-22

### Modifié
- **Le jeu s'appelle désormais Gemspire** : titre, site
  (https://gemspire.epsilon-forge.fr, l'ancienne adresse redirige), bot,
  exécutables (`gemspire-windows.exe`, `gemspire-linux`...), dépôt GitHub
  `Pierre-Pln-Bys/Gemspire`. Les réglages et services du serveur gardent
  leurs noms techniques (`SPLENDOR_*`, `splendor-server`).
- **Licence GPL-3.0**.

### Ajouté
- **Changelog sur Discord** : chaque release publie ses nouveautés dans un
  salon Discord (`tools/discord_release.py`, secret GitHub
  `DISCORD_RELEASE_WEBHOOK`).
- Bot : salons et rôles réglables par **identifiant** (`CREATE_CHANNEL_ID`,
  `RANKING_CHANNEL_ID`, `MOD_LOG_CHANNEL_ID`, `MOD_ROLE_ID`, `PLAYER_ROLE_ID`),
  les noms restant le choix par défaut.

## [3.5.0] - 2026-11-22

### Ajouté
- **Jouer dans le navigateur**, sur ordinateur et téléphone :
  https://splendor.epsilon-forge.fr/play/. Même code C++ compilé en
  WebAssembly (Emscripten, `make web`) : réseau par `fetch`, saisie par la
  boîte du navigateur (clavier du téléphone), session et sauvegarde gardées
  dans le navigateur, connexion Discord / Google dans le même onglet (retour
  automatique au jeu). Bouton « Jouer dans le navigateur » sur le site.
- **Site en anglais et en français** (langue du navigateur ou `?lang=`),
  adresses `/leaderboard`, `/terms`, `/privacy` (les françaises restent).
- **Bot en anglais**, traduit en français pour les membres dont Discord est
  en français : `/leaderboard`, `/profile`, `/link`, `/play`, `/rules`,
  `/report`, `/mod ban|unban|history|cancel-game|delete-account`.
- Release : archive `splendor-web.tar.gz`, installée par `deploy/install.sh`.
  CI : compilation web.

### Corrigé
- Le temps restant du tour était caché sous les tuiles des nobles.

## [3.4.1] - 2026-11-22

### Ajouté
- **Version obligatoire** : le jeu envoie sa version au serveur ; une version
  trop ancienne (ou sans version) est refusée et le jeu affiche l'écran
  bloquant « Mise à jour nécessaire » (bouton de téléchargement, partie
  locale en cours sauvegardée). Version minimale : `SPLENDOR_MIN_VERSION`,
  par défaut celle du serveur. Les versions jusqu'à 3.4.0 ne peuvent donc
  plus jouer en ligne une fois le serveur mis à jour.

## [3.4.0] - 2026-11-22

### Ajouté
- **Bot Discord, hub du jeu** :
  - salon `#classement` : top 10 mis à jour toutes les 10 minutes ;
  - commandes `/classement`, `/profil`, `/lier` (rôle Joueur), `/jouer`,
    `/regles`, `/signaler` ;
  - modération `/mod ban` (suspension du compte de jeu, 0 jour = définitif),
    `/mod unban`, `/mod historique` (alerte multi-comptes), `/mod
    annuler-partie`, `/mod supprimer-compte` ; journal dans `#moderation`.
- Serveur : comptes suspendus (connexion refusée, sessions fermées, parties
  abandonnées, absents du classement), historique des parties, annulation
  d'une partie, suppression de compte, routes `/bot/profile`,
  `/bot/history`, `/bot/ban`, `/bot/unban`, `/bot/cancel-game`,
  `/bot/delete-account`. Base migrée automatiquement.
- Wiki « Serveur Discord » : rôles, salons et réglages conseillés.

### Modifié
- Le bot garde aussi l'identifiant du message du classement dans son fichier
  d'état (l'ancien format est encore lu).
- Lien d'invitation du bot : scope `applications.commands` et droits
  « gérer les rôles », « intégrer des liens », « lire l'historique ».

## [3.3.0] - 2026-11-22

### Ajouté
- **Temps de jeu en ligne** : 2 minutes par tour (compte à rebours affiché).
  Passé ce délai le tour passe ; après 3 tours manqués d'affilée le joueur
  abandonne. Bouton **Abandonner** (deux clics). Celui qui abandonne est
  classé dernier et perd de l'Elo ; la partie continue entre les autres.
  Avant, quitter le jeu bloquait la partie et évitait la défaite.
- **Site** https://splendor.epsilon-forge.fr : accueil et téléchargements,
  **classement** public (`/classement`), **conditions d'utilisation** et
  **politique de confidentialité**. HTML sans JavaScript, lisible sur mobile.
- Logique : `Game::skip_turn`, `Game::forfeit` ; la sauvegarde garde les
  abandons (ligne facultative `forfeited`).

### Corrigé
- Sélection des jetons : impossible de choisir des combinaisons interdites
  (2 identiques dans une pile de moins de 4, plus de 3 couleurs).
- Annonces bien visibles quand un noble rend visite à un joueur et quand le
  dernier tour commence.
- Le serveur oublie les parties terminées depuis plus d'une heure (mémoire).
- Test du serveur instable (pioche de niveau I parfois vide en fin de partie).

## [3.2.2] - 2026-11-22

### Corrigé
- Windows : la version graphique ne se compilait plus (l'OpenSSL récent de
  MSYS2 demande la bibliothèque `bcrypt`). La 3.2.1 n'a donc pas été publiée ;
  la 3.2.2 contient tous ses changements.

### Ajouté
- Image de couverture des invitations Discord (`assets/invite-cover.png`).

## [3.2.1] - 2026-11-22 (non publiée)

Première version reliée au serveur officiel, après les essais réels.

### Modifié
- Le jeu se connecte par défaut au serveur officiel
  `https://splendor.epsilon-forge.fr` (Raspberry Pi derrière un tunnel
  Cloudflare). `serveur.txt` ou `SPLENDOR_SERVER` visent un autre serveur.
- Rich Presence : logo du jeu et textes plus clairs (pseudo et Elo).
- Bot : un salon temporaire vide est supprimé après 30 secondes ; la liste
  des salons est gardée dans un fichier, pour s'en souvenir après un
  redémarrage.
- Déploiement : port 8090 (8080 souvent déjà pris), identifiants publics et
  adresse du projet préremplis.

### Corrigé
- Le jeu plantait au démarrage quand Discord était ouvert (réponse
  `"evt": null` de Discord).
- « Valider » (pseudo), « Créer le salon », « Rejoindre » et « Lancer la
  partie » restaient grisés une fois connecté (attentes longues comptées
  comme des requêtes en cours).
- Serveur : un échec du webhook Discord est écrit dans le journal.
- `install.sh` attend que le serveur réponde avant de tester `/health`.

## [3.2.0] - 2026-11-20

### Ajouté
- **Rich Presence Discord** : le profil affiche le menu, la partie (mode,
  tour, points) ou le salon (places) ; « Rejoindre » depuis Discord ouvre le
  salon dans le jeu. Passe par l'IPC local de Discord (Linux et Windows),
  sans SDK.
- **Invitations** : bouton « Copier une invitation » dans le salon.
- **Bot Discord « Crée ta partie »** (Python, discord.py) : salon vocal
  temporaire, proposition de partie dans le jeu si Splendor est ouvert,
  salon limité au nombre de places, supprimé quand il est vide.
- Serveur : présence des joueurs, notifications du jeu, `/config`, API du
  bot protégée par clé, salons liés à un salon vocal, annonces des nouveaux
  salons par webhook Discord.
- **Déploiement** sur le Raspberry Pi : `deploy/install.sh`, services
  systemd durcis, modèle de réglages.
- Tests : faux Discord local (`make test-discord`), bot et intégration
  avec le serveur (`make test-bot`).

## [3.1.0] - 2026-11-20

### Ajouté
- **Jeu en ligne** : connexion avec Discord ou Google (sans mot de passe),
  choix d'un pseudo unique, salons avec code, parties arbitrées par le
  serveur, classement **Elo** et leaderboard.
- `splendor-server` (C++ : cpp-httplib, SQLite, OpenSSL) qui réutilise la
  logique du jeu ; chaque joueur ne reçoit que ce qu'il a le droit de voir
  (pioches et réservées des autres cachées) ; jetons de session stockés en
  empreinte SHA-256.
- Écrans connexion, pseudo, menu en ligne, salon, classement ; résultats
  avec variation d'Elo.
- `make test-server` : deux joueurs jouent une partie complète en ligne
  (coups invalides refusés, Elo, classement, informations cachées).

### Modifié
- Format de coup commun (`Move`) pour la fenêtre, l'IA et le serveur ;
  l'IA choisit un coup (`ai_choose`) sans le jouer.
- La version graphique Linux utilise l'OpenSSL du système (libssl3).

## [3.0.0] - 2026-11-20

### Ajouté
- Nouvelle interface graphique en **raylib**, livrée en exécutable unique
  pour Windows (`splendor-windows.exe`) et Linux (`splendor-linux`), sans
  rien à installer.
- Écrans d'accueil et de nouvelle partie dessinés dans le jeu (nom, couleur
  et type de chaque joueur).
- Journal de la partie (`J`), infobulles « il manque ... » au survol des
  cartes et des nobles, raccourcis clavier (1 à 5, Entrée, R, Échap,
  Ctrl+S).
- Écran de passage entre deux joueurs humains en partie classique.
- Symbole propre à chaque gemme (daltonisme).
- Animations : jetons qui glissent, cartes qui partent vers le joueur, carte
  qui se retourne en arrivant sur le plateau.
- Police DejaVu Sans intégrée (accents) ; bouton « Menu » qui sauvegarde la
  partie en cours.

### Modifié
- `enum class Color` devient `enum class Gem` (raylib a déjà un type
  `Color`).
- Releases : `splendor-windows.exe` / `splendor-linux` sont maintenant les
  versions graphiques ; les versions console s'appellent
  `splendor-console-*`.

### Supprimé
- Interface GTK 3 (remplacée par raylib) et dépendance à GTK.

## [2.4.0] - 2026-11-20

### Ajouté
- Sauvegarde et reprise d'une partie (fichiers texte `.splendor`) : commande
  `s` et option `--charger` dans la console, boutons « Sauvegarder » /
  « Ouvrir… » (Ctrl+S / Ctrl+O) dans la fenêtre.
- Tests : une partie rechargée se termine exactement comme l'originale ;
  les fichiers invalides sont refusés proprement.

## [2.3.0] - 2026-11-20

### Ajouté
- Adversaires ordinateur « facile » et « moyen », dans la console et dans la
  fenêtre (colonne « Joué par » ; l'ordinateur joue avec une courte pause).
- Tournoi de 1 000 parties entre ordinateurs dans les tests : aucune partie
  ne plante ni ne perd de jeton, et l'IA moyenne bat l'IA facile.

## [2.2.0] - 2026-11-20

### Ajouté
- CI : compilation et tests à chaque push et pull request, sous Linux et
  Windows.
- Historique des actions : chaque action (prise de jetons, achat,
  réservation, défausse, visite de noble, passe) est décrite dans la console
  et dans la ligne d'état de la fenêtre.
- Console : chaque joueur a sa couleur (nom, tableau des joueurs, résultats).
- Tests du déroulement du tour (défausse avec or, choix entre deux nobles,
  dernier tour, passes en chaîne, partie bloquée) et 300 parties complètes
  simulées.

### Modifié
- Le déroulement d'un tour (défausse, nobles, joueur suivant, passes) est
  géré par `Game` lui-même ; la console et la fenêtre partagent ce code.
- Actions GitHub mises à jour (plus d'avertissement Node.js 20).

## [2.1.0] - 2026-11-20

### Modifié
- La 5e couleur de ressource s'appelle maintenant Noir (au lieu de Marron),
  dans la console comme dans l'interface graphique.
- Interface graphique : cartes achetables entourées en doré, sélection en
  blanc.

### Ajouté
- Interface graphique : chaque joueur choisit sa couleur à la création de la
  partie ; son panneau en est entouré pendant son tour et ses cartes
  réservées en prennent le bord.
- Workflow de release : lancement manuel possible pour publier un tag déjà
  existant (onglet Actions ou `gh workflow run release.yml -f tag=vX.Y.Z`).

## [2.0.0] - 2026-11-20

### Ajouté
- Interface graphique `splendor-gui` (GTK 3 + Cairo) : plateau dessiné
  façon vraie table, jeu entièrement à la souris, dialogue de nouvelle
  partie, écran de fin avec « Rejouer ».
- Workflow GitHub Actions : à chaque version majeure (tag `vX.0.0`), les
  exécutables Windows (.exe) et Linux sont attachés à la release.
- Version console : couleurs et accents sous Windows, attente d'Entrée en fin
  de partie pour ne pas fermer la fenêtre.

## [1.0.0] - 2026-11-20

### Ajouté
- Jeu Splendor complet en C++17, dans le terminal.
- Deux modes : partie rapide (sans or ni réservation) et partie classique
  (jetons or, réservation de 3 cartes au maximum).
- Plateau dessiné comme la vraie table : tuiles des nobles, rangées de cartes
  avec leur pioche, réserve de jetons sur le côté, tableau des joueurs.
- Options `--rapide`, `--classique`, `--version` et graine de partie.
- Tests de la logique (`make test`).
- Arrêt de la partie si plus aucun joueur ne peut jouer.

### Supprimé
- Ancienne base C99 + ncurses (wrapper ncurses, exemples, splendor.h).
