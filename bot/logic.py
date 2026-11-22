"""Décisions du bot « Crée ta partie », sans dépendre de discord.py (testable).

Fonctionnement :
1. Un membre rejoint le salon vocal « Crée ta partie » : le bot crée un salon
   vocal temporaire « Partie de <nom> » et l'y déplace.
2. Si le compte Splendor lié à ce Discord a le jeu ouvert, le serveur propose
   dans le jeu de créer la partie (mode, places).
3. Le salon temporaire est limité au nombre de places de la partie, et
   supprimé quand il est resté vide 30 secondes.
4. La liste des salons temporaires est gardée dans un fichier : après un
   redémarrage, le bot les reconnaît encore (et supprime ceux restés vides).
"""

import json
import os

CREATE_CHANNEL_NAME = "Crée ta partie"
OFFER_RETRY_SECONDS = 10      # nouvelle tentative si le jeu n'est pas ouvert
OFFER_RETRY_COUNT = 12        # pendant 2 minutes
DELETE_DELAY_SECONDS = 30     # un salon vide est supprimé après ce délai


def normalize(name):
    """Nom de salon comparé sans casse ni espaces autour."""
    return name.strip().casefold()


def is_create_channel(channel_name, configured_name=CREATE_CHANNEL_NAME):
    return channel_name is not None and normalize(channel_name) == normalize(configured_name)


def temp_channel_name(display_name):
    """Nom du salon temporaire (100 caractères au plus pour Discord)."""
    name = "Partie de " + (display_name.strip() or "joueur")
    return name[:100]


def should_delete(is_temp_channel, member_count):
    """Un salon temporaire vide doit être supprimé (après DELETE_DELAY_SECONDS)."""
    return is_temp_channel and member_count == 0


def load_channel_ids(path):
    """Identifiants des salons temporaires enregistrés ([] si le fichier manque ou est abîmé)."""
    try:
        with open(path, encoding="utf-8") as file:
            ids = json.load(file)
    except (OSError, ValueError):
        return []
    if not isinstance(ids, list):
        return []
    return [int(i) for i in ids if isinstance(i, int) and not isinstance(i, bool)]


def save_channel_ids(path, ids):
    """Enregistre les identifiants (fichier temporaire puis renommage : jamais de fichier à moitié écrit)."""
    temporary = path + ".tmp"
    with open(temporary, "w", encoding="utf-8") as file:
        json.dump(sorted(ids), file)
    os.replace(temporary, path)


class TempChannel:
    """Ce que le bot sait d'un salon temporaire qu'il a créé."""

    def __init__(self, owner_id):
        self.owner_id = owner_id
        self.code = None          # code du salon Splendor lié
        self.announced = False    # code déjà annoncé dans le salon
        self.started = False      # partie lancée


def apply_event(temp, event):
    """Applique un événement du serveur à un salon temporaire.

    Retourne (limite_de_places, message) : la limite à mettre sur le salon
    vocal (0 = illimité, None = ne pas changer) et le message à écrire dans le
    salon (None = rien).
    """
    if event.get("closed"):
        temp.code = None
        temp.announced = False
        temp.started = False
        return 0, "La partie Splendor liée à ce salon a été annulée."
    temp.code = event.get("code")
    seats = int(event.get("seats", 0))
    message = None
    if event.get("started") and not temp.started:
        temp.started = True
        message = "La partie Splendor **{}** a commencé. Bon jeu !".format(temp.code)
    elif not temp.announced:
        temp.announced = True
        message = ("Partie Splendor créée : code **{}** ({} places). Rejoignez ce salon vocal, "
                   "puis dans Splendor : Jouer en ligne > Rejoindre.").format(temp.code, seats)
    return seats, message


def offer_message(presence):
    """Message écrit dans le salon temporaire selon la présence du joueur."""
    if not presence.get("linked"):
        return ("Pour créer une partie Splendor ici, connectez-vous dans le jeu avec **Discord** "
                "(Jouer en ligne > Se connecter avec Discord).")
    if not presence.get("online"):
        return ("Lancez Splendor (connecté avec Discord) : la création de la partie s'ouvrira "
                "dans le jeu. J'attends 2 minutes.")
    return "Configurez la partie dans Splendor : la fenêtre vient de s'ouvrir dans le jeu."
