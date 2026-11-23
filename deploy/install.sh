#!/usr/bin/env bash
# Installe (ou met à jour) le serveur et le bot Gemspire sur le Raspberry Pi.
# À lancer depuis la racine du dépôt cloné sur le Pi :  sudo deploy/install.sh
# Le script ne touche jamais aux secrets déjà présents dans /etc/splendor/splendor.env.
set -euo pipefail

if [ "$(id -u)" -ne 0 ]; then
    echo "Lancez ce script avec sudo." >&2
    exit 1
fi
cd "$(dirname "$0")/.."

echo "== Paquets nécessaires"
apt-get update -qq
apt-get install -y -qq build-essential libssl-dev python3 python3-venv

echo "== Compilation du serveur"
sudo -u "${SUDO_USER:-root}" make splendor-server

echo "== Utilisateur et dossiers"
id splendor >/dev/null 2>&1 || useradd --system --home /var/lib/splendor --shell /usr/sbin/nologin splendor
install -d -o splendor -g splendor -m 750 /var/lib/splendor
install -d -m 755 /opt/splendor /opt/splendor/bot
install -d -o root -g splendor -m 750 /etc/splendor

echo "== Installation des fichiers"
install -m 755 splendor-server /opt/splendor/splendor-server
# Tous les fichiers Python du bot (sauf les tests) et ses dépendances
for file in bot/*.py bot/requirements.txt; do
    case "$file" in
        bot/test_*) ;;
        *) install -m 644 "$file" /opt/splendor/bot/ ;;
    esac
done
if [ ! -d /opt/splendor/bot/venv ]; then
    python3 -m venv /opt/splendor/bot/venv
fi
/opt/splendor/bot/venv/bin/pip install -q -r /opt/splendor/bot/requirements.txt
# Vérification : le bot se charge (un fichier oublié se voit ici, pas au démarrage du service)
(cd /opt/splendor/bot && SPLENDOR_BOT_KEY=verification venv/bin/python -c "import splendor_bot") \
    || { echo "Le bot ne se charge pas : installation arrêtée." >&2; exit 1; }

if [ ! -f /etc/splendor/splendor.env ]; then
    echo "== Premier lancement : fichier de réglages à compléter"
    install -o root -g splendor -m 640 deploy/splendor.env.example /etc/splendor/splendor.env
    key=$(head -c 32 /dev/urandom | od -An -tx1 | tr -d ' \n')
    sed -i "s/^SPLENDOR_BOT_KEY=.*/SPLENDOR_BOT_KEY=$key/" /etc/splendor/splendor.env
fi
# Réglages ajoutés depuis (v4) : recopiés du modèle s'ils manquent, sans toucher au reste
for setting in SPLENDOR_ADMIN_DISCORD_IDS; do
    if ! grep -q "^$setting=" /etc/splendor/splendor.env; then
        grep "^$setting=" deploy/splendor.env.example >> /etc/splendor/splendor.env
        echo "Réglage ajouté : $(grep "^$setting=" deploy/splendor.env.example)"
    fi
done

# Jeu dans le navigateur (/play) : compilé ici s'il existe (make web), sinon
# l'archive de la dernière version publiée sur GitHub
echo "== Jeu dans le navigateur"
install -d -m 755 /opt/splendor/web
if [ -f build/web/index.html ]; then
    install -m 644 build/web/index.html build/web/index.js build/web/index.wasm /opt/splendor/web/
else
    tmp=$(mktemp -d)
    if curl -fsSL -o "$tmp/web.tar.gz" https://github.com/Pierre-Pln-Bys/Gemspire/releases/latest/download/gemspire-web.tar.gz; then
        tar xzf "$tmp/web.tar.gz" -C /opt/splendor/web
        chmod 644 /opt/splendor/web/*
    else
        echo "Jeu dans le navigateur non installé (archive introuvable) : /play sera absent."
    fi
    rm -rf "$tmp"
fi

install -m 644 deploy/splendor-server.service deploy/splendor-bot.service /etc/systemd/system/
systemctl daemon-reload
systemctl enable --now splendor-server.service
systemctl restart splendor-server.service

# Le bot ne démarre que si son jeton est renseigné
if grep -q '^DISCORD_BOT_TOKEN=.\+' /etc/splendor/splendor.env; then
    systemctl enable --now splendor-bot.service
    systemctl restart splendor-bot.service
else
    echo "Bot non démarré : renseignez DISCORD_BOT_TOKEN dans /etc/splendor/splendor.env puis relancez ce script."
fi

echo "== État"
systemctl --no-pager --lines=3 status splendor-server.service || true
# Le serveur met un instant à ouvrir son port : quelques essais
port=$(grep '^SPLENDOR_PORT=' /etc/splendor/splendor.env | cut -d= -f2)
for _ in 1 2 3 4 5; do
    curl -fsS "http://127.0.0.1:$port/health" && echo && break
    sleep 1
done
echo "Fini. Réglages : sudo nano /etc/splendor/splendor.env  (puis sudo systemctl restart splendor-server splendor-bot)"
