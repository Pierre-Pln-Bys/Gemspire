"""Appels du bot au serveur Splendor (bibliothèque standard uniquement).

Le bot s'identifie avec la clé partagée SPLENDOR_BOT_KEY (en-tête X-Bot-Key).
"""

import json
import urllib.error
import urllib.parse
import urllib.request


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
            raise RuntimeError("Serveur Splendor : erreur {} sur {}".format(error.code, path)) from error

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
