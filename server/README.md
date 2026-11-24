# Serveur Gemspire en ligne

`splendor-server` gère les comptes (connexion Google ou Discord, sans mot de
passe), les salons, les parties en ligne et le classement Elo. Il réutilise la
logique du jeu (`src/game.cpp`, `src/rules.cpp`...) : c'est lui qui arbitre
chaque coup, aucun client ne peut tricher.

## Compiler

Sur Linux (Raspberry Pi OS / Debian / Ubuntu) :

```bash
sudo apt install build-essential libssl-dev
make splendor-server
make test-server        # test complet : 2 joueurs, une partie en ligne
```

cpp-httplib, nlohmann/json et SQLite sont inclus dans `third_party/`.

## Réglages (variables d'environnement)

| Variable | Rôle | Défaut |
|---|---|---|
| `SPLENDOR_HOST` | adresse d'écoute | `127.0.0.1` |
| `SPLENDOR_PORT` | port | `8080` |
| `SPLENDOR_DATABASE` | fichier SQLite | `splendor.db` |
| `SPLENDOR_PUBLIC_URL` | adresse publique (HTTPS) vue par les navigateurs | `http://localhost:8080` |
| `DISCORD_CLIENT_ID`, `DISCORD_CLIENT_SECRET` | application Discord (connexion) | vide = désactivé |
| `GOOGLE_CLIENT_ID`, `GOOGLE_CLIENT_SECRET` | identifiant OAuth Google | vide = désactivé |
| `DISCORD_WEBHOOK_URL` | annonces de salons (v3.2) | vide |
| `SPLENDOR_DEV_LOGIN` | `1` : connexion de test sans OAuth (**jamais en production**) | désactivé |
| `SPLENDOR_TURN_SECONDS` | temps pour jouer un tour en ligne | `120` |
| `SPLENDOR_RATE_LIMIT` | requêtes/minute par IP sur les routes de connexion (0 = sans limite) | `120` |
| `SPLENDOR_MIN_VERSION` | version minimale du jeu acceptée | version du serveur |
| `SPLENDOR_WEB_DIR` | jeu dans le navigateur (`make web`), servi sur `/play/` | `/opt/splendor/web` |
| `SPLENDOR_CONTACT_EMAIL` | adresse de contact affichée sur le site | vide |
| `SPLENDOR_DISCORD_INVITE` | lien d'invitation du serveur Discord (site) | vide |
| `SPLENDOR_ADMIN_DISCORD_IDS` | identifiants Discord des administrateurs (virgules) : panel `/panel-admin` et `/dev` du bot | vide = fermé |

Les secrets ne vont jamais dans le dépôt : mettez-les dans un fichier
`/etc/splendor.env` lisible seulement par le service (voir plus bas).

## Créer les applications de connexion

Dans les deux cas, l'adresse de retour (redirect URI) est :

- Discord : `https://VOTRE-ADRESSE/auth/discord/callback`
- Google : `https://VOTRE-ADRESSE/auth/google/callback`

**Discord** : [portail développeur](https://discord.com/developers/applications)
→ *New Application* → *OAuth2* : copier *Client ID* et *Client Secret*,
ajouter l'adresse de retour. Seul le scope `identify` est demandé.

**Google** : [Google Cloud Console](https://console.cloud.google.com/apis/credentials)
→ *Créer des identifiants* → *ID client OAuth* → type *Application Web* ;
ajouter l'adresse de retour. Seul le scope `openid` est demandé (pas
d'e-mail).

## Comment se passe une connexion

1. Le jeu tire un identifiant de connexion `ID` (public) **et un secret**
   `verifier` (gardé en mémoire, en `sessionStorage` sur le web), puis enregistre
   les deux avec `POST /auth/begin` (`{"provider", "login", "verifier"}`) sur son
   propre canal — le secret ne passe jamais par l'URL du navigateur.
2. Le jeu ouvre alors `/auth/<discord|google>/start?login=ID` dans le navigateur
   (refusé si la connexion n'a pas été préparée par `/auth/begin`).
3. Le serveur redirige vers Discord/Google, qui renvoie vers
   `/auth/<...>/callback` avec un code.
4. Le serveur échange ce code contre l'identifiant du compte (le secret OAuth
   reste sur le serveur), crée le compte au besoin et une session.
5. Le jeu interroge `/auth/poll?login=ID&verifier=SECRET` et reçoit le jeton de
   session (une seule fois) : connaître `ID` seul ne suffit pas. Il l'enregistre
   dans `session.txt`.
6. Au premier login, le joueur choisit son pseudo (unique).

Les jetons de session ne sont stockés qu'en empreinte SHA-256 dans la base.

## Activité Discord

Le jeu navigateur (`/play/`) peut s'ouvrir comme **Activité Discord** dans un
salon vocal. Discord charge la page par son proxy (`https://<id>.discordsays.com`) ;
le jeu y reconnaît les paramètres `frame_id` / `instance_id`, demande un code
à Discord (`AUTHORIZE`, portée `identify`, par `postMessage` dans
`web/shell.html`) et l'échange contre une session avec
`POST /auth/discord/activity {"code": ...}` (sans `redirect_uri`, même compte
que la connexion Discord habituelle). Les requêtes du jeu passent alors par
`/api`.

Réglages dans le portail Discord (application Gemspire) :

1. **Activities > Settings** : activer les Activités (« Enable Activities »).
2. **Activities > URL Mappings** (le plus long préfixe d'abord) :
   - `/api` → `gemspire.epsilon-forge.fr`
   - `/` → `gemspire.epsilon-forge.fr/play`
3. **OAuth2 > Redirects** : l'adresse habituelle
   (`https://gemspire.epsilon-forge.fr/auth/discord/callback`) suffit.
4. Tester : dans un salon vocal, bouton « Activités » (fusée) > Gemspire.
   Pendant le développement, seuls les membres de l'équipe de l'application
   la voient.

Tous ceux qui ouvrent l'Activité dans le même salon vocal (même
`instance_id`) arrivent dans le même salon de jeu (`POST /lobbies/activity`).

## API (JSON)

Authentification : en-tête `Authorization: Bearer <jeton>`.

| Méthode | Chemin | Rôle |
|---|---|---|
| GET | `/health` | état du serveur |
| POST | `/auth/begin` | `{"provider", "login", "verifier"}` : enregistre le secret avant d'ouvrir le navigateur |
| GET | `/auth/<discord\|google>/start?login=ID` | début de connexion (navigateur ; préparé par `/auth/begin`) |
| GET | `/auth/poll?login=ID&verifier=SECRET` | `pending`, `done` (+ `token`) ou `error` ; le jeton exige le bon `verifier` |
| POST | `/auth/logout` | supprime la session |
| GET | `/me` | compte : pseudo, Elo, parties, victoires, succès |
| POST | `/me/pseudo` | `{"pseudo": "..."}` |
| POST | `/lobbies` | `{"mode": "classique"\|"rapide", "seats": 2..4}` → `{"code"}` |
| POST | `/lobbies/CODE/join` · `/leave` · `/start` | rejoindre, quitter, lancer (hôte) |
| POST | `/lobbies/activity` | `{"instance": "..."}` : salon partagé d'une Activité Discord (créé à 4 places au besoin) |
| POST | `/auth/discord/activity` | `{"code": ...}` (code du SDK de Discord) → `{"token"}` |
| GET | `/lobbies/CODE?since=V` | état du salon (attente longue si version ≤ V) |
| GET | `/games/ID?since=V` | état de la partie vu par ce joueur (pioches et réservées des autres cachées) |
| POST | `/games/ID/move` | un coup (voir `src/move_json.hpp`) |
| POST | `/games/ID/forfeit` | abandon (classé dernier, Elo perdu) |
| POST | `/games/ID/chat` | `{"text": "..."}` (1 à 120 caractères) ou `{"reaction": 0..5}` ; 1 message par seconde, 6 par 20 s |
| GET | `/games/live` | parties en cours à regarder (20 au plus, les mieux classées d'abord) ; un non-joueur qui lit `/games/ID` est spectateur (tout caché) |
| GET | `/me/games` | mes 15 dernières parties (`replay` : rediffusion disponible) |
| GET | `/replays/ID` | rediffusion : `seed`, `mode`, `names`, `actions` (`move`, `skip`, `forfeit`), rejouée par `replay_game` |
| POST | `/matchmaking/join` · `/matchmaking/leave` | `{"mode": ...}` : file d'attente ; écart d'Elo accepté 100 + 20 par seconde d'attente |
| GET | `/matchmaking` | `idle`, `waiting` (`waited`, `players_waiting`) ou `matched` (`game_id`) |
| GET | `/friends` | amis (`relation` : `friend`, `incoming`, `outgoing` ; `status` des amis : `playing`, `online`, `offline`) |
| POST | `/friends/add` · `/friends/remove` | `{"pseudo": "..."}` : demande ou acceptation, retrait |
| GET | `/leaderboard` | 50 meilleurs Elo |
| POST | `/me/link` | `{"provider": "google"\|"discord"}` → `{"login"}` : ouvrir ensuite `/auth/<provider>/start?login=…` ; `/auth/poll` répond `done` + `linked` (pas de nouveau jeton) |

`GET /games/ID` donne aussi `turn_seconds_left` (temps restant du tour) et
`missed` (tours manqués d'affilée par place), `chat` (50 derniers messages,
gardés en mémoire seulement) et, à la fin, `achievements` (succès débloqués
par place, voir `src/achievements.cpp`).

Routes du bot (en-tête `X-Bot-Key`) ajoutées en v3.8 : `GET /bot/ranks`
(Elo des joueurs liés à Discord, pour les rôles de rang) et
`POST /bot/challenge` (`discord_id`, `target_discord_id`, `mode`) qui crée un
salon à 2 places et prévient les deux jeux. En v3.9 : `POST /bot/season/end`
(fin de saison : top 50 archivé, badges du top 10, Elo ramenés à mi-chemin de
1000) ; `/me` et `/leaderboard` donnent la saison en cours (`season`), `/me`
et `/bot/profile` les badges.

## Administration (v4)

Les comptes Discord listés dans `SPLENDOR_ADMIN_DISCORD_IDS` ont accès à :

- **Panel web** `https://<site>/panel-admin` : connexion par Discord (OAuth,
  même application que le jeu, aucune adresse de redirection à ajouter : le
  retour passe par `/auth/discord/callback`). Session de 8 h en mémoire
  (cookie `HttpOnly`, `SameSite=Strict`, `Secure` en HTTPS), jeton anti-CSRF
  dans chaque formulaire, pages `noindex` sans JavaScript. On y trouve :
  - état du serveur (joueurs en ligne, parties, salons, file, comptes) ;
  - interrupteurs : **fermer les inscriptions** (les comptes existants se
    connectent toujours) et **suspendre les nouvelles parties** (salons,
    recherche, `/challenge` ; les parties en cours continuent) ;
  - **annonce** affichée dans le jeu (accueil, menu en ligne) et envoyée tout
    de suite aux joueurs connectés ;
  - recherche d'un compte (pseudo, numéro ou identifiant Discord) et fiche :
    renommer, fixer l'Elo, donner / retirer un badge de saison, déconnecter
    partout, suspendre / réactiver, supprimer (taper `DELETE`), annuler l'Elo
    d'une partie ;
  - **graphique des parties par jour** (30 jours) ;
  - **terminer une partie en cours bloquée** (sans Elo ni enregistrement : les
    jeux des joueurs reviennent au menu en ligne) ;
  - **version minimale du jeu** modifiable sans redémarrer (vide : revient à
    `SPLENDOR_MIN_VERSION`) ;
  - **signalements** envoyés avec `/report` sur Discord (`POST /bot/report`),
    à marquer comme traités ;
  - journal de toutes les actions d'administration (table `admin_log`).
- **Commandes `/dev` du bot** (`POST /bot/admin`, clé du bot + identifiant
  Discord de l'auteur, vérifié par le serveur) : `status`, `signups`, `games`,
  `announce`, `rename`, `elo`, `badge`, `remove-badge`, `logout`, `end-game`,
  `min-version`.

Le panel et le bot passent par la même fonction (`ServerApp::admin_action`,
`server/admin.cpp`). Il n'y a pas de mot de passe à réinitialiser : la
connexion se fait par Discord ou Google ; « déconnecter partout » ferme
toutes les sessions d'un compte.

## Version du jeu

Le jeu envoie sa version dans l'en-tête `X-Splendor-Version`. Pour les routes
du jeu (`/me`, `/lobbies`, `/games/`, `/friends`, `/auth/poll`, `/auth/logout`,
`/config`), une version absente, mal écrite ou plus ancienne que
`SPLENDOR_MIN_VERSION` est refusée avec **426** (`error`, `min_version`,
`download`) : le jeu affiche alors l'écran « Mise à jour nécessaire » et se
bloque. Le site, `/leaderboard`, `/health`, la connexion dans le navigateur et
le bot ne sont pas concernés. Par défaut, la version minimale est celle du
serveur : mettre à jour le serveur oblige à mettre à jour le jeu.

Un jeu modifié peut annoncer une fausse version : ce contrôle sert à écarter
les versions périmées, pas la triche (le serveur arbitre de toute façon
chaque coup).

## Temps de jeu et abandon

Chaque tour dure au plus `SPLENDOR_TURN_SECONDS` (2 minutes). Passé ce délai,
le serveur termine la défausse ou le choix du noble s'il le faut, puis le tour
passe. Après 3 tours manqués d'affilée, le joueur abandonne : il est classé
dernier (Elo perdu) et la partie continue entre les autres ; s'il ne reste
qu'un joueur, il gagne. Un temps écoulé ne compte pas comme « ne peut rien
faire » (la partie n'est donc pas déclarée bloquée).

## Site web

En anglais et en français : `?lang=en` / `?lang=fr`, sinon la langue du
navigateur (`Accept-Language`). Lien « Français / English » sur chaque page.

| Chemin | Page |
|---|---|
| `/` | accueil, téléchargements |
| `/leaderboard` (`/classement`) | classement Elo (100 premiers) ; JSON si le client ne demande pas de HTML |
| `/terms` (`/conditions`) | conditions d'utilisation |
| `/privacy` (`/confidentialite`) | politique de confidentialité |
| `/play/` | le jeu dans le navigateur (si `SPLENDOR_WEB_DIR` existe) |

Connexion depuis le jeu dans le navigateur : `/auth/<fournisseur>/start?login=ID&from=play`
fait revenir sur `/play/?login=ID` après Discord ou Google (même onglet, pas de
fenêtre surgissante), et le jeu termine la connexion avec `/auth/poll`.

Pages en HTML et CSS (`server/pages.cpp`), sans JavaScript, avec une
politique de sécurité du contenu stricte.

## Installer sur le Raspberry Pi

Voir [deploy/README.md](../deploy/README.md) : services systemd, base dans
`/var/lib/splendor`, HTTPS par Cloudflare Tunnel.
