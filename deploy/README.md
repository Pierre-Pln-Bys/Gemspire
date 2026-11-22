# Déployer Splendor sur le Raspberry Pi

Ce dossier installe le **serveur** (`splendor-server`) et le **bot Discord**
(`bot/`) comme services systemd sur le Raspberry Pi.

| Fichier | Rôle |
|---|---|
| `install.sh` | installe ou met à jour (paquets, compilation, utilisateur `splendor`, services) |
| `splendor.env.example` | modèle des réglages et secrets (copié dans `/etc/splendor/splendor.env`) |
| `splendor-server.service` | service du serveur (durci : n'écrit que dans `/var/lib/splendor`) |
| `splendor-bot.service` | service du bot (démarré seulement si son jeton est renseigné) |

## 1. Installer

Sur le Pi (Raspberry Pi OS 64 bits ou Debian/Ubuntu) :

```bash
git clone https://github.com/Pierre-Pln-Bys/ProjectSplendor.git
```

```bash
cd ProjectSplendor && sudo deploy/install.sh
```

Le serveur écoute sur `127.0.0.1:8090` (pas exposé directement ; 8080 est souvent déjà pris, par exemple par Nextcloud). Au premier
lancement, `/etc/splendor/splendor.env` est créé avec une clé de bot
aléatoire. Pour mettre à jour plus tard : `git pull` puis relancer le script.

## 2. Adresse publique en HTTPS

Discord et Google exigent une adresse HTTPS. Le plus simple, sans ouvrir de
port sur la box : **Cloudflare Tunnel** (nom de domaine géré par Cloudflare).

```bash
sudo apt install cloudflared   # voir la doc Cloudflare pour le dépôt apt
cloudflared tunnel login
cloudflared tunnel create splendor
cloudflared tunnel route dns splendor splendor.votre-domaine.fr
```

Configuration du tunnel (`/etc/cloudflared/config.yml`) : diriger
`splendor.votre-domaine.fr` vers `http://127.0.0.1:8090`, puis
`sudo cloudflared service install`.

Autre possibilité : rediriger les ports 80/443 de la box vers le Pi et
mettre **Caddy** devant le serveur (`reverse_proxy 127.0.0.1:8090`, certificat
automatique).

Dans `/etc/splendor/splendor.env` : `SPLENDOR_PUBLIC_URL=https://splendor.votre-domaine.fr`.

## 3. Connexions Discord et Google

Adresses de retour à déclarer :

- Discord : `https://splendor.votre-domaine.fr/auth/discord/callback`
- Google : `https://splendor.votre-domaine.fr/auth/google/callback`

Renseigner `DISCORD_CLIENT_ID`, `DISCORD_CLIENT_SECRET`, `GOOGLE_CLIENT_ID`,
`GOOGLE_CLIENT_SECRET`, puis :

```bash
sudo systemctl restart splendor-server
```

`DISCORD_CLIENT_ID` sert aussi à la **Rich Presence** : le jeu le récupère
sur `/config`. Dans le portail Discord, ajouter dans *Rich Presence > Art
Assets* une image `logo` si on veut une icône.

## 4. Bot « Crée ta partie »

1. Portail développeur > votre application > **Bot** > *Reset Token* :
   copier le jeton dans `DISCORD_BOT_TOKEN`.
2. Inviter le bot sur le serveur Discord (remplacer `ID` par le Client ID) :
   `https://discord.com/oauth2/authorize?client_id=ID&scope=bot&permissions=17828880`
   (voir les salons, envoyer des messages, se connecter, gérer les salons,
   déplacer des membres).
3. Créer un salon vocal nommé **Crée ta partie** (ou changer
   `CREATE_CHANNEL_NAME`).
4. Relancer `sudo deploy/install.sh` : le bot démarre.

Fonctionnement : quand un membre rejoint « Crée ta partie », le bot crée
« Partie de <nom> » et l'y déplace. Si son compte Splendor (connecté avec
Discord) a le jeu ouvert, le jeu propose de créer la partie ; le salon vocal
est alors limité au nombre de places, le code y est annoncé, et le salon est
supprimé quand il se vide.

## 5. Indiquer le serveur au jeu

Dans le jeu : fichier `serveur.txt` à côté de l'exécutable contenant
`https://splendor.votre-domaine.fr` (ou variable `SPLENDOR_SERVER`). Une fois
l'adresse définitive connue, elle deviendra l'adresse par défaut des
releases.

## Vérifier

```bash
systemctl status splendor-server splendor-bot
```

```bash
journalctl -u splendor-server -u splendor-bot -f
```

```bash
curl -s http://127.0.0.1:8090/health
```
