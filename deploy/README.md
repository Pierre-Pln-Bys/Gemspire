# Déployer Gemspire sur le Raspberry Pi

Ce dossier installe le **serveur** (`splendor-server`) et le **bot Discord**
(`bot/`) comme services systemd sur le Raspberry Pi.

| Fichier | Rôle |
|---|---|
| `install.sh` | installe ou met à jour (paquets, compilation, utilisateur `splendor`, services) |
| `splendor.env.example` | modèle des réglages et secrets (copié dans `/etc/splendor/splendor.env`) |
| `splendor-server.service` | service du serveur (durci : n'écrit que dans `/var/lib/splendor`) |
| `splendor-bot.service` | service du bot (démarré seulement si son jeton est renseigné) |
| `wifi-powersave-off.service` | Pi en Wi-Fi : coupe l'économie d'énergie du Wi-Fi à chaque démarrage (sinon le Pi peut devenir injoignable en local, SSH compris) ; installé seulement si `wlan0` existe |

## 1. Installer

Sur le Pi (Raspberry Pi OS 64 bits ou Debian/Ubuntu) :

```bash
git clone https://github.com/Pierre-Pln-Bys/Gemspire.git
```

```bash
cd Gemspire && sudo deploy/install.sh
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

## 4. Bot Discord

1. Portail développeur > votre application > **Bot** > *Reset Token* :
   copier le jeton dans `DISCORD_BOT_TOKEN`. Laisser les trois *Privileged
   Gateway Intents* **désactivés** (inutiles).
2. Inviter le bot (ou le réinviter pour lui donner les nouveaux droits) :
   `https://discord.com/oauth2/authorize?client_id=ID&scope=bot+applications.commands&permissions=286346256`
   (voir les salons, envoyer des messages, intégrer des liens, lire
   l'historique, gérer les salons, gérer les rôles, se connecter, déplacer
   des membres).
3. Créer le salon vocal **Crée ta partie**, et si on veut : `#classement`,
   `#moderation`, les rôles `Modérateur` et `Joueur` (le rôle du bot doit être
   **au-dessus** de `Joueur`). Plan complet du serveur : wiki, page
   « Serveur Discord ».
4. Relancer `sudo deploy/install.sh` : le bot démarre et publie ses commandes.

Fonctionnement :

- **Crée ta partie** : quand un membre rejoint ce salon, le bot crée « Partie
  de <nom> » et l'y déplace. Si son compte Gemspire (connecté avec Discord) a
  le jeu ouvert, le jeu propose de créer la partie ; le salon vocal est alors
  limité au nombre de places, le code y est annoncé, et le salon est supprimé
  quand il est resté vide 30 secondes (le bot s'en souvient même après un
  redémarrage : état dans `/var/lib/splendor-bot`).
- **#classement** : top 10, un seul message modifié toutes les 10 minutes.
- Salons et rôles : par **identifiant** si `CREATE_CHANNEL_ID`,
  `RANKING_CHANNEL_ID`, `MOD_LOG_CHANNEL_ID`, `MOD_ROLE_ID`, `PLAYER_ROLE_ID`
  sont remplis (un salon renommé reste reconnu), sinon par nom.
- **Commandes** : `/classement`, `/profil`, `/lier` (donne le rôle Joueur),
  `/jouer`, `/regles`, `/signaler` ; modérateurs (rôle `Modérateur` ou
  administrateurs) : `/mod ban`, `/mod unban`, `/mod historique`,
  `/mod annuler-partie`, `/mod supprimer-compte`, journalisées dans
  `#moderation`.

## Jeu dans le navigateur

`install.sh` installe le jeu WebAssembly dans `/opt/splendor/web` : celui de
`build/web` s'il a été compilé sur place (`make web`), sinon l'archive
`gemspire-web.tar.gz` de la dernière release GitHub. Il est servi sur
`https://gemspire.epsilon-forge.fr/play/`, et le site affiche alors le bouton
« Jouer dans le navigateur ».

## Changelog sur Discord

À chaque release, le workflow publie la section du CHANGELOG de la version
dans un salon Discord (`tools/discord_release.py`). Créer un webhook dans le
salon des annonces (*Paramètres du salon > Intégrations > Webhooks*), puis
l'enregistrer comme secret GitHub, depuis le dossier du dépôt :

```bash
gh secret set DISCORD_RELEASE_WEBHOOK
```

(la commande demande l'adresse, qui n'apparaît ni dans le dépôt ni dans
l'historique). Aperçu sans envoyer : `python3 tools/discord_release.py vX.Y.Z --dry-run`.
Republier une annonce : `gh workflow run announce.yml -f tag=vX.Y.Z`.
Le CHANGELOG est écrit en anglais, puisqu'il est publié tel quel.

## 5. Indiquer le serveur au jeu

Le jeu utilise par défaut `https://gemspire.epsilon-forge.fr` (constante
`DEFAULT_SERVER` dans `src/gui.hpp`). Pour un autre serveur : fichier
`serveur.txt` à côté de l'exécutable contenant son adresse, ou variable
`SPLENDOR_SERVER`.

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
