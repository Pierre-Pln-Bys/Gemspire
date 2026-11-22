"""Textes du bot en anglais (par défaut) et en français.

Chaque membre voit les réponses dans la langue de son Discord ; les messages
postés dans un salon utilisent la langue du serveur Discord.
"""

LANGUAGES = ("en", "fr")

TEXTS = {
    # --- Salons vocaux
    "temp_channel": {"en": "{name}'s game", "fr": "Partie de {name}"},
    "player": {"en": "player", "fr": "joueur"},
    "offer_not_linked": {
        "en": "To create a Splendor game here, sign in to the game with **Discord** (Play online > Sign in with Discord).",
        "fr": "Pour créer une partie Splendor ici, connectez-vous dans le jeu avec **Discord** "
              "(Jouer en ligne > Se connecter avec Discord)."},
    "offer_not_online": {
        "en": "Start Splendor (signed in with Discord): the game setup will open in the game. I'll wait 2 minutes.",
        "fr": "Lancez Splendor (connecté avec Discord) : la création de la partie s'ouvrira dans le jeu. "
              "J'attends 2 minutes."},
    "offer_sent": {"en": "Set up the game in Splendor: the window just opened in the game.",
                   "fr": "Configurez la partie dans Splendor : la fenêtre vient de s'ouvrir dans le jeu."},
    "lobby_closed": {"en": "The Splendor game linked to this channel was cancelled.",
                     "fr": "La partie Splendor liée à ce salon a été annulée."},
    "lobby_started": {"en": "Splendor game **{code}** has started. Have fun!",
                      "fr": "La partie Splendor **{code}** a commencé. Bon jeu !"},
    "lobby_created": {
        "en": "Splendor game created: code **{code}** ({seats} seats). Join this voice channel, then in Splendor: "
              "Play online > Join.",
        "fr": "Partie Splendor créée : code **{code}** ({seats} places). Rejoignez ce salon vocal, puis dans "
              "Splendor : Jouer en ligne > Rejoindre."},
    # --- Classement et profils
    "ranking_title": {"en": "🏆 **Splendor leaderboard** — top {size}", "fr": "🏆 **Classement Splendor** — top {size}"},
    "ranking_empty": {"en": "Nobody has finished an online game yet. Be the first!",
                      "fr": "Personne n'a encore terminé de partie en ligne. Soyez le premier !"},
    "ranking_line": {"en": "{place} **{pseudo}** — {elo} Elo · {games} games · {rate}% wins",
                     "fr": "{place} **{pseudo}** — {elo} Elo · {games} parties · {rate} % de victoires"},
    "ranking_footer": {"en": "Full leaderboard: <{url}>", "fr": "Classement complet : <{url}>"},
    "ranking_updated": {"en": " · updated {when}", "fr": " · mis à jour {when}"},
    "no_player": {"en": "No Splendor player found.", "fr": "Aucun joueur Splendor trouvé."},
    "profile_rank": {"en": " · #{rank} on the leaderboard", "fr": " · {rank}ᵉ au classement"},
    "profile_stats": {"en": "{games} games · {wins} wins ({rate}%)", "fr": "{games} parties · {wins} victoires ({rate} %)"},
    "banned_forever": {"en": "⛔ Account suspended permanently", "fr": "⛔ Compte suspendu définitivement"},
    "banned_until": {"en": "⛔ Account suspended until {date}", "fr": "⛔ Compte suspendu jusqu'au {date}"},
    "not_linked_member": {
        "en": "This member hasn't linked Splendor to Discord (in the game: Play online > Sign in with Discord).",
        "fr": "Ce membre n'a pas lié Splendor à Discord (dans le jeu : Jouer en ligne > Se connecter avec Discord)."},
    # --- Historique (modérateurs)
    "player_not_found": {"en": "Player not found.", "fr": "Joueur introuvable."},
    "no_games": {"en": "No online games.", "fr": "Aucune partie en ligne."},
    "history_line": {"en": "`#{id}` {date} · {mode} · place {place} ({score} pts) · {change} Elo · {players}{cancelled}",
                     "fr": "`#{id}` {date} · {mode} · {place}ᵉ ({score} pts) · {change} Elo · {players}{cancelled}"},
    "cancelled": {"en": " · ~~cancelled~~", "fr": " · ~~annulée~~"},
    "frequent": {"en": "⚠️ Very frequent opponents (7 days): {list}", "fr": "⚠️ Adversaires très fréquents (7 jours) : {list}"},
    "frequent_item": {"en": "{name} ({count} games)", "fr": "{name} ({count} parties)"},
    "forever": {"en": "permanently", "fr": "définitivement"},
    "for_days": {"en": "for {days} day(s)", "fr": "pour {days} jour(s)"},
    # --- Commandes
    "server_down": {"en": "❌ The Splendor server is not responding. Try again in a moment.",
                    "fr": "❌ Le serveur Splendor ne répond pas. Réessayez dans un moment."},
    "link_missing": {
        "en": "No Splendor account linked yet: in the game, **Play online > Sign in with Discord**, choose a "
              "username, then run `/link` again.",
        "fr": "Pas encore de compte Splendor lié : dans le jeu, **Jouer en ligne > Se connecter avec Discord**, "
              "choisissez un pseudo, puis relancez `/lier`."},
    "link_ok": {"en": "✅ Splendor account **{pseudo}** linked.", "fr": "✅ Compte Splendor **{pseudo}** lié."},
    "role_added": {"en": " Role **{role}** added.", "fr": " Rôle **{role}** ajouté."},
    "role_denied": {"en": " (the bot can't give the {role} role: move its role above it)",
                    "fr": " (le bot ne peut pas donner le rôle {role} : placez son rôle au-dessus)"},
    "play": {
        "en": "🎲 **Splendor** — Windows and Linux, a single file to run:\n• Download: <{download}>\n"
              "• Play in your browser, website and leaderboard: <{site}>\n"
              "• Join the **{channel}** voice channel: the bot sets up your game.",
        "fr": "🎲 **Splendor** — Windows et Linux, un seul fichier à lancer :\n• Téléchargement : <{download}>\n"
              "• Jouer dans le navigateur, site et classement : <{site}>\n"
              "• Rejoignez le salon vocal **{channel}** : le bot prépare votre partie."},
    "rules": {
        "en": "📜 **Rules in short** — on your turn, ONE action:\n"
              "• take 3 tokens of different colours, or 2 of the same colour (stack of at least 4);\n"
              "• buy a card (your cards lower the price);\n"
              "• in classic mode: reserve a card and get 1 gold (wild).\n"
              "10 tokens at most. Nobles visit when you own enough cards. At 15 points, the round is finished. "
              "Online: 2 minutes per turn.\nFull rules: <{wiki}>",
        "fr": "📜 **Règles en bref** — à votre tour, UNE action :\n"
              "• prendre 3 jetons de couleurs différentes, ou 2 identiques (pile d'au moins 4) ;\n"
              "• acheter une carte (vos cartes réduisent le prix) ;\n"
              "• en classique : réserver une carte et gagner 1 or (joker).\n"
              "10 jetons au maximum. Les nobles viennent quand vous avez assez de cartes. À 15 points, on finit "
              "le tour de table. En ligne : 2 minutes par tour.\nRègles complètes : <{wiki}>"},
    "report_log": {"en": "🚩 **Report** from {user} against **{pseudo}**: {reason}",
                   "fr": "🚩 **Signalement** de {user} contre **{pseudo}** : {reason}"},
    "report_ok": {"en": "Thanks, the moderators received your report.",
                  "fr": "Merci, les modérateurs ont reçu votre signalement."},
    "mods_only": {"en": "Moderators only.", "fr": "Réservé aux modérateurs."},
    "ban_ok": {"en": "⛔ **{pseudo}** suspended {duration}: {reason}", "fr": "⛔ **{pseudo}** suspendu {duration} : {reason}"},
    "ban_log": {"en": "⛔ {user} suspended **{pseudo}** {duration} — {reason}",
                "fr": "⛔ {user} a suspendu **{pseudo}** {duration} — {reason}"},
    "unban_ok": {"en": "✅ **{pseudo}** reactivated.", "fr": "✅ **{pseudo}** réactivé."},
    "unban_log": {"en": "✅ {user} reactivated **{pseudo}**", "fr": "✅ {user} a réactivé **{pseudo}**"},
    "cancel_ok": {"en": "↩️ Game #{number} cancelled: Elo, games and wins restored.",
                  "fr": "↩️ Partie #{number} annulée : Elo, parties et victoires rendus."},
    "cancel_log": {"en": "↩️ {user} cancelled game #{number}", "fr": "↩️ {user} a annulé la partie #{number}"},
    "delete_ok": {"en": "🗑️ Account **{pseudo}** deleted.", "fr": "🗑️ Compte **{pseudo}** supprimé."},
    "delete_log": {"en": "🗑️ {user} deleted the account **{pseudo}**", "fr": "🗑️ {user} a supprimé le compte **{pseudo}**"},
}

# Noms et descriptions des commandes : anglais dans le code, français ici
# (Discord affiche la traduction aux membres dont Discord est en français)
COMMAND_FR = {
    "leaderboard": "classement",
    "Top 10 Splendor online players": "Top 10 des joueurs Splendor en ligne",
    "profile": "profil",
    "Elo, rank and wins of a player": "Elo, rang et victoires d'un joueur",
    "member": "membre",
    "Discord member (default: you)": "Membre Discord (par défaut : vous)",
    "username": "pseudo",
    "Or Splendor username": "Ou pseudo Splendor",
    "Splendor username": "Pseudo Splendor",
    "link": "lier",
    "Link your Splendor account to Discord and get the Player role":
        "Relie votre compte Splendor à Discord et donne le rôle Joueur",
    "play": "jouer",
    "Download Splendor or play in your browser": "Télécharger Splendor ou jouer dans le navigateur",
    "rules": "regles",
    "Splendor rules in short": "Les règles de Splendor en bref",
    "report": "signaler",
    "Report a player to the moderators (cheating, username...)":
        "Signaler un joueur aux modérateurs (triche, pseudo...)",
    "reason": "raison",
    "What happened": "Ce qui s'est passé",
    "mod": "mod",
    "Splendor moderation": "Modération de Splendor",
    "ban": "ban",
    "Suspend a Splendor account (0 days = permanent)": "Suspendre un compte Splendor (0 jour = définitif)",
    "days": "jours",
    "Duration in days (0 = permanent)": "Durée en jours (0 = définitif)",
    "Reason": "Motif",
    "unban": "unban",
    "Reactivate a Splendor account": "Réactiver un compte Splendor",
    "history": "historique",
    "Last 15 games of a player (spot cheating)": "15 dernières parties d'un joueur (repérer la triche)",
    "cancel-game": "annuler-partie",
    "Cancel the Elo of a rigged game (number from /mod history)":
        "Annuler l'Elo d'une partie truquée (numéro de /mod historique)",
    "number": "numero",
    "Game number": "Numéro de la partie",
    "delete-account": "supprimer-compte",
    "Delete a Splendor account (GDPR request)": "Supprimer un compte Splendor (demande RGPD)",
}


def lang_of(locale):
    """« fr », « fr-FR » -> "fr" ; tout le reste -> "en"."""
    return "fr" if str(locale or "").lower().startswith("fr") else "en"


def tr(key, lang="en", **values):
    """Texte `key` dans la langue `lang` (anglais si inconnue), avec ses valeurs."""
    entry = TEXTS[key]
    return entry.get(lang, entry["en"]).format(**values)
