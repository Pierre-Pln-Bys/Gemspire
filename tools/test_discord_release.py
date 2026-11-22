"""Tests de tools/discord_release.py (sans envoyer de message).

Lancer : python3 -m unittest tools/test_discord_release.py  (ou make test-bot)
"""

import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from discord_release import changelog_section, main, release_message, to_discord  # noqa: E402

CHANGELOG = """# Changelog

## [2.0.0] - 2026-11-22

### Ajouté
- **Grande** nouveauté, décrite sur
  deux lignes.

### Corrigé
- Un bug.
- Bot :
  - une sous-puce
    sur deux lignes ;

## [1.0.0] - 2026-01-01

- Première version.
"""


class ReleaseTests(unittest.TestCase):
    def test_section(self):
        section = changelog_section(CHANGELOG, "2.0.0")
        self.assertIn("Grande", section)
        self.assertNotIn("Première version", section)
        self.assertEqual(changelog_section(CHANGELOG, "1.0.0"), "- Première version.")
        self.assertIsNone(changelog_section(CHANGELOG, "9.9.9"))

    def test_discord_format(self):
        text = to_discord(changelog_section(CHANGELOG, "2.0.0"))
        self.assertIn("**Ajouté**", text)
        self.assertIn("- **Grande** nouveauté, décrite sur deux lignes.", text)  # lignes recollées
        self.assertIn("**Corrigé**\n- Un bug.", text)
        self.assertIn("- Bot :\n  - une sous-puce sur deux lignes ;", text)

    def test_message(self):
        message = release_message("2.0.0", "- " + "x" * 5000)
        embed = message["embeds"][0]
        self.assertEqual(embed["title"], "Splendor v2.0.0 est disponible")
        self.assertTrue(embed["url"].endswith("/releases/tag/v2.0.0"))
        self.assertLessEqual(len(embed["description"]), 4096)
        self.assertEqual(message["allowed_mentions"], {"parse": []})  # jamais de @everyone

    def test_real_changelog_and_no_webhook(self):
        # Le vrai CHANGELOG a une section pour la version du jeu ; sans webhook, rien n'est envoyé
        root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        with open(os.path.join(root, "src", "version.hpp"), encoding="utf-8") as file:
            version = file.read().split('SPLENDOR_VERSION = "')[1].split('"')[0]
        with open(os.path.join(root, "CHANGELOG.md"), encoding="utf-8") as file:
            self.assertIsNotNone(changelog_section(file.read(), version))
        saved = os.environ.pop("DISCORD_RELEASE_WEBHOOK", None)
        try:
            self.assertEqual(main(["v" + version]), 0)
        finally:
            if saved is not None:
                os.environ["DISCORD_RELEASE_WEBHOOK"] = saved


if __name__ == "__main__":
    unittest.main()
