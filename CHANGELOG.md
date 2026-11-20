# Changelog

Toutes les évolutions notables du projet. Format inspiré de
[Keep a Changelog](https://keepachangelog.com/fr/1.1.0/), versions selon
[SemVer](https://semver.org/lang/fr/).

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
