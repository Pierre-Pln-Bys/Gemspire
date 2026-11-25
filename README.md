# Projet des goats

Swann PROUTEAU

Gemspire en C++17, en version graphique (fenêtre) ou dans le terminal,
avec deux modes :

- **partie rapide** : sans jetons or ni réservation ;
- **partie classique** : règles complètes, avec or et réservation.

## Télécharger

Chaque version est publiée sur la page
[Releases](https://github.com/Pierre-Pln-Bys/Gemspire/releases), avec
les exécutables à télécharger directement (pas de zip, rien à installer) :

| Fichier | Système | Contenu |
|---|---|---|
| `gemspire-windows.exe` | Windows | **jeu graphique**, double-cliquer pour jouer |
| `gemspire-linux` | Linux x86_64 | **jeu graphique** |
| `gemspire-console-windows.exe` | Windows | version console |
| `gemspire-console-linux` | Linux x86_64 | version console |

Sous Linux, rendre le fichier exécutable une fois :

```bash
chmod +x gemspire-linux
```

## Compilation

raylib (bibliothèque graphique) est incluse dans `third_party/` et compilée
automatiquement au premier `make`. Sous Linux, il faut les en-têtes X11 et
OpenGL :

```bash
sudo apt install build-essential libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev
```

```bash
make            # construit ./splendor-gui (fenêtre) et ./splendor (console)
make splendor   # version console seule (aucune dépendance)
make test       # lance les tests de la logique
make splendor-server test-server   # serveur en ligne et son test (Linux, libssl-dev)
make test-discord test-bot         # Rich Presence (faux Discord) et bot Python
make clean      # make clean-all supprime aussi la raylib compilée
```

## Version graphique

```bash
./splendor-gui
```

- **Langue** : anglais ou français, selon la langue du système (ou du
  navigateur) au premier lancement ; bouton « Français » / « English » en haut
  à droite de l'accueil (choix gardé dans `langue.txt`).
- **Accueil** : nouvelle partie, jouer en ligne, reprise de la sauvegarde,
  quitter. En haut à gauche : **Règles** (les règles en bref) et **Thème** du
  plateau (Tapis vert, Nuit, Bois, Contraste élevé, Clair ; choix gardé dans
  `theme.txt`).
- **Plein écran** : `F11` (fenêtre sans bord ; dans le navigateur, F11 du
  navigateur). Le dessin s'adapte à la taille réelle de l'écran (net aussi sur
  les écrans Retina et sous Windows agrandi à 125 % ou 150 %).
- **Nouvelle partie** : mode, nombre de joueurs, puis pour chacun son nom, sa
  couleur (Violet, Orange, Turquoise, Rose, Marron ; Purple, Orange, Turquoise, Pink, Brown en anglais) et qui le joue (humain,
  ordinateur facile ou moyen).
- **Plateau** : cliquer des jetons de la réserve puis « Prendre », ou une
  carte (ou une pioche) puis « Acheter » / « Réserver ». Contour doré = carte
  achetable, contour blanc = sélection. Le survol d'une carte indique ce
  qu'il manque. Pour défausser, cliquer ses propres jetons.
- **Clavier** : `1` à `5` choisissent les jetons, Entrée valide, `R` réserve,
  Échap annule, `J` ouvre le journal, Ctrl+S sauvegarde.
- **Sauvegarde** : bouton « Sauvegarder » (fichier `partie.splendor`) ;
  « Menu » sauvegarde aussi la partie en cours avant de revenir à l'accueil.
- En partie classique avec plusieurs humains, un écran de passage cache les
  cartes réservées entre deux joueurs.
- Chaque gemme a son symbole (cercle, losange, triangle, carré, hexagone, étoile
  pour l'or) en plus de sa couleur.

## Jouer dans le navigateur (ordinateur, téléphone)

Sans rien installer : https://gemspire.epsilon-forge.fr/play/ (téléphone tenu
à l'horizontale). Même jeu que la version à télécharger (même code C++,
compilé en WebAssembly avec Emscripten), sans la Rich Presence Discord.

La même page sert d'**Activité Discord** : lancée depuis un salon vocal, elle
connecte le joueur avec son compte Discord sans autre étape (réglages du
portail Discord : voir `server/README.md`).

Compiler soi-même (Emscripten installé, `emcc` dans le PATH) :

```bash
make web   # résultat dans build/web (index.html, index.js, index.wasm)
```

## Jouer en ligne

Depuis l'accueil, « Jouer en ligne » :

1. **Connexion** avec Discord ou Google : le navigateur s'ouvre, aucun mot de
   passe n'est demandé ni stocké par le jeu. Au premier login, on choisit un
   **pseudo** unique.
2. **Créer un salon** (mode, 2 à 4 joueurs) : un code de 5 lettres s'affiche,
   à donner à ses amis (bouton « Copier le code »). Ou **rejoindre** avec un
   code. Les parties en salon (et les défis entre amis) sont **amicales** :
   l'Elo ne change pas.
3. L'hôte lance la partie. Le serveur arbitre chaque coup ; chacun ne voit
   que ses propres cartes réservées.
4. Chaque tour dure au plus **2 minutes** (temps affiché en haut à gauche) ;
   ensuite le tour passe. **3 tours manqués d'affilée** ou le bouton
   « Abandonner » (deux clics) : le joueur est classé dernier (il perd de
   l'Elo dans une partie classée), les autres continuent.
5. **Trouver un adversaire** (partie à 2, Elo proche), deux files :
   - **Classée** : change l'**Elo global**, jamais remis à zéro ;
   - **Saison** : change l'**Elo de saison**, qui repart à 1000 à chaque
     saison. Une saison dure deux mois et change toute seule le 1er janvier,
     mars, mai, juillet, septembre et novembre à minuit (heure de Paris). Il
     faut 10 parties de saison pour être classé ; le top 10 gagne un badge.

   Les deux **classements** (onglets Saison et Global) sont visibles depuis le
   menu en ligne et sur
   [gemspire.epsilon-forge.fr/classement](https://gemspire.epsilon-forge.fr/classement).
6. **Chat** pendant la partie (bouton « Chat ») : messages courts, réactions
   rapides, sourdine d'un joueur. **Amis** (menu en ligne) : ajout par
   pseudo, statut en ligne / en partie, **Défier** un ami connecté (partie
   amicale à 2). **Succès** : 11 à débloquer, liste dans le menu en ligne.
7. **Lier Google / Discord** (menu en ligne) : ajoute l'autre mode de
   connexion au même compte (même pseudo, même Elo).
8. **Spectateurs et rediffusions** : « En cours » pour regarder une partie,
   « Mes parties » pour revoir les siennes coup par coup.

**Discord** :

- **Rich Presence** : votre profil Discord affiche ce que vous faites
  (« Partie en ligne (classique) — Tour 7 · 12 pts »). Dans un salon, vos amis
  peuvent cliquer **Rejoindre** directement depuis Discord.
- **Inviter** : dans le salon, « Copier une invitation » met dans le
  presse-papiers un message prêt à coller (code + lien de téléchargement).
- **Salon vocal « Crée ta partie »** : en le rejoignant, le bot Discord vous
  crée un salon vocal temporaire ; si Gemspire est ouvert (connecté avec
  Discord), le jeu propose de créer la partie, et le salon vocal est limité au
  nombre de places.
- **Bot** : `/challenge @membre` crée un salon à 2 places (partie amicale) et
  prévient le membre défié ; le salon du classement montre le top 10 de la
  saison et le top 10 global, et annonce chaque fin de saison ; rôles de rang
  selon l'Elo global (Pearl, Ruby, Sapphire, Emerald,
  Diamond). Les administrateurs ont `/dev` (état du serveur, inscriptions,
  pause des parties, annonce, gestion d'un joueur) et le panel web
  `/panel-admin` (voir [server/README.md](server/README.md)).

Par défaut, le jeu se connecte au serveur officiel
`https://gemspire.epsilon-forge.fr`. Pour un autre serveur (par exemple
`http://localhost:8080` en développement) : variable `SPLENDOR_SERVER` ou
fichier `serveur.txt` à côté du jeu. Le serveur est décrit dans
[server/README.md](server/README.md), le bot dans `bot/`, l'installation sur
le Raspberry Pi dans [deploy/README.md](deploy/README.md).

## Version console

```bash
./splendor                  # le mode est demandé au lancement
./splendor --quick          # partie rapide (ou --rapide)
./splendor --classic 42     # partie classique, graine fixe (ou --classique)
./splendor --version        # affiche la version
```

Langue : français si la variable `LANG` (ou `LANGUAGE`, `LC_ALL`) commence par
`fr`, anglais sinon (`LANG=fr_FR.UTF-8 ./splendor`).

Sauvegarde : `s` (ou `s fichier.splendor`) pendant la partie, puis
`./splendor --load partie.splendor` (ou `--charger`) pour la reprendre (même format que la
version graphique).

Pour chaque joueur, on choisit aussi qui le joue : humain (Entrée),
ordinateur facile (`e`, ou `f` en français) ou ordinateur moyen (`m`).

Le plateau tient dans un terminal de 80 colonnes. Variable `NO_COLOR=1` pour
désactiver les couleurs.

## Commandes

Couleurs : 1=Blanc 2=Bleu 3=Vert 4=Rouge 5=Noir (6=Or, seulement pour défausser).
Les commandes anglaises (`t` = take, `b` = buy, `r 3 d` = deck) et
françaises (`j`, `a`, `r 3 p`) marchent dans les deux langues.

| Commande  | Effet                                                    |
|-----------|----------------------------------------------------------|
| `j 1 3 5` | prendre 3 jetons de couleurs différentes                 |
| `j 2 2`   | prendre 2 jetons de la même couleur (pile d'au moins 4)  |
| `a 2 4`   | acheter la carte 4 du niveau 2                           |
| `a r 1`   | acheter sa carte réservée R1 (classique)                 |
| `r 2 4`   | réserver la carte 4 du niveau 2, +1 or (classique)       |
| `r 3 p`   | réserver le dessus de la pioche du niveau 3 (classique)  |
| `?`       | aide                                                     |
| `q`       | quitter                                                  |

Les cartes achetables par le joueur courant sont marquées d'une `*`.

## Règles implémentées

- 2 à 4 joueurs ; 4, 5 ou 7 jetons par couleur selon le nombre de joueurs.
- 4 cartes visibles par niveau, remplacées par la pioche après achat ou réservation.
- Nobles : nombre de joueurs + 1, 3 points, coûtent des bonus.
- Une action par tour : 3 jetons différents (moins s'il reste moins de
  couleurs), 2 jetons identiques (pile ≥ 4), acheter une carte (coût réduit
  par les bonus), et en classique : réserver une carte.
- Classique : 5 jetons or ; réserver donne 1 or (s'il en reste), 3 réservations
  au maximum ; l'or remplace n'importe quel jeton manquant.
- Maximum 10 jetons (or compris) : défausse obligatoire au-delà.
- Un noble au plus par tour, au choix du joueur si plusieurs.
- Un joueur qui ne peut rien faire passe ; si tout le monde passe d'affilée,
  la partie s'arrête.
- Fin : dès 15 points, on finit le tour de table. Gagnant : plus de points,
  puis le moins de cartes ; sinon égalité.

## Publier une version

1. Mettre à jour `src/version.hpp` et `CHANGELOG.md`, commiter sur `main`.
2. Créer le tag et le pousser :

```bash
git tag -a v4.0.0 -m "Version 4.0.0"
```

```bash
git push origin main v4.0.0
```

Un tag majeur (`vX.0.0`) lance le workflow qui publie les exécutables. Pour
une version mineure : `gh workflow run release.yml -f tag=vX.Y.Z`.

## Suite du projet

Voir [ROADMAP.md](ROADMAP.md) : v3.5 (mobile), v4
(publication et test d'intrusion), v5 (communauté).

Documentation complète : [wiki](https://github.com/Pierre-Pln-Bys/Gemspire/wiki).

## Licence

© 2026 Gemspire. **Tous droits réservés** (fichier [LICENSE](LICENSE)). Le
jeu se télécharge et se joue gratuitement pour un usage personnel ; toute
copie, redistribution, modification ou revente du code ou des graphismes est
interdite sans autorisation écrite. Les versions publiées avant la 4.0
restent sous la licence GPL-3.0 avec laquelle elles ont été distribuées.

Les bibliothèques de `third_party/` gardent leur propre licence, jointe dans
leur dossier (raylib : zlib ; cpp-httplib, nlohmann/json : MIT ; SQLite :
domaine public ; polices DejaVu : licence Bitstream Vera).
