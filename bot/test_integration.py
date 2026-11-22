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
                   SPLENDOR_DATABASE=os.path.join(self.folder.name, "test.db"))
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
        self.assertEqual(str(caught.exception), "Joueur introuvable.")
        with self.assertRaises(ApiError):
            api.cancel_game(12345)
        api.delete_account(pseudo="Tricheur")
        self.assertFalse(api.profile(pseudo="Tricheur")["found"])


if __name__ == "__main__":
    unittest.main()
