# Roadmap vers la v3

Point de départ : **v2.1.0** (20/11/2026). Le jeu est complet et jouable en
console et en fenêtre (GTK), en mode rapide et classique, avec publication
automatique des exécutables aux versions majeures.

Ce qui manque encore pour une v3 « complète » :

- **Pas de version graphique pour Windows.** GTK impose des dizaines de DLL, il
  n'existe donc qu'un `.exe` console.
- **La version graphique Linux dépend de GTK 3** installé sur la machine.
- **On ne peut jouer qu'à plusieurs, sur le même écran.** Il n'y a ni
  adversaire contrôlé par l'ordinateur ni sauvegarde.
- **L'enchaînement d'un tour est écrit deux fois.** On le trouve dans `main.cpp`
  (défausse, nobles) et dans `gui_main.cpp` (`next_turn`, `after_discard`), et
  il n'est pas testé.
- **La CI ne tourne qu'aux releases.** Un commit qui casse la compilation
  Windows n'est vu qu'au moment de publier.

## Définition de « v3 complète »

La v3.0.0 est prête quand tout ceci est vrai :

1. Un **exécutable graphique unique** se télécharge et se lance, sans rien
   installer, sous Windows **et** sous Linux.
2. On peut jouer **seul contre l'ordinateur** (1 à 3 adversaires).
3. Une partie peut être **sauvegardée et reprise**.
4. Les règles sont **testées automatiquement à chaque push**, sur Linux et
   Windows.
5. La console et l'interface graphique partagent **le même code de
   déroulement du tour**.

---

## v2.2 — Fondations (qualité, sans changement visible)

Objectif : rendre les étapes suivantes sûres.

- [ ] **CI à chaque push et pull request** : compilation console + GUI sous
  Linux, console sous Windows, `make test`. Un échec bloque la fusion.
- [ ] **Mettre à jour les actions GitHub** (`checkout`, `upload-artifact`,
  `download-artifact`, `action-gh-release`) : avertissement « Node.js 20
  obsolète » vu sur les releases 2.0.0 et 2.1.0.
- [ ] **Extraire le déroulement du tour** dans la logique (`game.cpp` ou un
  nouveau `turn.cpp`) : après l'action → défausse obligatoire → visite de
  noble (choix si plusieurs) → joueur suivant, avec les passes automatiques.
  `main.cpp` et `gui_main.cpp` ne font plus qu'afficher et demander.
- [ ] **Tests de ce déroulement** : défausse avec or, deux nobles possibles,
  passes en chaîne, partie bloquée, dernier tour à 15 points.
- [ ] **Tests de l'interface graphique sans écran** : rendu Cairo dans un PNG
  (déjà utilisé à la main) pour vérifier qu'un plateau se dessine sans
  planter, dans chaque phase.

## v2.3 — Confort de jeu

- [ ] **Historique des actions** (panneau ou ligne déroulante) : « Alice prend
  Blanc, Vert, Rouge », « Bob achète une carte Bleu (2 pts) »…
- [ ] **Survol** dans la fenêtre : afficher ce qu'il manque pour acheter une
  carte (« il manque 2 Rouge »), et quel noble est proche.
- [ ] **Raccourcis clavier** dans la fenêtre (1 à 5 pour les jetons, Entrée
  pour valider) en plus de la souris.
- [ ] **Écran entre deux joueurs** (« Au tour de Bob — cliquez pour
  continuer ») pour ne pas voir les cartes réservées cachées des autres
  quand on joue sur le même écran.
- [ ] **Couleurs de joueurs dans la console** (nom et tableau), comme dans la
  fenêtre.
- [ ] **Accessibilité daltonisme** : un symbole différent par gemme (en plus
  de la couleur) sur les jetons et les cartes.

## v2.4 — Jouer contre l'ordinateur

- [ ] **Adversaire « facile »** : achète la carte la plus chère possible,
  sinon prend les jetons qui le rapprochent d'une carte visible.
- [ ] **Adversaire « moyen »** : vise les nobles et les cartes à points, et
  réserve (en classique) les cartes qu'un autre joueur va acheter.
- [ ] Choix humain / ordinateur pour chaque joueur à la création de la
  partie, dans la console comme dans la fenêtre. Petite pause visible entre
  les coups de l'ordinateur dans la fenêtre.
- [ ] **Tournoi automatique** dans les tests : 1 000 parties ordinateur
  contre ordinateur. Le « moyen » doit gagner nettement plus souvent que le
  « facile », et aucune partie ne doit planter ni perdre de jeton.

## v2.5 — Sauvegarde

- [ ] Format texte simple et lisible (`.splendor`) : mode, graine, joueurs,
  pioches, plateau, réserve, mains.
- [ ] Console : commandes `s` (sauvegarder) et `--charger fichier`. Fenêtre :
  menu Sauvegarder / Ouvrir.
- [ ] Tests : sauvegarder puis recharger donne exactement la même partie.

## v3.0 — Interface graphique portable (version majeure)

C'est le changement qui justifie le passage à la v3 : il remplace la
bibliothèque graphique.

- [ ] **Remplacer GTK par une bibliothèque liable statiquement**, pour obtenir
  un seul fichier exécutable par système. Décision à prendre :

  | Option | Pour | Contre |
  |---|---|---|
  | **raylib** (recommandée) | très simple, pensée pour les jeux, un seul `.exe` facile | moins de widgets (le dialogue de nouvelle partie est à redessiner) |
  | SDL2 + SDL_ttf | très répandue, un seul `.exe` possible | plus de code pour le texte et les formes |
  | Garder GTK + AppImage (Linux) / installateur (Windows) | aucun code à réécrire | sous Windows, ce n'est plus « juste l'exécutable » |

  Comme le placement (`*_rect`) et l'état (`App`) sont déjà séparés du
  dessin, seuls `gui_draw.cpp` et `gui_main.cpp` changent.
- [ ] **Release** : `splendor-windows.exe` (graphique), `splendor-linux`
  (graphique), plus les versions console. Tous autonomes.
- [ ] **Petites animations** : jeton qui glisse vers la main, carte qui se
  retourne quand elle remplace une carte achetée.
- [ ] Mettre à jour README, CLAUDE.md, CHANGELOG, puis tag `v3.0.0` : le
  workflow publie les exécutables.

## Plus tard (après la v3, si le temps le permet)

- Partie en réseau local (deux ordinateurs).
- Version macOS.
- Extensions de règles (score de victoire réglable, variantes).
- Statistiques de fin de partie (jetons pris, cartes par niveau…).

## Questions à trancher

- **Bibliothèque graphique de la v3** : raylib, SDL2 ou garder GTK
  (voir le tableau ci-dessus).
- **Les adversaires ordinateur passent-ils avant la sauvegarde ?** (ordre
  v2.4 / v2.5 interchangeable).
- **Version console** : est-elle conservée en v3 à côté de la version
  graphique, ou devient-elle un simple mode de débogage ?
