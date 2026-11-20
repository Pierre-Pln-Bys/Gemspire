# Roadmap vers la v3

Point de départ : **v2.1.0** (20/11/2026). Le jeu est complet et jouable en
console et en fenêtre (GTK), en mode rapide et classique, à plusieurs sur le
même écran. Les exécutables sont publiés automatiquement.

Suivi des tâches : [GitHub Project « Splendor — Roadmap v3 »](https://github.com/users/Pierre-Pln-Bys/projects/2)
(une issue par case ci-dessous, un jalon GitHub par version).

## Ce que vise la v3

1. Un **exécutable graphique unique** se lance sans rien installer, sous
   Windows **et** sous Linux.
2. On peut jouer **seul contre l'ordinateur**, ou **en ligne** contre
   d'autres joueurs.
3. **Comptes sans mot de passe** (Google ou Discord), avec un pseudo choisi à
   la création du compte.
4. **Classement Elo** des parties en ligne et leaderboard public.
5. **Discord** : Rich Presence (« En partie classique — tour 7 »), invitations
   à rejoindre une partie en un clic, partage dans un serveur Discord.
6. Une partie locale peut être **sauvegardée et reprise**.
7. Les règles sont **testées à chaque push** (Linux et Windows), et la
   console, la fenêtre et le serveur partagent **le même code de jeu**.

## Choix techniques (décidés)

| Sujet | Choix | Pourquoi |
|---|---|---|
| Interface graphique | **raylib** (liée statiquement) | un seul `.exe` / binaire Linux, pensée pour les jeux |
| Serveur | **C++ maison** (`server/`) | réutilise `game.cpp` / `rules.cpp` : le serveur arbitre les parties, pas de triche, Elo calculé côté serveur |
| HTTP | **cpp-httplib** | un seul en-tête, client et serveur |
| JSON | **nlohmann/json** | un seul en-tête |
| Base de données | **SQLite** (amalgamation) | un seul fichier, rien à installer |
| Connexion | **OAuth Google et Discord** uniquement | pas de mot de passe stocké ; pseudo choisi au premier login |
| Synchronisation des parties | requêtes HTTP + attente longue (*long polling*) | jeu au tour par tour : pas besoin de WebSocket |
| Discord | Rich Presence par l'IPC local de Discord | pas de SDK à télécharger ni à lier |
| Bibliothèques | copiées dans `third_party/` | builds autonomes, rien à installer |
| Console | conservée | utile pour tester et jouer sans fenêtre |

---

## v2.2 — Fondations

- [x] CI à chaque push et pull request : console + GUI sous Linux, console
  sous Windows, `make test`.
- [x] Mise à jour des actions GitHub (avertissement Node.js 20).
- [x] Déroulement du tour dans la logique (`turn.cpp`) : action → défausse
  → noble (choix) → joueur suivant, passes automatiques. Console et fenêtre
  l'utilisent.
- [x] Tests du déroulement : défausse avec or, deux nobles, passes en
  chaîne, partie bloquée, dernier tour.
- [x] Couleurs de joueurs dans la console.

## v2.3 — Adversaires ordinateur

- [x] IA « facile » et « moyenne » (logique pure, testable).
- [x] Choix humain / ordinateur par joueur (console et fenêtre).
- [x] Tournoi automatique de 1 000 parties dans les tests (aucun plantage,
  aucun jeton perdu, la moyenne bat la facile).

## v2.4 — Sauvegarde

- [ ] Format texte `.splendor` (mode, joueurs, pioches, plateau, mains).
- [ ] Console : `s` pour sauvegarder, `--charger fichier`. Fenêtre :
  Sauvegarder / Ouvrir.
- [ ] Test : sauvegarder puis recharger donne la même partie.

## v3.0 — Interface portable (raylib) — version majeure

- [ ] Bibliothèques dans `third_party/` (raylib), build Linux et Windows.
- [ ] Réécriture de l'interface en raylib : même plateau, même placement
  (`*_rect`), écrans d'accueil et de nouvelle partie dessinés dans le jeu.
- [ ] Confort : historique des actions, survol (« il manque 2 Rouge »),
  raccourcis clavier, écran de passage entre joueurs, symbole par gemme
  (daltonisme), petites animations.
- [ ] Release : `splendor-windows.exe` et `splendor-linux` graphiques et
  autonomes, plus les versions console. GTK est retiré.

## v3.1 — Serveur, comptes et parties en ligne

- [ ] `splendor-server` (C++) : API HTTP, SQLite, arbitre les parties avec
  `Game`.
- [ ] Connexion Google / Discord par OAuth (navigateur + retour local), sans
  mot de passe ; choix d'un pseudo unique à la création du compte.
- [ ] Salons : créer une partie (mode, 2 à 4 places), code d'invitation,
  rejoindre, lancer.
- [ ] Parties en ligne dans le client : coups envoyés au serveur, plateau
  mis à jour par attente longue, reconnexion après coupure.
- [ ] Elo (K = 32, adapté à 2 à 4 joueurs par duels deux à deux) et
  leaderboard (écran dans le jeu + `GET /leaderboard`).
- [ ] Tests du serveur : faux coups refusés, Elo, partie complète à 2 clients.

## v3.2 — Discord

- [ ] Rich Presence : écran, mode, tour, score, places libres du salon.
- [ ] Invitations : bouton « Inviter » (lien `splendor://join/CODE` copié) et
  « Rejoindre » depuis le profil Discord (secret de partie Rich Presence).
- [ ] Partage dans un serveur Discord : un webhook configuré par le serveur
  publie « Alice ouvre une partie classique (2/4) — code ABCD ».
- [ ] Déploiement du serveur (hébergeur à choisir) et documentation.

## Plus tard

- Version macOS.
- Variantes de règles (score de victoire réglable).
- Statistiques de fin de partie et historique des parties en ligne.

## À faire par le propriétaire du projet

Ces étapes demandent votre compte et ne peuvent pas être faites à votre
place :

- ~~**Wiki** : créer la première page sur GitHub~~ (fait).
- **Discord** : créer une application sur le [portail développeur](https://discord.com/developers/applications)
  (identifiant client pour la connexion et la Rich Presence, secret client
  pour le serveur, adresse de retour OAuth). Créer un webhook dans le salon
  qui recevra les invitations.
- **Google** : créer un identifiant OAuth « Application de bureau » dans
  Google Cloud Console.
- **Hébergement** du serveur (v3.2) : choisir l'hébergeur et créer le compte.
