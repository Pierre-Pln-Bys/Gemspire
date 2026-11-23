"""Décisions du bot « Crée ta partie », sans dépendre de discord.py (testable).

Fonctionnement :
1. Un membre rejoint le salon vocal « Crée ta partie » : le bot crée un salon
   vocal temporaire « Partie de <nom> » et l'y déplace.
2. Si le compte Gemspire lié à ce Discord a le jeu ouvert, le serveur propose
   dans le jeu de créer la partie (mode, places).
3. Le salon temporaire est limité au nombre de places de la partie, et
   supprimé quand il est resté vide 30 secondes.
4. La liste des salons temporaires est gardée dans un fichier : après un
   redémarrage, le bot les reconnaît encore (et supprime ceux restés vides).
5. Le salon #classement affiche le top 10, mis à jour toutes les 10 minutes
   (le bot modifie toujours le même message).
6. Commandes slash pour tous (/leaderboard, /profile, /link...) et pour les
   modérateurs (/mod ban, /mod history...), journalisées dans #moderation.
   Noms et textes en anglais, traduits en français (texts.py) selon la
   langue de chaque membre ou du serveur.
"""

import json
import os
import time
import unicodedata
from collections import Counter

from texts import tr

CREATE_CHANNEL_NAME = "Create a game"
CREATE_CHANNEL_ALIASES = ["Crée ta partie", "Create a game"]  # acceptés en plus du nom réglé
OFFER_RETRY_SECONDS = 10      # nouvelle tentative si le jeu n'est pas ouvert
OFFER_RETRY_COUNT = 12        # pendant 2 minutes
DELETE_DELAY_SECONDS = 30     # un salon vide est supprimé après ce délai
RANKING_SECONDS = 600         # mise à jour du salon #classement
RANKING_SIZE = 10
SITE_URL = "https://gemspire.epsilon-forge.fr"
DOWNLOAD_URL = "https://github.com/Pierre-Pln-Bys/Gemspire/releases/latest"
MEDALS = ["🥇", "🥈", "🥉"]
# Rôles de rang selon l'Elo (du plus haut au plus bas) : nom et couleur du rôle
RANK_TIERS = [(1400, "Diamond", 0xB9F2FF), (1250, "Emerald", 0x2ECC71), (1100, "Sapphire", 0x3498DB),
              (1000, "Ruby", 0xE74C3C), (0, "Pearl", 0xECEFF1)]


def normalize(name):
    """Nom de salon comparé sans casse ni espaces autour."""
    return name.strip().casefold()


def is_create_channel(channel_name, configured_name=CREATE_CHANNEL_NAME):
    if channel_name is None:
        return False
    names = [configured_name] + CREATE_CHANNEL_ALIASES
    return any(normalize(channel_name) == normalize(name) for name in names)


def temp_channel_name(display_name, lang="en"):
    """Nom du salon temporaire (100 caractères au plus pour Discord)."""
    name = tr("temp_channel", lang, name=display_name.strip() or tr("player", lang))
    return name[:100]


def should_delete(is_temp_channel, member_count):
    """Un salon temporaire vide doit être supprimé (après DELETE_DELAY_SECONDS)."""
    return is_temp_channel and member_count == 0


def load_state(path):
    """État enregistré du bot : {"channels": [ids des salons temporaires],
    "ranking": {id du serveur: id du message du classement}}.
    Un fichier absent ou abîmé donne un état vide ; l'ancien format (simple
    liste d'identifiants) est encore accepté."""
    state = {"channels": [], "ranking": {}}
    try:
        with open(path, encoding="utf-8") as file:
            data = json.load(file)
    except (OSError, ValueError):
        return state
    if isinstance(data, list):
        data = {"channels": data}
    if not isinstance(data, dict):
        return state
    channels = data.get("channels", [])
    if isinstance(channels, list):
        state["channels"] = [int(i) for i in channels if isinstance(i, int) and not isinstance(i, bool)]
    ranking = data.get("ranking", {})
    if isinstance(ranking, dict):
        for guild, message in ranking.items():
            if str(guild).isdigit() and isinstance(message, int) and not isinstance(message, bool):
                state["ranking"][int(guild)] = message
    return state


def save_state(path, channel_ids, ranking_messages):
    """Enregistre l'état (fichier temporaire puis renommage : jamais à moitié écrit)."""
    data = {"channels": sorted(channel_ids),
            "ranking": {str(guild): message for guild, message in ranking_messages.items()}}
    temporary = path + ".tmp"
    with open(temporary, "w", encoding="utf-8") as file:
        json.dump(data, file)
    os.replace(temporary, path)


def normalize_channel(name):
    """Nom de salon sans accents, emojis ni séparateurs : « 📊・Modération » -> « moderation »."""
    text = unicodedata.normalize("NFKD", name or "").casefold()
    return "".join(c for c in text if c.isascii() and (c.isalnum() or c == "-"))


def channel_matches(channel_name, wanted):
    """Le salon « 🏆・classement » convient pour « classement »."""
    target = normalize_channel(wanted)
    return bool(target) and target in normalize_channel(channel_name)


def parse_id(text):
    """Identifiant Discord (salon, rôle) écrit dans un réglage, ou None s'il est vide ou mal écrit."""
    text = (text or "").strip()
    return int(text) if text.isdigit() and 15 <= len(text) <= 21 else None


def can_moderate(role_names, is_admin, moderator_role, role_ids=(), moderator_role_id=None):
    """Modérateur : administrateur du serveur, ou rôle de modération (par
    identifiant s'il est réglé, sinon par nom)."""
    if is_admin:
        return True
    if moderator_role_id is not None:
        return moderator_role_id in role_ids
    wanted = normalize_channel(moderator_role)
    return any(normalize_channel(role) == wanted for role in role_names)


def win_rate(player):
    games = player.get("games", 0)
    return player.get("wins", 0) * 100 // games if games else 0


def format_ranking(players, updated_at=None, lang="en"):
    """Texte du top 10 (salon #classement et /leaderboard)."""
    lines = [tr("ranking_title", lang, size=RANKING_SIZE), ""]
    if not players:
        lines.append(tr("ranking_empty", lang))
    for index, player in enumerate(players[:RANKING_SIZE]):
        place = MEDALS[index] if index < len(MEDALS) else "`{:>2}.`".format(index + 1)
        lines.append(tr("ranking_line", lang, place=place, pseudo=player.get("pseudo", "?"), elo=player.get("elo", 0),
                        games=player.get("games", 0), rate=win_rate(player)))
    lines.append("")
    footer = tr("ranking_footer", lang, url=site_page("leaderboard", lang))
    if updated_at is not None:
        footer += tr("ranking_updated", lang, when="<t:{}:R>".format(int(updated_at)))
    lines.append(footer)
    return "\n".join(lines)


def format_podium(top):
    """« 🥇 Alice (1320) · 🥈 Bob (1250) · 🥉 Sam (1190) » pour l'annonce de fin de saison."""
    parts = []
    for index, player in enumerate(top[:3]):
        parts.append("{} **{}** ({})".format(MEDALS[index], player.get("pseudo", "?"), player.get("elo", 0)))
    return " · ".join(parts) if parts else "—"


def rank_tier(elo):
    """Nom du rôle de rang pour cet Elo (« Ruby » à partir de 1000...)."""
    for minimum, name, _color in RANK_TIERS:
        if elo >= minimum:
            return name
    return RANK_TIERS[-1][1]


def rank_role_changes(current_roles, elo):
    """(rôle à ajouter ou None, rôles de rang à retirer) pour un membre qui a
    les rôles `current_roles` (noms) et cet Elo."""
    wanted = rank_tier(elo)
    tier_names = [name for _minimum, name, _color in RANK_TIERS]
    to_remove = [name for name in current_roles if name in tier_names and name != wanted]
    return (None if wanted in current_roles else wanted), to_remove


def site_page(page, lang="en"):
    """Adresse d'une page du site, dans la bonne langue."""
    path = {"leaderboard": "/leaderboard", "home": "/"}.get(page, "/")
    return SITE_URL + path + ("?lang=fr" if lang == "fr" else "")


def format_date(timestamp):
    """Date Discord (affichée dans le fuseau et la langue de chacun)."""
    return "<t:{}:f>".format(int(timestamp))


def format_profile(profile, lang="en"):
    """Texte de /profile."""
    if not profile.get("found") or not profile.get("pseudo"):
        return tr("no_player", lang)
    rank = profile.get("rank", 0)
    lines = ["**{}** — {} Elo{}".format(profile["pseudo"], profile.get("elo", 1000),
                                       tr("profile_rank", lang, rank=rank) if rank else ""),
             tr("profile_stats", lang, games=profile.get("games", 0), wins=profile.get("wins", 0),
                rate=win_rate(profile))]
    extras = []
    if profile.get("achievements"):
        extras.append(tr("profile_achievements", lang, count=profile["achievements"]))
    badges = ["S{} #{}".format(b.get("season", 0), b.get("rank", 0)) for b in profile.get("badges") or []]
    if badges:
        extras.append(tr("profile_badges", lang, badges=", ".join(badges)))
    if extras:
        lines.append(" · ".join(extras))
    if profile.get("banned"):
        if profile.get("banned_until") == -1:
            lines.append(tr("banned_forever", lang))
        else:
            lines.append(tr("banned_until", lang, date=format_date(profile.get("banned_until", 0))))
    return "\n".join(lines)


def frequent_opponents(history, now=None, days=7):
    """[(pseudo, nombre de parties)] contre les mêmes adversaires ces derniers jours
    (indice de multi-comptes ou d'entente), du plus fréquent au moins fréquent."""
    now = time.time() if now is None else now
    me = history.get("pseudo", "")
    counter = Counter()
    for game in history.get("games_list", []):
        if game.get("cancelled") or now - game.get("finished_at", 0) > days * 86400:
            continue
        for player in game.get("players", []):
            if player != me:
                counter[player] += 1
    return counter.most_common()


def format_history(history, now=None, lang="en"):
    """Texte de /mod history (modérateurs)."""
    if not history.get("found"):
        return tr("player_not_found", lang)
    lines = [format_profile(history, lang), ""]
    games = history.get("games_list", [])
    if not games:
        lines.append(tr("no_games", lang))
    for game in games:
        change = game.get("elo_change", 0)
        lines.append(tr("history_line", lang, id=game.get("id"), date=format_date(game.get("finished_at", 0)),
                        mode=game.get("mode", "?"), place=game.get("place", 0) + 1, score=game.get("score", 0),
                        change=("+" if change >= 0 else "") + str(change), players=", ".join(game.get("players", [])),
                        cancelled=tr("cancelled", lang) if game.get("cancelled") else ""))
    suspicious = [(name, count) for name, count in frequent_opponents(history, now) if count >= 5]
    if suspicious:
        lines.append("")
        lines.append(tr("frequent", lang, list=", ".join(
            tr("frequent_item", lang, name=name, count=count) for name, count in suspicious)))
    return "\n".join(lines)[:1900]


def ban_duration_text(days, lang="en"):
    return tr("forever", lang) if int(days) == 0 else tr("for_days", lang, days=days)


class TempChannel:
    """Ce que le bot sait d'un salon temporaire qu'il a créé."""

    def __init__(self, owner_id, lang="en"):
        self.owner_id = owner_id
        self.lang = lang          # langue du serveur Discord
        self.code = None          # code du salon Gemspire lié
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
        return 0, tr("lobby_closed", temp.lang)
    temp.code = event.get("code")
    seats = int(event.get("seats", 0))
    message = None
    if event.get("started") and not temp.started:
        temp.started = True
        message = tr("lobby_started", temp.lang, code=temp.code)
    elif not temp.announced:
        temp.announced = True
        message = tr("lobby_created", temp.lang, code=temp.code, seats=seats)
    return seats, message


def offer_message(presence, lang="en"):
    """Message écrit dans le salon temporaire selon la présence du joueur."""
    if not presence.get("linked"):
        return tr("offer_not_linked", lang)
    if not presence.get("online"):
        return tr("offer_not_online", lang)
    return tr("offer_sent", lang)


def format_status(status, lang="en"):
    """État du serveur pour /dev status (plusieurs lignes)."""
    uptime = int(status.get("uptime", 0))
    lines = [
        tr("status_title", lang, version=status.get("version", "?"), minimum=status.get("min_version", "?"),
           hours=uptime // 3600, minutes=uptime % 3600 // 60),
        tr("status_live", lang, online=status.get("online", 0), games=status.get("games_live", 0),
           lobbies=status.get("lobbies", 0), queue=status.get("queue", 0)),
        tr("status_totals", lang, accounts=status.get("accounts", 0), games=status.get("games_total", 0),
           today=status.get("games_today", 0), banned=status.get("banned", 0), season=status.get("season", 0)),
        tr("status_switches", lang,
           signups=tr("closed" if status.get("signups_closed") else "open", lang),
           games=tr("paused" if status.get("games_paused") else "allowed", lang)),
    ]
    if status.get("announcement"):
        lines.append(tr("status_announcement", lang, text=status["announcement"]))
    return "\n".join(lines)
