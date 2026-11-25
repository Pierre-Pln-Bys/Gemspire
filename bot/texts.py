"""Textes du bot en anglais (par défaut) et en français.

Chaque membre voit les réponses dans la langue de son Discord ; les messages
postés dans un salon utilisent la langue du serveur Discord.
"""

LANGUAGES = ("en", "fr")

TEXTS = {
    # --- Commandes /dev (administrateurs)
    "status_title": {"en": "🛠️ **Gemspire {version}** (min. game version {minimum}) · up {hours} h {minutes} min",
                     "fr": "🛠️ **Gemspire {version}** (version min. du jeu {minimum}) · lancé depuis {hours} h {minutes} min"},
    "status_live": {"en": "Now: {online} online · {games} games · {lobbies} lobbies · {queue} in matchmaking",
                    "fr": "En ce moment : {online} en ligne · {games} parties · {lobbies} salons · {queue} en recherche"},
    "status_totals": {"en": "{accounts} accounts · {games} games ({today} in 24 h) · {banned} banned · season {season}",
                      "fr": "{accounts} comptes · {games} parties ({today} en 24 h) · {banned} suspendus · saison {season}"},
    "status_switches": {"en": "Sign-ups: **{signups}** · new games: **{games}**",
                        "fr": "Inscriptions : **{signups}** · nouvelles parties : **{games}**"},
    "status_announcement": {"en": "Announcement: {text}", "fr": "Annonce : {text}"},
    "open": {"en": "open", "fr": "ouvertes"},
    "closed": {"en": "closed", "fr": "fermées"},
    "allowed": {"en": "allowed", "fr": "autorisées"},
    "paused": {"en": "paused", "fr": "suspendues"},
    "dev_done": {"en": "✅ {message}", "fr": "✅ {message}"},
    "dev_log": {"en": "🛠️ {user}: {message}", "fr": "🛠️ {user} : {message}"},
    # --- Salons vocaux
    "temp_channel": {"en": "{name}'s game", "fr": "Partie de {name}"},
    "player": {"en": "player", "fr": "joueur"},
    "offer_not_linked": {
        "en": "To create a Gemspire game here, sign in to the game with **Discord** (Play online > Sign in with Discord).",
        "fr": "Pour créer une partie Gemspire ici, connectez-vous dans le jeu avec **Discord** "
              "(Jouer en ligne > Se connecter avec Discord)."},
    "offer_not_online": {
        "en": "Start Gemspire (signed in with Discord): the game setup will open in the game. I'll wait 2 minutes.",
        "fr": "Lancez Gemspire (connecté avec Discord) : la création de la partie s'ouvrira dans le jeu. "
              "J'attends 2 minutes."},
    "offer_sent": {"en": "Set up the game in Gemspire: the window just opened in the game.",
                   "fr": "Configurez la partie dans Gemspire : la fenêtre vient de s'ouvrir dans le jeu."},
    "lobby_closed": {"en": "The Gemspire game linked to this channel was cancelled.",
                     "fr": "La partie Gemspire liée à ce salon a été annulée."},
    "lobby_started": {"en": "Gemspire game **{code}** has started. Have fun!",
                      "fr": "La partie Gemspire **{code}** a commencé. Bon jeu !"},
    "lobby_created": {
        "en": "Gemspire game created: code **{code}** ({seats} seats). Join this voice channel, then in Gemspire: "
              "Play online > Join.",
        "fr": "Partie Gemspire créée : code **{code}** ({seats} places). Rejoignez ce salon vocal, puis dans "
              "Gemspire : Jouer en ligne > Rejoindre."},
    # --- Classement et profils
    "ranking_title": {"en": "🌍 **Global leaderboard** — top {size}", "fr": "🌍 **Classement global** — top {size}"},
    "ranking_empty": {"en": "Nobody has finished a ranked game yet. Be the first!",
                      "fr": "Personne n'a encore terminé de partie classée. Soyez le premier !"},
    "season_title": {"en": "🏆 **Season {season}** — ends {when}", "fr": "🏆 **Saison {season}** — fin {when}"},
    "season_empty": {"en": "Nobody is ranked yet this season ({games} season games needed). Be the first!",
                     "fr": "Personne n'est encore classé cette saison ({games} parties de saison minimum). "
                           "Soyez le premier !"},
    "ranking_line": {"en": "{place} **{pseudo}** — {elo} Elo · {games} games · {rate}% wins",
                     "fr": "{place} **{pseudo}** — {elo} Elo · {games} parties · {rate} % de victoires"},
    "ranking_footer": {"en": "Full leaderboard: <{url}>", "fr": "Classement complet : <{url}>"},
    "ranking_updated": {"en": " · updated {when}", "fr": " · mis à jour {when}"},
    "no_player": {"en": "No Gemspire player found.", "fr": "Aucun joueur Gemspire trouvé."},
    "profile_rank": {"en": " · #{rank} on the leaderboard", "fr": " · {rank}ᵉ au classement"},
    "profile_stats": {"en": "{games} games · {wins} wins ({rate}%)", "fr": "{games} parties · {wins} victoires ({rate} %)"},
    "profile_achievements": {"en": "🏅 {count} achievement(s)", "fr": "🏅 {count} succès"},
    "profile_badges": {"en": "🏆 Seasons: {badges}", "fr": "🏆 Saisons : {badges}"},
    "profile_season": {"en": "This season: {elo} Elo · {games} games{rank}",
                       "fr": "Cette saison : {elo} Elo · {games} parties{rank}"},
    "profile_season_rank": {"en": " · #{rank}", "fr": " · {rank}ᵉ"},
    "banned_forever": {"en": "⛔ Account suspended permanently", "fr": "⛔ Compte suspendu définitivement"},
    "banned_until": {"en": "⛔ Account suspended until {date}", "fr": "⛔ Compte suspendu jusqu'au {date}"},
    "not_linked_member": {
        "en": "This member hasn't linked Gemspire to Discord (in the game: Play online > Sign in with Discord).",
        "fr": "Ce membre n'a pas lié Gemspire à Discord (dans le jeu : Jouer en ligne > Se connecter avec Discord)."},
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
    "server_down": {"en": "❌ The Gemspire server is not responding. Try again in a moment.",
                    "fr": "❌ Le serveur Gemspire ne répond pas. Réessayez dans un moment."},
    "link_missing": {
        "en": "No Gemspire account linked yet: in the game, **Play online > Sign in with Discord**, choose a "
              "username, then run `/link` again.",
        "fr": "Pas encore de compte Gemspire lié : dans le jeu, **Jouer en ligne > Se connecter avec Discord**, "
              "choisissez un pseudo, puis relancez `/lier`."},
    "link_ok": {"en": "✅ Gemspire account **{pseudo}** linked.", "fr": "✅ Compte Gemspire **{pseudo}** lié."},
    "role_added": {"en": " Role **{role}** added.", "fr": " Rôle **{role}** ajouté."},
    "role_denied": {"en": " (the bot can't give the {role} role: move its role above it)",
                    "fr": " (le bot ne peut pas donner le rôle {role} : placez son rôle au-dessus)"},
    "play": {
        "en": "🎲 **Gemspire** — Windows and Linux, a single file to run:\n• Download: <{download}>\n"
              "• Play in your browser, website and leaderboard: <{site}>\n"
              "• Join the **{channel}** voice channel: the bot sets up your game.",
        "fr": "🎲 **Gemspire** — Windows et Linux, un seul fichier à lancer :\n• Téléchargement : <{download}>\n"
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
    "challenge": {
        "en": "⚔️ **{pseudo}** ({challenger}) challenges {target} to a {mode} game! Lobby code **{code}**: "
              "in Gemspire, Play online > Join (or <{site}/play/>).",
        "fr": "⚔️ **{pseudo}** ({challenger}) défie {target} dans une partie {mode} ! Code du salon **{code}** : "
              "dans Gemspire, Jouer en ligne > Rejoindre (ou <{site}/play/>)."},
    "challenge_not_linked": {
        "en": " ({target} hasn't linked Gemspire yet: `/link` after signing in with Discord.)",
        "fr": " ({target} n'a pas encore lié Gemspire : `/lier` après connexion avec Discord.)"},
    "challenge_self": {"en": "Pick someone else to challenge.", "fr": "Choisissez quelqu'un d'autre à défier."},
    "mode_classic": {"en": "classic", "fr": "classique"},
    "mode_quick": {"en": "quick", "fr": "rapide"},
    "season_confirm": {
        "en": "This ends the season now for everyone (season Elo back to 1000; seasons also change by themselves "
              "every two months). Run it again with `confirm: True`.",
        "fr": "Cela termine la saison tout de suite pour tout le monde (Elo de saison remis à 1000 ; les saisons "
              "changent aussi toutes seules tous les deux mois). Relancez avec `confirmer: True`."},
    "season_end": {
        "en": "🏁 **Season {ended} is over!**\n{podium}\nSeason {season} starts now: everyone's season Elo is "
              "back to 1000 (the global Elo doesn't change). It ends {when}. Good luck!",
        "fr": "🏁 **La saison {ended} est terminée !**\n{podium}\nLa saison {season} commence : l'Elo de saison de "
              "chacun repart à 1000 (l'Elo global ne change pas). Fin {when}. Bonne chance !"},
    "season_ended_ok": {"en": "Season {ended} ended: the announcement is posted in the leaderboard channel.",
                        "fr": "Saison {ended} terminée : l'annonce est publiée dans le salon du classement."},
    "season_log": {"en": "🏁 {user} ended season {ended}", "fr": "🏁 {user} a terminé la saison {ended}"},
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
    "Season and global top 10 of Gemspire": "Top 10 de la saison et global de Gemspire",
    "profile": "profil",
    "Elo, rank and wins of a player": "Elo, rang et victoires d'un joueur",
    "member": "membre",
    "Discord member (default: you)": "Membre Discord (par défaut : vous)",
    "username": "pseudo",
    "Or Gemspire username": "Ou pseudo Gemspire",
    "Gemspire username": "Pseudo Gemspire",
    "link": "lier",
    "Link your Gemspire account to Discord and get the Player role":
        "Relie votre compte Gemspire à Discord et donne le rôle Joueur",
    "play": "jouer",
    "Download Gemspire or play in your browser": "Télécharger Gemspire ou jouer dans le navigateur",
    "rules": "regles",
    "challenge": "defi",
    "Challenge a member to a Gemspire game (2 players)": "Défier un membre dans une partie de Gemspire (2 joueurs)",
    "Member to challenge": "Membre à défier",
    "mode": "mode",
    "Game mode": "Mode de jeu",
    "Classic": "Classique",
    "Quick": "Rapide",
    "Gemspire rules in short": "Les règles de Gemspire en bref",
    "report": "signaler",
    "Report a player to the moderators (cheating, username...)":
        "Signaler un joueur aux modérateurs (triche, pseudo...)",
    "reason": "raison",
    "What happened": "Ce qui s'est passé",
    "mod": "mod",
    "Gemspire moderation": "Modération de Gemspire",
    "ban": "ban",
    "Suspend a Gemspire account (0 days = permanent)": "Suspendre un compte Gemspire (0 jour = définitif)",
    "days": "jours",
    "Duration in days (0 = permanent)": "Durée en jours (0 = définitif)",
    "Reason": "Motif",
    "unban": "unban",
    "Reactivate a Gemspire account": "Réactiver un compte Gemspire",
    "history": "historique",
    "Last 15 games of a player (spot cheating)": "15 dernières parties d'un joueur (repérer la triche)",
    "cancel-game": "annuler-partie",
    "Cancel the Elo of a rigged game (number from /mod history)":
        "Annuler l'Elo d'une partie truquée (numéro de /mod historique)",
    "number": "numero",
    "Game number": "Numéro de la partie",
    "delete-account": "supprimer-compte",
    "new-season": "nouvelle-saison",
    "End the ranked season (archives the top 50, badges for the top 10)":
        "Terminer la saison classée (archive le top 50, badges pour le top 10)",
    "confirm": "confirmer",
    "Really end the season for everyone": "Vraiment terminer la saison pour tout le monde",
    "Delete a Gemspire account (GDPR request)": "Supprimer un compte Gemspire (demande RGPD)",
    "dev": "dev",
    "Gemspire administration (owner only)": "Administration de Gemspire (propriétaire seulement)",
    "status": "etat",
    "Server state: players, games, switches": "État du serveur : joueurs, parties, interrupteurs",
    "signups": "inscriptions",
    "Open or close the creation of new accounts": "Ouvrir ou fermer la création de comptes",
    "state": "etat",
    "Open": "Ouvertes",
    "Closed": "Fermées",
    "games": "parties",
    "Allow or pause new games (maintenance)": "Autoriser ou suspendre les nouvelles parties (maintenance)",
    "Allowed": "Autorisées",
    "Paused": "Suspendues",
    "announce": "annonce",
    "Message shown in the game to every player (empty = remove)":
        "Message affiché dans le jeu à tous les joueurs (vide = retirer)",
    "text": "texte",
    "rename": "renommer",
    "Change the username of a player": "Changer le pseudo d'un joueur",
    "new_username": "nouveau_pseudo",
    "elo": "elo",
    "Set the Elo of a player": "Fixer l'Elo d'un joueur",
    "value": "valeur",
    "badge": "badge",
    "Give a season badge (top 10) to a player": "Donner un badge de saison (top 10) à un joueur",
    "season": "saison",
    "rank": "rang",
    "remove-badge": "retirer-badge",
    "Remove a season badge from a player": "Retirer un badge de saison à un joueur",
    "logout": "deconnecter",
    "Sign a player out everywhere": "Déconnecter un joueur partout",
    "end-game": "terminer-partie",
    "End a stuck game in progress (no Elo)": "Terminer une partie en cours bloquée (sans Elo)",
    "Game number (panel or /dev status)": "Numéro de la partie (panel ou /dev etat)",
    "min-version": "version-min",
    "Minimum game version (empty = server setting)": "Version minimale du jeu (vide = réglage du serveur)",
    "version": "version",
}


def lang_of(locale):
    """« fr », « fr-FR » -> "fr" ; tout le reste -> "en"."""
    return "fr" if str(locale or "").lower().startswith("fr") else "en"


def tr(key, lang="en", **values):
    """Texte `key` dans la langue `lang` (anglais si inconnue), avec ses valeurs."""
    entry = TEXTS[key]
    return entry.get(lang, entry["en"]).format(**values)
