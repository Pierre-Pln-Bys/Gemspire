# Changelog

Toutes les évolutions notables du projet. Format inspiré de
[Keep a Changelog](https://keepachangelog.com/fr/1.1.0/), versions selon
[SemVer](https://semver.org/lang/fr/).

## [Non publié]

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
