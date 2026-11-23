"""Test d'intégration : le client du bot parle au vrai splendor-server.

Lance ./splendor-server (connexion de test + clé du bot), connecte un joueur
« Discord », vérifie présence, proposition reçue par le jeu et événements du
salon vocal. Lancer depuis la racine : python3 -m unittest bot/test_integration.py
"""

import json
import os
import socket
import subprocess
import sys
import tempfile
import time
import unittest
import urllib.error
import urllib.request

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from logic import format_status
from api import ApiError, SplendorApi  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SERVER = os.path.join(ROOT, "splendor-server")


def free_port():
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


@unittest.skipUnless(os.path.exists(SERVER), "compiler d'abord splendor-server")
class IntegrationTests(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.port = free_port()
        self.url = "http://127.0.0.1:{}".format(self.port)
        env = dict(os.environ, SPLENDOR_PORT=str(self.port), SPLENDOR_DEV_LOGIN="1", SPLENDOR_BOT_KEY="cle-integration",
                   SPLENDOR_ADMIN_DISCORD_IDS="405,406", SPLENDOR_DATABASE=os.path.join(self.folder.name, "test.db"))
        self.process = subprocess.Popen([SERVER], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        for _ in range(50):
            try:
                urllib.request.urlopen(self.url + "/health", timeout=1).close()
                break
            except OSError:
                time.sleep(0.1)

    def tearDown(self):
        self.process.terminate()
        self.process.wait(timeout=5)
        self.folder.cleanup()

    def call(self, path, token=None, body=None):
        data = json.dumps(body).encode() if body is not None else None
        request = urllib.request.Request(self.url + path, data=data, method="POST" if data else "GET")
        request.add_header("X-Splendor-Version", "99.0.0")  # comme un jeu à jour
        if token:
            request.add_header("Authorization", "Bearer " + token)
        with urllib.request.urlopen(request, timeout=40) as response:
            return json.loads(response.read().decode())

    def test_offer_reaches_the_game(self):
        login = "z" * 24
        urllib.request.urlopen(self.url + "/auth/dev/start?login={}&name=4242&as=discord".format(login)).close()
        token = self.call("/auth/poll?login=" + login)["token"]
        self.call("/me/pseudo", token, {"pseudo": "Pierre"})
        last = self.call("/me/notifications", token)["last"]  # le jeu est « ouvert »

        api = SplendorApi(self.url, "cle-integration")
        self.assertEqual(api.presence(4242), {"linked": True, "online": True, "pseudo": "Pierre"})
        self.assertFalse(api.presence(1)["linked"])
        self.assertTrue(api.offer(4242, 555, "Partie de Pierre"))

        notes = self.call("/me/notifications?since={}".format(last), token)["notifications"]
        self.assertEqual(notes[0]["type"], "voice_offer")
        self.assertEqual(notes[0]["voice_channel"], "555")

        code = self.call("/lobbies", token, {"mode": "rapide", "seats": 4, "voice_channel": "555"})["code"]
        event = api.events(0)["events"][-1]
        self.assertEqual((event["code"], event["seats"], event["voice_channel"]), (code, 4, "555"))

    def test_challenge_and_ranks(self):
        tokens = {}
        for discord_id, name in ((901, "Defieur"), (902, "Defie")):
            login = str(discord_id) * 8
            urllib.request.urlopen(self.url + "/auth/dev/start?login={}&name={}&as=discord".format(login, discord_id)).close()
            tokens[discord_id] = self.call("/auth/poll?login=" + login)["token"]
            self.call("/me/pseudo", tokens[discord_id], {"pseudo": name})
        last = self.call("/me/notifications", tokens[902])["last"]
        api = SplendorApi(self.url, "cle-integration")
        result = api.challenge(901, 902, "rapide")
        self.assertEqual((result["pseudo"], result["target_linked"]), ("Defieur", True))
        lobby = self.call("/lobbies/" + result["code"], tokens[901])
        self.assertEqual((lobby["seats"], lobby["mode"], len(lobby["members"])), (2, "rapide", 1))
        note = self.call("/me/notifications?since={}".format(last), tokens[902])["notifications"][0]
        self.assertEqual((note["type"], note["from"], note["code"]), ("challenge", "Defieur", result["code"]))
        with self.assertRaises(ApiError):
            api.challenge(999, 902, "classique")  # pas de compte lié
        self.assertEqual(api.ranks(), [])        # personne n'a encore joué
        season = api.end_season()
        self.assertEqual((season["ended"], season["season"], season["top"]), (1, 2, []))

    def test_moderation(self):
        login = "y" * 24
        urllib.request.urlopen(self.url + "/auth/dev/start?login={}&name=777&as=discord".format(login)).close()
        token = self.call("/auth/poll?login=" + login)["token"]
        self.call("/me/pseudo", token, {"pseudo": "Tricheur"})
        api = SplendorApi(self.url, "cle-integration")
        self.assertEqual(api.profile(discord_id=777)["pseudo"], "Tricheur")
        self.assertEqual(api.history(pseudo="tricheur")["games_list"], [])
        self.assertEqual(api.leaderboard(), [])
        banned = api.ban("Tricheur", 3, "multi-comptes")
        self.assertTrue(banned["banned"])
        with self.assertRaises(urllib.error.HTTPError):
            self.call("/me", token)  # session fermée
        self.assertFalse(api.unban("Tricheur")["banned"])
        with self.assertRaises(ApiError) as caught:
            api.ban("Personne", 1)
        self.assertEqual(str(caught.exception), "Player not found.")
        with self.assertRaises(ApiError):
            api.cancel_game(12345)
        api.delete_account(pseudo="Tricheur")
        self.assertFalse(api.profile(pseudo="Tricheur")["found"])

    def test_dev_commands(self):
        login = "d" * 24
        urllib.request.urlopen(self.url + "/auth/dev/start?login={}&name=900&as=discord".format(login)).close()
        token = self.call("/auth/poll?login=" + login)["token"]
        self.call("/me/pseudo", token, {"pseudo": "Joueuse"})
        api = SplendorApi(self.url, "cle-integration")
        with self.assertRaises(ApiError) as caught:
            api.admin(999, "status")
        self.assertIn("administrator", str(caught.exception))
        status = api.admin(405, "status")
        self.assertEqual(status["accounts"], 1)
        self.assertIn("1 accounts", format_status(status))
        self.assertIn("Sign-ups: **open**", format_status(status))
        self.assertIn("New accounts closed", api.admin(406, "signups", value="closed")["message"])
        self.assertTrue(api.admin(405, "status")["signups_closed"])
        api.admin(405, "elo", username="joueuse", value=1300)
        self.assertEqual(api.profile(pseudo="Joueuse")["elo"], 1300)
        api.admin(405, "badge", username="Joueuse", season=2, rank=1)
        self.assertEqual(api.profile(pseudo="Joueuse")["badges"], [{"season": 2, "rank": 1}])
        api.admin(405, "rename", username="Joueuse", value="Championne")
        api.admin(405, "logout", username="Championne")
        with self.assertRaises(urllib.error.HTTPError):
            self.call("/me", token)
        api.admin(405, "announce", text="Bientôt la v4")
        self.assertEqual(self.call("/config")["announcement"], "Bientôt la v4")
        with self.assertRaises(ApiError):
            api.admin(405, "elo", username="Personne", value=1000)


if __name__ == "__main__":
    unittest.main()
