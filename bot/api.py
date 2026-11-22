"""Appels du bot au serveur Gemspire (bibliothèque standard uniquement).

Le bot s'identifie avec la clé partagée SPLENDOR_BOT_KEY (en-tête X-Bot-Key).
"""

import json
import urllib.error
import urllib.parse
import urllib.request


class ApiError(RuntimeError):
    """Refus du serveur, avec son message (« Joueur introuvable. »...)."""


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
        """[{"pseudo", "elo", "games", "wins"}, ...] (50 meilleurs)"""
        return self._request("GET", "/leaderboard").get("players", [])

    def profile(self, pseudo=None, discord_id=None):
        """{"found", "pseudo", "elo", "games", "wins", "rank", "banned", "banned_until", "ban_reason"}"""
        return self._request("GET", "/bot/profile?" + urllib.parse.urlencode(self._who(pseudo, discord_id)))

    def history(self, pseudo=None, discord_id=None):
        """Profil + "games_list" : 15 dernières parties (id, mode, finished_at, place, players...)"""
        return self._request("GET", "/bot/history?" + urllib.parse.urlencode(self._who(pseudo, discord_id)))

    # --- Modération
    def ban(self, pseudo, days, reason=""):
        """Suspend le compte `days` jours (0 : définitif)."""
        return self._request("POST", "/bot/ban", {"pseudo": pseudo, "days": int(days), "reason": reason})

    def unban(self, pseudo):
        return self._request("POST", "/bot/unban", {"pseudo": pseudo})

    def cancel_game(self, game_id):
        return self._request("POST", "/bot/cancel-game", {"game_id": int(game_id)})

    def delete_account(self, pseudo=None, discord_id=None):
        return self._request("POST", "/bot/delete-account", self._who(pseudo, discord_id))
