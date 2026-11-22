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
import tempfile  # noqa: E402

from api import ApiError  # noqa: E402
from logic import (TempChannel, apply_event, can_moderate, channel_matches, format_history, parse_id,  # noqa: E402
                   format_profile, format_ranking, frequent_opponents, is_create_channel, load_state, offer_message,
                   save_state, should_delete, temp_channel_name)


class LogicTests(unittest.TestCase):
    def test_create_channel_name(self):
        self.assertTrue(is_create_channel("Crée ta partie"))
        self.assertTrue(is_create_channel("create a game"))
        self.assertTrue(is_create_channel("  crée TA partie "))
        self.assertFalse(is_create_channel("Général"))
        self.assertFalse(is_create_channel(None))

    def test_temp_channel(self):
        self.assertEqual(temp_channel_name("Pierre"), "Pierre's game")
        self.assertEqual(temp_channel_name("Pierre", "fr"), "Partie de Pierre")
        self.assertEqual(temp_channel_name("   ", "fr"), "Partie de joueur")
        self.assertEqual(len(temp_channel_name("x" * 300)), 100)
        self.assertTrue(should_delete(True, 0))
        self.assertFalse(should_delete(True, 1))
        self.assertFalse(should_delete(False, 0))

    def test_state_file(self):
        with tempfile.TemporaryDirectory() as folder:
            path = os.path.join(folder, "temp_channels.json")
            self.assertEqual(load_state(path), {"channels": [], "ranking": {}})  # pas encore de fichier
            save_state(path, {1574139585007718474, 42}, {111: 222})
            self.assertEqual(load_state(path), {"channels": [42, 1574139585007718474], "ranking": {111: 222}})
            with open(path, "w", encoding="utf-8") as file:
                file.write("{abîmé")
            self.assertEqual(load_state(path)["channels"], [])
            with open(path, "w", encoding="utf-8") as file:
                file.write('[1, "2", true, 3]')  # ancien format : simple liste
            self.assertEqual(load_state(path), {"channels": [1, 3], "ranking": {}})

    def test_channel_names_and_roles(self):
        self.assertTrue(channel_matches("🏆・classement", "classement"))
        self.assertTrue(channel_matches("Modération", "moderation"))
        self.assertFalse(channel_matches("général", "classement"))
        self.assertFalse(channel_matches("classement", ""))
        self.assertTrue(can_moderate(["@everyone", "Modérateur"], False, "Modérateur"))
        self.assertTrue(can_moderate(["moderateur"], False, "Modérateur"))
        self.assertTrue(can_moderate([], True, "Modérateur"))
        self.assertFalse(can_moderate(["Joueur"], False, "Modérateur"))
        # Rôle réglé par identifiant : le nom ne suffit plus
        self.assertTrue(can_moderate(["X"], False, "Modérateur", [111, 1234567890123456789], 1234567890123456789))
        self.assertFalse(can_moderate(["Modérateur"], False, "Modérateur", [111], 1234567890123456789))

    def test_parse_id(self):
        self.assertEqual(parse_id(" 1574139585007718474 "), 1574139585007718474)
        self.assertIsNone(parse_id(""))
        self.assertIsNone(parse_id(None))
        self.assertIsNone(parse_id("classement"))
        self.assertIsNone(parse_id("123"))

    def test_ranking_and_profile(self):
        players = [{"pseudo": "P{}".format(i), "elo": 1200 - i, "games": 4, "wins": 1} for i in range(12)]
        text = format_ranking(players, 1800000000)
        self.assertIn("🥇 **P0**", text)
        self.assertIn("`10.` **P9**", text)
        self.assertNotIn("P10", text)  # top 10 seulement
        self.assertIn("25% wins", text)
        self.assertIn("<t:1800000000:R>", text)
        self.assertIn("/leaderboard>", text)
        french = format_ranking(players, 1800000000, "fr")
        self.assertIn("25 % de victoires", french)
        self.assertIn("/leaderboard?lang=fr>", french)
        self.assertIn("Be the first", format_ranking([]))
        self.assertIn("Soyez le premier", format_ranking([], lang="fr"))
        profile = {"found": True, "pseudo": "Pierre", "elo": 1043, "games": 10, "wins": 7, "rank": 2}
        self.assertIn("#2 on the leaderboard", format_profile(profile))
        self.assertIn("2ᵉ au classement", format_profile(profile, "fr"))
        self.assertIn("70 %", format_profile(profile, "fr"))
        self.assertIn("suspended permanently", format_profile(dict(profile, banned=True, banned_until=-1)))
        self.assertIn("suspendu définitivement", format_profile(dict(profile, banned=True, banned_until=-1), "fr"))
        self.assertIn("No Gemspire player", format_profile({"found": False}))

    def test_history_flags_repeated_opponents(self):
        now = 1800000000
        games = [{"id": i, "mode": "rapide", "finished_at": now - 3600 * i, "place": 0, "score": 15,
                  "elo_change": 16, "cancelled": False, "players": ["Pierre", "Alt"]} for i in range(6)]
        games.append({"id": 99, "mode": "classique", "finished_at": now - 30 * 86400, "place": 1, "score": 9,
                      "elo_change": -12, "cancelled": False, "players": ["Autre", "Pierre"]})
        history = {"found": True, "pseudo": "Pierre", "elo": 1100, "games": 7, "wins": 6, "games_list": games}
        self.assertEqual(frequent_opponents(history, now), [("Alt", 6)])  # la partie d'il y a 30 jours ne compte pas
        text = format_history(history, now)
        self.assertIn("Alt (6 games)", text)
        self.assertIn("`#99`", text)
        self.assertIn("-12 Elo", text)
        self.assertIn("Alt (6 parties)", format_history(history, now, "fr"))
        self.assertEqual(format_history({"found": False}), "Player not found.")
        self.assertEqual(format_history({"found": False}, lang="fr"), "Joueur introuvable.")

    def test_events(self):
        temp = TempChannel(owner_id=1, lang="fr")
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
        self.assertIn("Start Gemspire", offer_message({"linked": True, "online": False}))
        self.assertIn("Lancez", offer_message({"linked": True, "online": False}, "fr"))
        self.assertIn("fenêtre", offer_message({"linked": True, "online": True}, "fr"))

    def test_texts_complete(self):
        # Chaque texte existe dans les deux langues, avec les mêmes valeurs à remplir
        import re
        from texts import COMMAND_FR, TEXTS, lang_of
        for key, entry in TEXTS.items():
            self.assertEqual(set(entry), {"en", "fr"}, key)
            self.assertEqual(sorted(re.findall(r"{(\w+)}", entry["en"])), sorted(re.findall(r"{(\w+)}", entry["fr"])), key)
        for english, french in COMMAND_FR.items():
            self.assertTrue(french)
        self.assertEqual((lang_of("fr"), lang_of("fr-FR"), lang_of("en-US"), lang_of(None)), ("fr", "fr", "en", "en"))


class FakeServer(BaseHTTPRequestHandler):
    """Faux serveur Gemspire : vérifie la clé et répond comme le vrai."""

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
        with self.assertRaises(ApiError) as caught:
            SplendorApi(self.url, "mauvaise").presence(42)
        self.assertEqual(str(caught.exception), "Clé du bot invalide.")  # le message du serveur


if __name__ == "__main__":
    unittest.main()
