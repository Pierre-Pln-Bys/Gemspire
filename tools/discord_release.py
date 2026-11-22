"""Publie le changelog d'une version dans un salon Discord (webhook).

Utilisé par le workflow de release : python3 tools/discord_release.py v3.5.0
Le webhook est lu dans la variable d'environnement DISCORD_RELEASE_WEBHOOK
(secret GitHub, jamais dans le dépôt) ; sans elle, rien n'est envoyé.
--dry-run affiche le message au lieu de l'envoyer.
Bibliothèque standard uniquement.
"""

import json
import os
import sys
import urllib.request

REPOSITORY = "https://github.com/Pierre-Pln-Bys/ProjectSplendor"
SITE = "https://splendor.epsilon-forge.fr"
GOLD = 0xFFD140
DESCRIPTION_LIMIT = 4000  # Discord : 4096 caractères au plus dans un embed


def changelog_section(text, version):
    """Lignes de la section « ## [version] » du CHANGELOG (sans le titre), ou None."""
    lines = text.splitlines()
    start = None
    for index, line in enumerate(lines):
        if line.startswith("## [{}]".format(version)):
            start = index + 1
        elif start is not None and line.startswith("## ["):
            return "\n".join(lines[start:index]).strip()
    if start is None:
        return None
    return "\n".join(lines[start:]).strip()


def to_discord(markdown):
    """Markdown du CHANGELOG -> Markdown de Discord (titres ### en gras, lignes
    continuées d'une puce recollées)."""
    out = []
    for line in markdown.splitlines():
        if line.startswith("### "):
            out.append("")
            out.append("**" + line[4:].strip() + "**")
        elif line.lstrip().startswith("- ") and line.startswith("  "):
            out.append("  - " + line.strip()[2:])  # sous-puce
        elif line.startswith("  ") and out and out[-1].lstrip().startswith("- "):
            out[-1] += " " + line.strip()
        elif line.strip():
            out.append(line.rstrip())
        elif out and out[-1] != "":
            out.append("")
    return "\n".join(out).strip()


def release_message(version, section):
    """Contenu JSON du webhook : un embed avec le changelog et les liens."""
    tag = "v" + version
    release_url = "{}/releases/tag/{}".format(REPOSITORY, tag)
    description = to_discord(section)
    if len(description) > DESCRIPTION_LIMIT:
        description = description[:DESCRIPTION_LIMIT].rsplit("\n", 1)[0] + "\n…"
    downloads = "{0}/releases/download/{1}/splendor-windows.exe".format(REPOSITORY, tag)
    linux = "{0}/releases/download/{1}/splendor-linux".format(REPOSITORY, tag)
    return {
        "username": "Splendor",
        "allowed_mentions": {"parse": []},
        "embeds": [{
            "title": "Splendor {} est disponible".format(tag),
            "url": release_url,
            "color": GOLD,
            "description": description,
            "fields": [
                {"name": "Jouer", "value": "[Dans le navigateur]({}/play/)".format(SITE), "inline": True},
                {"name": "Télécharger", "value": "[Windows]({}) · [Linux]({})".format(downloads, linux), "inline": True},
                {"name": "Détails", "value": "[Notes de version]({})".format(release_url), "inline": True},
            ],
        }],
    }


def main(arguments):
    dry_run = "--dry-run" in arguments
    tags = [a for a in arguments if not a.startswith("--")]
    if len(tags) != 1:
        print("Usage : discord_release.py vX.Y.Z [--dry-run]", file=sys.stderr)
        return 2
    version = tags[0].lstrip("v")
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    with open(os.path.join(root, "CHANGELOG.md"), encoding="utf-8") as file:
        section = changelog_section(file.read(), version)
    if section is None:
        print("Pas de section {} dans CHANGELOG.md : rien à publier.".format(version))
        return 0
    message = release_message(version, section)
    if dry_run:
        print(json.dumps(message, ensure_ascii=False, indent=2))
        return 0
    webhook = os.environ.get("DISCORD_RELEASE_WEBHOOK", "").strip()
    if not webhook.startswith("https://discord.com/api/webhooks/"):
        print("DISCORD_RELEASE_WEBHOOK absent : changelog non publié sur Discord.")
        return 0
    request = urllib.request.Request(webhook, data=json.dumps(message).encode("utf-8"), method="POST",
                                     headers={"Content-Type": "application/json", "User-Agent": "splendor-release"})
    with urllib.request.urlopen(request, timeout=20) as response:
        print("Changelog publié sur Discord (HTTP {}).".format(response.status))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
