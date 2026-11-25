# Passe de saison (v4.2)

Passe **gratuit pour tous**, sans piste payante. Toutes les récompenses sont
**cosmétiques** : aucune ne change les règles, l'IA, l'Elo ou la recherche de
partie.

## Fonctionnement

- Un passe par **saison** (deux mois, mêmes dates que les saisons classées).
- **50 paliers** de `PASS_XP_PER_TIER` = 700 XP ; une **récompense tous les
  2 paliers**, soit 25 objets par saison.
- Les objets d'une saison sont **générés** à partir du numéro de saison
  (couleur et nom d'une gemme, motif, symbole) : ils changent à chaque saison,
  ceux d'une saison passée restent exclusifs, et rien n'est à dessiner à la main.
  Code d'un objet : `s<saison>.<sorte>.<n>` (ex. `s1.theme.0`).
- Les objets gagnés **restent** après la fin de la saison ; l'XP repart de 0.

## Gain d'XP (calculé par le serveur à la fin d'une partie en ligne)

| Source | XP | Limite |
|---|---|---|
| Partie classée ou de saison | 100, +50 si victoire | — |
| Partie amicale | 50 | 5 par jour |
| Abandon / 3 tours manqués | 0 | — |
| Première partie en ligne du jour | +150 | 1 par jour (jour UTC) |
| Défi de la semaine | 400 à 600 | 3 défis, renouvelés le lundi à 0 h UTC |
| Parties contre l'ordinateur | 0 | non vérifiables par le serveur |

Défis possibles (3 tirés chaque semaine, les mêmes pour tous) : gagner des
parties de saison ou classées, terminer des parties en ligne, recevoir des
nobles, acheter des cartes d'une couleur, finir à 18 points, jouer une partie
amicale, gagner avec 2 nobles.

## Récompenses (dans l'ordre des paliers 2, 4, ... 50)

| Sorte | Par saison | Où elle se voit |
|---|---|---|
| Thème du plateau | 3 (paliers 10, 30, 50) | sur son écran (en ligne et hors ligne) |
| Dos de cartes | 2 | sur son écran |
| Jetons | 2 | sur son écran |
| Cadre | 4 | carte du compte, salons, panneaux en partie (vu par tous) |
| Titre | 5 | idem, et `/profile` sur Discord |
| Avatar | 4 | carte du compte, salons, panneaux en partie |
| Réaction du chat | 5 | boutons du chat en partie |

L'écran **Apparence** (depuis le passe) permet de porter un objet par sorte ou
de revenir à l'apparence normale. Thème, dos de cartes et jetons sont aussi
gardés dans `apparence.txt` pour jouer hors ligne.

## Où est le code

- `src/season_pass.*` : logique pure (catalogue, XP, défis), testée dans
  `tests/test_rules.cpp`.
- `server/store.cpp` : tables `pass_progress`, `cosmetics`, `equipped`,
  `challenge_progress` ; `add_game_to_pass` (appelé par `Rooms::finish_game`).
- Routes : `GET /me/pass`, `POST /me/equip`, `look` dans `/me`, les salons et
  les parties, `reaction_code` dans le chat.
- Jeu : écrans `Screen::Pass` et `Screen::Appearance` (gui_draw / gui_main).

Les valeurs (XP, limites, défis) sont des constantes de `season_pass.hpp` /
`season_pass.cpp`, faciles à régler après quelques semaines de jeu.
