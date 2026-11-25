"""Appels du bot au serveur Gemspire (bibliothèque standard uniquement).

Le bot s'identifie avec la clé partagée SPLENDOR_BOT_KEY (en-tête X-Bot-Key).
"""

import json
import urllib.error
import urllib.parse
import urllib.request


class ApiError(RuntimeError):
    """Refus du serveur, avec son message (« Player not found. »...)."""


class SplendorApi:
    def __init__(self, base_url, bot_key, timeout=40):
        self.base_url = base_url.rstrip("/")
        self.bot_key = bot_key
        self.timeout = timeout

    def _request(self, method, path, body=None):
        data = json.dumps(body).encode("utf-8") if body is not None else None
        request = urllib.request.Request(self.base_url + path, data=data, method=method)
        request.add_header("X-Bot-Key", self.bot_key)
        if data is not None:
            request.add_header("Content-Type", "application/json")
        try:
            with urllib.request.urlopen(request, timeout=self.timeout) as response:
                return json.loads(response.read().decode("utf-8"))
        except urllib.error.HTTPError as error:
            message = "Serveur Gemspire : erreur {} sur {}".format(error.code, path.split("?")[0])
            try:
                message = json.loads(error.read().decode("utf-8")).get("error", message)
            except (ValueError, AttributeError):
                pass
            raise ApiError(message) from error

    @staticmethod
    def _who(pseudo=None, discord_id=None):
        """Paramètres qui désignent un joueur : par pseudo, sinon par compte Discord."""
        if pseudo:
            return {"pseudo": pseudo}
        return {"discord_id": str(discord_id)}

    def presence(self, discord_id):
        """{"linked": bool, "online": bool, "pseudo": str}"""
        return self._request("GET", "/bot/presence?discord_id=" + urllib.parse.quote(str(discord_id)))

    def offer(self, discord_id, voice_channel_id, channel_name):
        """Propose dans le jeu de créer une partie liée à ce salon vocal."""
        body = {"discord_id": str(discord_id), "voice_channel": str(voice_channel_id), "channel_name": channel_name}
        return self._request("POST", "/bot/offer", body).get("offered", False)

    def events(self, since):
        """Événements des salons liés à un salon vocal (attente longue)."""
        return self._request("GET", "/bot/events?since={}".format(int(since)))

    # --- Classement et profils
    def leaderboard(self):
        """[{"pseudo", "elo", "games", "wins"}, ...] (50 meilleurs, classement global)"""
        return self._request("GET", "/leaderboard").get("players", [])

    def rankings(self):
        """Les deux classements : {"players" (global), "season_players", "season",
        "season_ends_at", "season_min_games"}"""
        return self._request("GET", "/leaderboard")

    def profile(self, pseudo=None, discord_id=None):
        """{"found", "pseudo", "elo", "games", "wins", "rank", "banned", "banned_until", "ban_reason"}"""
        return self._request("GET", "/bot/profile?" + urllib.parse.urlencode(self._who(pseudo, discord_id)))

    def history(self, pseudo=None, discord_id=None):
        """Profil + "games_list" : 15 dernières parties (id, mode, finished_at, place, players...)"""
        return self._request("GET", "/bot/history?" + urllib.parse.urlencode(self._who(pseudo, discord_id)))

    def ranks(self):
        """[{"discord_id", "elo"}, ...] : joueurs liés à Discord et classés (rôles de rang)"""
        return self._request("GET", "/bot/ranks").get("players", [])

    def challenge(self, discord_id, target_discord_id, mode):
        """Crée un salon à 2 places pour `discord_id` ; {"code", "pseudo", "target_linked"}"""
        body = {"discord_id": str(discord_id), "target_discord_id": str(target_discord_id), "mode": mode}
        return self._request("POST", "/bot/challenge", body)

    # --- Modération (actor : identifiant Discord du modérateur, tracé côté serveur)
    def ban(self, pseudo, days, reason="", actor=""):
        """Suspend le compte `days` jours (0 : définitif)."""
        return self._request("POST", "/bot/ban",
                             {"pseudo": pseudo, "days": int(days), "reason": reason, "actor": str(actor)})

    def unban(self, pseudo, actor=""):
        return self._request("POST", "/bot/unban", {"pseudo": pseudo, "actor": str(actor)})

    def end_season(self):
        """Termine la saison : {"ended", "season", "top": [{"pseudo", "elo"}, ...]}"""
        return self._request("POST", "/bot/season/end", {})

    def cancel_game(self, game_id, actor=""):
        return self._request("POST", "/bot/cancel-game", {"game_id": int(game_id), "actor": str(actor)})

    def delete_account(self, pseudo=None, discord_id=None, actor=""):
        body = self._who(pseudo, discord_id)
        body["actor"] = str(actor)
        return self._request("POST", "/bot/delete-account", body)

    def report(self, reporter, pseudo, reason):
        """Garde un signalement /report pour le panel d'administration."""
        return self._request("POST", "/bot/report", {"reporter": reporter, "pseudo": pseudo, "reason": reason})

    # --- Commandes /dev (administrateurs seulement : le serveur vérifie le compte Discord)
    def admin(self, admin_discord_id, action, **fields):
        """Action d'administration ; {"message": ...} ou l'état du serveur (action "status")."""
        body = {"admin_discord_id": str(admin_discord_id), "action": action}
        body.update({key: str(value) for key, value in fields.items()})
        return self._request("POST", "/bot/admin", body)
