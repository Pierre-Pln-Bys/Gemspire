# Roadmap

Point de départ : v2.1.0 (20/11/2026). **Fait depuis** : v2.2 (fondations),
v2.3 (ordinateur), v2.4 (sauvegarde), v3.0 (interface raylib autonome),
v3.1 (serveur, comptes, parties en ligne, Elo).
v3.2 (Discord : présence, invitations, bot vocal ; serveur en ligne sur le
Raspberry Pi, `https://gemspire.epsilon-forge.fr`).
v3.3 (temps de jeu et abandon en ligne, site web), v3.4 (serveur Discord :
commandes du bot, classement, modération).
v3.5 (jeu dans le navigateur, site et bot bilingues), v3.6 (Gemspire),
v3.7 (jeu en anglais et en français, Activité Discord), v3.8 (chat, amis,
succès, `/challenge`, rôles de rang), v3.9 (spectateurs, rediffusions,
saisons, recherche de partie).
**Prochaine étape** : la **v4 : publication et test d'intrusion**, qui
couvrira tout cela.

Suivi des tâches : [GitHub Project « Gemspire — Roadmap v3 »](https://github.com/users/Pierre-Pln-Bys/projects/2)
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

- [x] Format texte `.splendor` (mode, joueurs, pioches, plateau, mains).
- [x] Console : `s` pour sauvegarder, `--charger fichier`. Fenêtre :
  Sauvegarder / Ouvrir.
- [x] Test : sauvegarder puis recharger donne la même partie.

## v3.0 — Interface portable (raylib) — version majeure

- [x] Bibliothèques dans `third_party/` (raylib), build Linux et Windows.
- [x] Réécriture de l'interface en raylib : même plateau, même placement
  (`*_rect`), écrans d'accueil et de nouvelle partie dessinés dans le jeu.
- [x] Confort : historique des actions, survol (« il manque 2 Rouge »),
  raccourcis clavier, écran de passage entre joueurs, symbole par gemme
  (daltonisme), petites animations.
- [x] Release : `gemspire-windows.exe` et `gemspire-linux` graphiques et
  autonomes, plus les versions console. GTK est retiré.

## v3.1 — Serveur, comptes et parties en ligne

- [x] `splendor-server` (C++) : API HTTP, SQLite, arbitre les parties avec
  `Game`.
- [x] Connexion Google / Discord par OAuth (navigateur + retour local), sans
  mot de passe ; choix d'un pseudo unique à la création du compte.
- [x] Salons : créer une partie (mode, 2 à 4 places), code d'invitation,
  rejoindre, lancer.
- [x] Parties en ligne dans le client : coups envoyés au serveur, plateau
  mis à jour par attente longue, reconnexion après coupure.
- [x] Elo (K = 32, adapté à 2 à 4 joueurs par duels deux à deux) et
  leaderboard (écran dans le jeu + `GET /leaderboard`).
- [x] Tests du serveur : faux coups refusés, Elo, partie complète à 2 clients.

## v3.2 — Discord

- [x] Rich Presence : écran, mode, tour, score, places libres du salon.
- [x] Invitations : bouton « Inviter » (lien `splendor://join/CODE` copié) et
  « Rejoindre » depuis le profil Discord (secret de partie Rich Presence).
- [x] Partage dans un serveur Discord : un webhook configuré par le serveur
  publie « Alice ouvre une partie classique (2/4) — code ABCD ».
- [x] Bot Discord « Crée ta partie » : rejoindre ce salon vocal crée un salon
  temporaire ; si le compte lié a le jeu ouvert, le jeu propose de créer la
  partie (mode, places) ; le salon est limité au nombre de places et supprimé
  quand il est vide.
- [x] Déploiement du serveur sur le Raspberry Pi du projet (HTTPS par
  Cloudflare Tunnel, démarrage automatique) et documentation ; testé avec les
  vraies applications Discord et Google, sous Linux et Windows 11 (v3.2.2).

## v3.3 — Corrections et site web

- [x] Sélection des jetons : impossible de choisir 2 jetons identiques si la
  pile en a moins de 4 (ou avec d'autres couleurs), ni plus de 3 couleurs.
- [x] Annonce bien visible quand un noble rend visite à un joueur.
- [x] Annonce du dernier tour quand un joueur atteint 15 points.
- [x] Temps de jeu en ligne : 2 minutes par tour, tour passé, abandon après
  3 tours manqués ou bouton « Abandonner » (classé dernier, Elo perdu).
- [x] Page d'accueil du site (`/`) : présentation, téléchargements, liens.
- [x] Classement public (`/classement`), lisible sur mobile.
- [x] Pages Conditions d'utilisation (`/conditions`) et Confidentialité
  (`/confidentialite`), demandées par Google et Discord.

## v3.4 — Discord, hub du jeu

- [x] Salon `#classement` : top 10 mis à jour toutes les 10 minutes (un seul
  message modifié).
- [x] Commandes du bot : `/classement`, `/profil`, `/lier` (rôle Joueur),
  `/jouer`, `/regles`, `/signaler`.
- [x] Modération : `/mod ban` (compte de jeu suspendu, parties abandonnées,
  retiré du classement), `/mod unban`, `/mod historique` (alerte sur les
  adversaires trop fréquents), `/mod annuler-partie`, `/mod supprimer-compte`
  (RGPD), journal dans `#moderation`.
- [x] Plan du serveur Discord (rôles, salons, réglages) : wiki « Serveur
  Discord ».

## v3.5 — Jouer sur mobile

- [x] Version navigateur (`/play/`) utilisable sur téléphone : le même code
  C++ compilé en WebAssembly (raylib + Emscripten), commandes tactiles,
  connexion Discord / Google et parties en ligne.
- [x] Site en anglais et en français ; bot en anglais traduit en français.

## v3.6 — Gemspire

- [x] Nouveau nom **Gemspire** (jeu, site, bot, dépôt GitHub), licence GPL-3.0.
- [x] Changelog publié sur Discord à chaque release ; bot réglable par
  identifiants de salons et de rôles.
- [x] Adresse `gemspire.epsilon-forge.fr` (tunnel, Discord, Google, Pi),
  release v3.6.0 avec les exécutables `gemspire-*`.

## v3.7 — Activité Discord et jeu en anglais

- [x] Interface du jeu en anglais, avec choix de la langue (anglais par
  défaut, français) : fenêtre, navigateur, console, messages du serveur.
- [x] Gemspire comme **Activité Discord** (connexion et réseau ; la partie
  liée au salon vocal viendra avec la v3.8) : on lance une partie directement
  dans un salon vocal, sans rien installer (la version navigateur intégrée à
  Discord avec l'Embedded App SDK : connexion par Discord, réseau par le
  proxy de Discord, partie liée au salon vocal).

## v3.8 — Communauté

- [x] Chat dans la partie (via le serveur) : longueur limitée, anti-spam,
  mise en sourdine d'un joueur, réactions rapides.
- [x] Liste d'amis, statut en ligne / en partie.
- [x] Commandes du bot : `/challenge @joueur` (défi), rôles de rang selon
  l'Elo.
- [x] Succès (premier noble, victoire sans réserver...).
- [x] Activité Discord : même salon de jeu pour tout le salon vocal.

## v3.9 — Compétition

- [x] Mode spectateur et rediffusion des parties (graine et actions enregistrées).
- [x] Saisons classées (Elo remis partiellement à zéro, badges).
- [x] Recherche de partie automatique à Elo proche.

## v4 — Publication et sécurité (version majeure) — publiée le 25/11/2026

- [x] Google : écran de consentement complété (accueil, confidentialité,
  e-mail d'assistance), domaine vérifié, branding **validé** et application
  publiée.
- [x] Panel d'administration `/panel-admin` et commandes `/dev` du bot.
- [x] Thèmes du plateau, écran des règles, rendu net (navigateur, HiDPI) et
  plein écran ; liaison Google + Discord sur un même compte.
- [x] Discord : bot **vérifié**.
- [x] Test d'intrusion autorisé du serveur, du Raspberry Pi, du tunnel et de
  l'application (extension de pentest), rapport.
- [x] Corrections des failles trouvées (relues). Nouveau test complet : plus tard.
- [x] Empreintes SHA-256 publiées avec chaque release.

## v4.1 — Saisons et visibilité

- [x] Saisons automatiques (tous les deux mois, 1er janvier, mars, mai...) ;
  Elo de saison remis à 1000, Elo global jamais remis à zéro ; classement de
  saison (10 parties minimum) et classement global.
- [x] Recherche de partie en deux files : Classé (Elo global) et Saison.
- [x] Parties amicales (salons, défis) sans Elo ; défi d'un ami depuis la
  liste d'amis.
- [x] SEO du site : titre et description par page, adresses par langue
  (canonical, hreflang), aperçu de partage (Open Graph), fiche `VideoGame`,
  `sitemap.xml`.
- [ ] Passe de saison : à cadrer (voir plus bas).

## Plus tard

- Signature des exécutables Windows (plus d'avertissement SmartScreen) :
  reportée (l'offre gratuite de SignPath Foundation exige un code source
  public, ce qui n'est pas souhaité pour l'instant).
- **Publication sur des boutiques** : itch.io d'abord (simple, gratuit, jeu
  navigateur et téléchargements), puis Steam (frais d'inscription, page
  boutique, SDK facultatif).
- **Passe de saison** (« passe de combat ») : paliers débloqués en jouant
  (défis de saison, XP par partie) donnant des récompenses cosmétiques
  (thèmes, dos de cartes, cadres de profil). À cadrer : gratuit seulement, ou
  payant (paiement, conditions de vente, mineurs).
- **Revendiquer le jeu sur Discord** (*Claim Your Game*) : possible seulement
  quand Discord détecte Gemspire comme jeu publié (assez de joueurs, ou
  sortie sur une boutique comme itch.io / Steam).
- Version macOS.
- Variantes de règles (score de victoire réglable).
- Statistiques de fin de partie et historique des parties en ligne.

## À faire par le propriétaire du projet

Ces étapes demandent votre compte et ne peuvent pas être faites à votre
place :

- ~~Wiki, application Discord, webhook, identifiant OAuth Google, accès au
  Raspberry Pi et nom de domaine~~ (fait, v3.2).
- **v4** : créer la Team Discord et y rattacher l'application ; publier
  l'application Google une fois les pages du site en ligne ; choisir une
  solution de signature des exécutables Windows.
