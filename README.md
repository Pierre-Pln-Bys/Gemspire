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

- **Accueil** : nouvelle partie, reprise de la sauvegarde, quitter.
- **Nouvelle partie** : mode, nombre de joueurs, puis pour chacun son nom, sa
  couleur (Violet, Orange, Turquoise, Rose, Marron) et qui le joue (humain,
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
   code.
3. L'hôte lance la partie. Le serveur arbitre chaque coup ; chacun ne voit
   que ses propres cartes réservées.
4. Chaque tour dure au plus **2 minutes** (temps affiché en haut à gauche) ;
   ensuite le tour passe. **3 tours manqués d'affilée** ou le bouton
   « Abandonner » (deux clics) : le joueur est classé dernier et perd de l'Elo,
   les autres continuent.
5. En fin de partie, l'**Elo** de chaque joueur change ; le **classement**
   est visible depuis le menu en ligne et sur
   [gemspire.epsilon-forge.fr/classement](https://gemspire.epsilon-forge.fr/classement).

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

Par défaut, le jeu se connecte au serveur officiel
`https://gemspire.epsilon-forge.fr`. Pour un autre serveur (par exemple
`http://localhost:8080` en développement) : variable `SPLENDOR_SERVER` ou
fichier `serveur.txt` à côté du jeu. Le serveur est décrit dans
[server/README.md](server/README.md), le bot dans `bot/`, l'installation sur
le Raspberry Pi dans [deploy/README.md](deploy/README.md).

## Version console

```bash
./splendor                  # le mode est demandé au lancement
./splendor --rapide         # partie rapide
./splendor --classique 42   # partie classique, graine fixe (rejouer la même partie)
./splendor --version        # affiche la version
```

Sauvegarde : `s` (ou `s fichier.splendor`) pendant la partie, puis
`./splendor --charger partie.splendor` pour la reprendre (même format que la
version graphique).

Pour chaque joueur, on choisit aussi qui le joue : humain (Entrée),
ordinateur facile (`f`) ou ordinateur moyen (`m`).

Le plateau tient dans un terminal de 80 colonnes. Variable `NO_COLOR=1` pour
désactiver les couleurs.

## Commandes

Couleurs : 1=Blanc 2=Bleu 3=Vert 4=Rouge 5=Noir (6=Or, seulement pour défausser)

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

Gemspire est distribué sous licence **GPL-3.0** (fichier [LICENSE](LICENSE)) :
chacun peut l'utiliser, l'étudier, le modifier et le redistribuer, à
condition que les versions modifiées restent sous GPL-3.0 et publient leur
code. Les bibliothèques de `third_party/` gardent leur propre licence
(raylib : zlib ; cpp-httplib, nlohmann/json : MIT ; SQLite : domaine public),
toutes compatibles.
