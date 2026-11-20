"""Tests du bot sans Discord : logique pure et appels au serveur (faux serveur).

Lancer : python3 -m unittest bot/test_bot.py   (ou make test-bot)
"""

import json
import os
import sys
import threading
import unittest
from http.server import BaseHTTPRequestHandler, HTTPServer

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from api import SplendorApi  # noqa: E402
from logic import TempChannel, apply_event, is_create_channel, offer_message, should_delete, temp_channel_name  # noqa: E402


class LogicTests(unittest.TestCase):
    def test_create_channel_name(self):
        self.assertTrue(is_create_channel("Crée ta partie"))
        self.assertTrue(is_create_channel("  crée TA partie "))
        self.assertFalse(is_create_channel("Général"))
        self.assertFalse(is_create_channel(None))

    def test_temp_channel(self):
        self.assertEqual(temp_channel_name("Pierre"), "Partie de Pierre")
        self.assertEqual(temp_channel_name("   "), "Partie de joueur")
        self.assertEqual(len(temp_channel_name("x" * 300)), 100)
        self.assertTrue(should_delete(True, 0))
        self.assertFalse(should_delete(True, 1))
        self.assertFalse(should_delete(False, 0))

    def test_events(self):
        temp = TempChannel(owner_id=1)
        limit, message = apply_event(temp, {"code": "K7Q2M", "seats": 3, "players": 1, "started": False, "closed": False})
        self.assertEqual(limit, 3)
        self.assertIn("K7Q2M", message)
        # Un joueur de plus : pas de nouveau message, même limite
        limit, message = apply_event(temp, {"code": "K7Q2M", "seats": 3, "players": 2, "started": False, "closed": False})
        self.assertEqual((limit, message), (3, None))
        limit, message = apply_event(temp, {"code": "K7Q2M", "seats": 3, "players": 2, "started": True, "closed": False})
        self.assertIn("commencé", message)
        limit, message = apply_event(temp, {"code": "K7Q2M", "seats": 3, "players": 0, "started": False, "closed": True})
        self.assertEqual(limit, 0)
        self.assertIsNone(temp.code)

    def test_offer_messages(self):
        self.assertIn("Discord", offer_message({"linked": False}))
        self.assertIn("Lancez", offer_message({"linked": True, "online": False}))
        self.assertIn("fenêtre", offer_message({"linked": True, "online": True}))


class FakeServer(BaseHTTPRequestHandler):
    """Faux serveur Splendor : vérifie la clé et répond comme le vrai."""

    received = []

    def log_message(self, *args):
        pass

    def reply(self, status, body):
        data = json.dumps(body).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        if self.headers.get("X-Bot-Key") != "cle":
            return self.reply(403, {"error": "Clé du bot invalide."})
        if self.path.startswith("/bot/presence"):
            return self.reply(200, {"linked": True, "online": "discord_id=42" in self.path, "pseudo": "Pierre"})
        if self.path.startswith("/bot/events"):
            return self.reply(200, {"last": 3, "events": [{"id": 3, "code": "ABCDE", "voice_channel": "9"}]})
        self.reply(404, {})

    def do_POST(self):
        length = int(self.headers.get("Content-Length", 0))
        FakeServer.received.append(json.loads(self.rfile.read(length)))
        self.reply(200, {"offered": True})


class ApiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = HTTPServer(("127.0.0.1", 0), FakeServer)
        threading.Thread(target=cls.server.serve_forever, daemon=True).start()
        cls.url = "http://127.0.0.1:{}".format(cls.server.server_address[1])

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()

    def test_presence_and_offer(self):
        api = SplendorApi(self.url, "cle")
        self.assertTrue(api.presence(42)["online"])
        self.assertFalse(api.presence(7)["online"])
        self.assertTrue(api.offer(42, 987, "Partie de Pierre"))
        self.assertEqual(FakeServer.received[-1], {"discord_id": "42", "voice_channel": "987",
                                                   "channel_name": "Partie de Pierre"})
        self.assertEqual(api.events(0)["events"][0]["code"], "ABCDE")

    def test_bad_key(self):
        with self.assertRaises(RuntimeError):
            SplendorApi(self.url, "mauvaise").presence(42)


if __name__ == "__main__":
    unittest.main()
