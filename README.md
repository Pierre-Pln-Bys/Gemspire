# Projet des goats

Swann PROUTEAU

Splendor en C++17, en version graphique (fenêtre) ou dans le terminal,
avec deux modes :

- **partie rapide** : sans jetons or ni réservation ;
- **partie classique** : règles complètes, avec or et réservation.

## Télécharger

Chaque version majeure est publiée sur la page
[Releases](https://github.com/Pierre-Pln-Bys/ProjectSplendor/releases), avec
les exécutables à télécharger directement (pas de zip) :

| Fichier               | Système | Remarque                                      |
|-----------------------|---------|-----------------------------------------------|
| `splendor-windows.exe`| Windows | version console, double-cliquer pour jouer   |
| `splendor-linux`      | Linux   | version console, aucune dépendance            |
| `splendor-gui-linux`  | Linux   | version graphique, nécessite GTK 3 (présent sur la plupart des bureaux) |

Sous Linux, rendre le fichier exécutable une fois :

```bash
chmod +x splendor-gui-linux
```

## Compilation

```bash
sudo apt install libgtk-3-dev   # uniquement pour la version graphique
make                            # construit ./splendor et ./splendor-gui
make splendor                   # version console seule
make test                       # lance les tests de la logique
make clean
```

## Version graphique

```bash
./splendor-gui
```

Tout se joue à la souris : cliquer des jetons de la réserve puis « Prendre »,
ou une carte (ou une pioche) puis « Acheter » / « Réserver ». Contour doré =
carte achetable, contour blanc = sélection. Chaque joueur choisit sa couleur à
la création de la partie : son panneau en est entouré pendant son tour, et ses
cartes réservées en prennent le bord. Pour défausser, cliquer ses propres jetons. Échap annule la
sélection.

## Version console

```bash
./splendor                  # le mode est demandé au lancement
./splendor --rapide         # partie rapide
./splendor --classique 42   # partie classique, graine fixe (rejouer la même partie)
./splendor --version        # affiche la version
```

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
git tag -a v2.0.0 -m "Version 2.0.0"
```

```bash
git push origin main v2.0.0
```

Un tag majeur (`vX.0.0`) lance le workflow qui publie les exécutables.

## Suite du projet

Voir [ROADMAP.md](ROADMAP.md) pour les étapes prévues jusqu'à la v3.

Documentation complète : [wiki](https://github.com/Pierre-Pln-Bys/ProjectSplendor/wiki).
