# Serveur Splendor en ligne

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

1. Le jeu tire un identifiant de connexion aléatoire et ouvre
   `/auth/<discord|google>/start?login=ID` dans le navigateur.
2. Le serveur redirige vers Discord/Google, qui renvoie vers
   `/auth/<...>/callback` avec un code.
3. Le serveur échange ce code contre l'identifiant du compte (le secret reste
   sur le serveur), crée le compte au besoin et une session.
4. Le jeu interroge `/auth/poll?login=ID` et reçoit le jeton de session (une
   seule fois). Il l'enregistre dans `session.txt`.
5. Au premier login, le joueur choisit son pseudo (unique).

Les jetons de session ne sont stockés qu'en empreinte SHA-256 dans la base.

## API (JSON)

Authentification : en-tête `Authorization: Bearer <jeton>`.

| Méthode | Chemin | Rôle |
|---|---|---|
| GET | `/health` | état du serveur |
| GET | `/auth/<discord\|google>/start?login=ID` | début de connexion (navigateur) |
| GET | `/auth/poll?login=ID` | `pending`, `done` (+ `token`) ou `error` |
| POST | `/auth/logout` | supprime la session |
| GET | `/me` | compte : pseudo, Elo, parties, victoires |
| POST | `/me/pseudo` | `{"pseudo": "..."}` |
| POST | `/lobbies` | `{"mode": "classique"\|"rapide", "seats": 2..4}` → `{"code"}` |
| POST | `/lobbies/CODE/join` · `/leave` · `/start` | rejoindre, quitter, lancer (hôte) |
| GET | `/lobbies/CODE?since=V` | état du salon (attente longue si version ≤ V) |
| GET | `/games/ID?since=V` | état de la partie vu par ce joueur (pioches et réservées des autres cachées) |
| POST | `/games/ID/move` | un coup (voir `src/move_json.hpp`) |
| GET | `/leaderboard` | 50 meilleurs Elo |

## Installer sur le Raspberry Pi (v3.2)

Prévu : service systemd, base dans `/var/lib/splendor`, accès HTTPS par un
tunnel (Cloudflare Tunnel) ou un proxy (Caddy) devant `127.0.0.1:8080`. Ce sera
détaillé quand l'accès SSH au Pi sera disponible.
