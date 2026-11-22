"""Bot Discord de Splendor : « Crée ta partie », #classement et commandes.

Réglages (variables d'environnement) :
  DISCORD_BOT_TOKEN    jeton du bot (portail développeur Discord)
  SPLENDOR_API         adresse du serveur Splendor (défaut http://127.0.0.1:8080)
  SPLENDOR_BOT_KEY     clé partagée avec le serveur (même valeur des deux côtés)
  CREATE_CHANNEL_NAME  nom du salon vocal « déclencheur » (défaut « Crée ta partie »)
  SPLENDOR_BOT_STATE   fichier d'état du bot (défaut temp_channels.json)
  RANKING_CHANNEL      salon du classement mis à jour (défaut « classement »)
  MOD_LOG_CHANNEL      salon du journal de modération (défaut « moderation »)
  MOD_ROLE             rôle des modérateurs (défaut « Modérateur »)
  PLAYER_ROLE          rôle donné par /lier (défaut « Joueur »)

Permissions du bot sur le serveur Discord : voir les salons, envoyer des
messages, intégrer des liens, lire l'historique, gérer les salons, gérer les
rôles, déplacer des membres. Aucun intent privilégié.
"""

import asyncio
import logging
import os
import time

import discord
from discord import app_commands

from api import ApiError, SplendorApi
from logic import (CREATE_CHANNEL_NAME, DELETE_DELAY_SECONDS, DOWNLOAD_URL, OFFER_RETRY_COUNT, OFFER_RETRY_SECONDS,
                   RANKING_SECONDS, SITE_URL, TempChannel, apply_event, ban_duration_text, can_moderate,
                   channel_matches, format_history, format_profile, format_ranking, is_create_channel, load_state,
                   offer_message, save_state, should_delete, temp_channel_name)

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
log = logging.getLogger("splendor-bot")

api = SplendorApi(os.environ.get("SPLENDOR_API", "http://127.0.0.1:8080"), os.environ["SPLENDOR_BOT_KEY"])
create_name = os.environ.get("CREATE_CHANNEL_NAME", CREATE_CHANNEL_NAME)
state_file = os.environ.get("SPLENDOR_BOT_STATE", "temp_channels.json")
ranking_channel_name = os.environ.get("RANKING_CHANNEL", "classement")
mod_log_name = os.environ.get("MOD_LOG_CHANNEL", "moderation")
mod_role_name = os.environ.get("MOD_ROLE", "Modérateur")
player_role_name = os.environ.get("PLAYER_ROLE", "Joueur")

intents = discord.Intents.none()
intents.guilds = True
intents.voice_states = True
client = discord.Client(intents=intents)
tree = app_commands.CommandTree(client)

temp_channels = {}      # identifiant du salon vocal temporaire -> TempChannel
ranking_messages = {}   # identifiant du serveur Discord -> message du classement


def remember():
    """Enregistre l'état (pour s'en souvenir après un redémarrage)."""
    try:
        save_state(state_file, temp_channels.keys(), ranking_messages)
    except OSError as error:
        log.warning("Impossible d'enregistrer l'état du bot : %s", error)


def find_text_channel(guild, wanted):
    for channel in guild.text_channels:
        if channel_matches(channel.name, wanted):
            return channel
    return None


async def mod_log(guild, text):
    """Écrit dans le salon de modération (s'il existe)."""
    channel = find_text_channel(guild, mod_log_name) if guild else None
    if channel is not None:
        try:
            await channel.send(text, allowed_mentions=discord.AllowedMentions.none())
        except discord.HTTPException as error:
            log.warning("Journal de modération : %s", error)


# ---------------------------------------------------------------- Salons vocaux « Crée ta partie »

async def offer_game(member, channel):
    """Propose la partie dans le jeu ; réessaie tant que le jeu n'est pas ouvert."""
    for attempt in range(OFFER_RETRY_COUNT):
        if channel.id not in temp_channels:
            return  # salon supprimé entre-temps
        presence = await asyncio.to_thread(api.presence, member.id)
        if attempt == 0:
            await channel.send(offer_message(presence))
        if not presence.get("linked"):
            return
        if presence.get("online"):
            await asyncio.to_thread(api.offer, member.id, channel.id, channel.name)
            return
        await asyncio.sleep(OFFER_RETRY_SECONDS)


async def delete_if_still_empty(channel_id):
    """Supprime le salon temporaire s'il est encore vide après DELETE_DELAY_SECONDS."""
    await asyncio.sleep(DELETE_DELAY_SECONDS)
    channel = client.get_channel(channel_id)
    if channel is None:  # déjà supprimé à la main
        if temp_channels.pop(channel_id, None) is not None:
            remember()
        return
    if not should_delete(channel_id in temp_channels, len(channel.members)):
        return  # quelqu'un est revenu
    temp_channels.pop(channel_id, None)
    remember()
    try:
        await channel.delete(reason="Splendor : salon temporaire vide")
        log.info("Salon temporaire %s supprimé", channel_id)
    except discord.NotFound:
        pass


@client.event
async def on_voice_state_update(member, before, after):
    # Entrée dans « Crée ta partie » : salon temporaire, puis proposition dans le jeu
    if after.channel is not None and before.channel != after.channel and is_create_channel(after.channel.name, create_name):
        guild = after.channel.guild
        channel = await guild.create_voice_channel(temp_channel_name(member.display_name),
                                                   category=after.channel.category,
                                                   reason="Splendor : Crée ta partie")
        temp_channels[channel.id] = TempChannel(member.id)
        remember()
        await member.move_to(channel)
        log.info("Salon temporaire %s créé pour %s", channel.id, member.id)
        asyncio.create_task(offer_game(member, channel))

    # Sortie d'un salon temporaire : suppression s'il reste vide 30 secondes
    if before.channel is not None and before.channel != after.channel:
        if should_delete(before.channel.id in temp_channels, len(before.channel.members)):
            asyncio.create_task(delete_if_still_empty(before.channel.id))


async def follow_events():
    """Suit les salons Splendor liés aux salons vocaux (places, lancement)."""
    since = 0
    while True:
        try:
            body = await asyncio.to_thread(api.events, since)
        except Exception as error:  # serveur redémarré, réseau...
            log.warning("Événements indisponibles : %s", error)
            await asyncio.sleep(5)
            continue
        since = body.get("last", since)
        for event in body.get("events", []):
            channel_id = int(event.get("voice_channel", "0"))
            temp = temp_channels.get(channel_id)
            channel = client.get_channel(channel_id)
            if temp is None or channel is None:
                continue  # le bot ne touche qu'aux salons qu'il a créés
            limit, message = apply_event(temp, event)
            if limit is not None and channel.user_limit != limit:
                await channel.edit(user_limit=limit, reason="Splendor : places de la partie")
            if message:
                await channel.send(message)


# ---------------------------------------------------------------- Salon #classement

async def update_rankings():
    """Toutes les 10 minutes : le même message du salon #classement est mis à jour."""
    while True:
        try:
            players = await asyncio.to_thread(api.leaderboard)
            text = format_ranking(players, time.time())
            for guild in client.guilds:
                channel = find_text_channel(guild, ranking_channel_name)
                if channel is None:
                    continue
                message = None
                message_id = ranking_messages.get(guild.id)
                if message_id:
                    try:
                        message = await channel.fetch_message(message_id)
                    except (discord.NotFound, discord.Forbidden):
                        message = None
                if message is not None and message.author == client.user:
                    await message.edit(content=text, allowed_mentions=discord.AllowedMentions.none())
                else:
                    message = await channel.send(text, allowed_mentions=discord.AllowedMentions.none())
                    ranking_messages[guild.id] = message.id
                    remember()
        except Exception as error:  # serveur indisponible, permissions...
            log.warning("Classement non mis à jour : %s", error)
        await asyncio.sleep(RANKING_SECONDS)


# ---------------------------------------------------------------- Commandes pour tous

async def answer(interaction, text, private=False):
    await interaction.response.send_message(text, ephemeral=private, allowed_mentions=discord.AllowedMentions.none())


async def call_api(interaction, function, *args, **kwargs):
    """Appelle le serveur ; en cas d'erreur, répond au joueur et renvoie None."""
    try:
        return await asyncio.to_thread(function, *args, **kwargs)
    except ApiError as error:
        await answer(interaction, "❌ " + str(error), private=True)
    except Exception as error:  # serveur injoignable
        log.warning("Serveur Splendor : %s", error)
        await answer(interaction, "❌ Le serveur Splendor ne répond pas. Réessayez dans un moment.", private=True)
    return None


@tree.command(name="classement", description="Top 10 des joueurs Splendor en ligne")
async def classement(interaction: discord.Interaction):
    players = await call_api(interaction, api.leaderboard)
    if players is not None:
        await answer(interaction, format_ranking(players))


@tree.command(name="profil", description="Elo, rang et victoires d'un joueur")
@app_commands.describe(membre="Membre Discord (par défaut : vous)", pseudo="Ou pseudo Splendor")
async def profil(interaction: discord.Interaction, membre: discord.Member = None, pseudo: str = None):
    if pseudo:
        result = await call_api(interaction, api.profile, pseudo=pseudo)
    else:
        result = await call_api(interaction, api.profile, discord_id=(membre or interaction.user).id)
    if result is None:
        return
    if not result.get("found") and not pseudo:
        await answer(interaction, "Ce membre n'a pas lié Splendor à Discord (dans le jeu : Jouer en ligne > "
                                  "Se connecter avec Discord).", private=True)
        return
    await answer(interaction, format_profile(result))


@tree.command(name="lier", description="Relie votre compte Splendor à Discord et donne le rôle Joueur")
async def lier(interaction: discord.Interaction):
    presence = await call_api(interaction, api.presence, interaction.user.id)
    if presence is None:
        return
    if not presence.get("linked") or not presence.get("pseudo"):
        await answer(interaction, "Pas encore de compte Splendor lié : dans le jeu, **Jouer en ligne > Se connecter "
                                  "avec Discord**, choisissez un pseudo, puis relancez `/lier`.", private=True)
        return
    text = "✅ Compte Splendor **{}** lié.".format(presence["pseudo"])
    guild = interaction.guild
    role = discord.utils.find(lambda r: channel_matches(r.name, player_role_name), guild.roles) if guild else None
    if role is not None and isinstance(interaction.user, discord.Member):
        try:
            await interaction.user.add_roles(role, reason="Splendor : compte lié")
            text += " Rôle **{}** ajouté.".format(role.name)
        except discord.Forbidden:
            text += " (le bot ne peut pas donner le rôle {} : placez son rôle au-dessus)".format(role.name)
    await answer(interaction, text, private=True)


@tree.command(name="jouer", description="Télécharger Splendor et jouer en ligne")
async def jouer(interaction: discord.Interaction):
    await answer(interaction,
                 "🎲 **Splendor** — Windows et Linux, un seul fichier à lancer :\n"
                 "• Téléchargement : <{}>\n• Site et classement : <{}>\n"
                 "• Rejoignez le salon vocal **{}** : le bot prépare votre partie.".format(
                     DOWNLOAD_URL, SITE_URL, create_name), private=True)


@tree.command(name="regles", description="Les règles de Splendor en bref")
async def regles(interaction: discord.Interaction):
    await answer(interaction,
                 "📜 **Règles en bref** — à votre tour, UNE action :\n"
                 "• prendre 3 jetons de couleurs différentes, ou 2 identiques (pile d'au moins 4) ;\n"
                 "• acheter une carte (vos cartes réduisent le prix) ;\n"
                 "• en classique : réserver une carte et gagner 1 or (joker).\n"
                 "10 jetons au maximum. Les nobles viennent quand vous avez assez de cartes. "
                 "À 15 points, on finit le tour de table. En ligne : 2 minutes par tour.\n"
                 "Règles complètes : <https://github.com/Pierre-Pln-Bys/ProjectSplendor/wiki/Regles-du-jeu>",
                 private=True)


@tree.command(name="signaler", description="Signaler un joueur aux modérateurs (triche, pseudo...)")
@app_commands.describe(pseudo="Pseudo Splendor du joueur", raison="Ce qui s'est passé")
async def signaler(interaction: discord.Interaction, pseudo: str, raison: app_commands.Range[str, 5, 500]):
    await mod_log(interaction.guild, "🚩 **Signalement** de {} contre **{}** : {}".format(
        interaction.user.mention, discord.utils.escape_markdown(pseudo), discord.utils.escape_markdown(raison)))
    await answer(interaction, "Merci, les modérateurs ont reçu votre signalement.", private=True)


# ---------------------------------------------------------------- Commandes des modérateurs

def is_moderator(interaction):
    user = interaction.user
    if not isinstance(user, discord.Member):
        return False
    return can_moderate([role.name for role in user.roles], user.guild_permissions.administrator, mod_role_name)


async def refuse_if_not_moderator(interaction):
    if is_moderator(interaction):
        return False
    await answer(interaction, "Réservé aux modérateurs.", private=True)
    return True


moderation = app_commands.Group(name="mod", description="Modération de Splendor",
                                default_permissions=discord.Permissions(moderate_members=True), guild_only=True)


@moderation.command(name="ban", description="Suspendre un compte Splendor (0 jour = définitif)")
@app_commands.describe(pseudo="Pseudo Splendor", jours="Durée en jours (0 = définitif)", raison="Motif")
async def mod_ban(interaction: discord.Interaction, pseudo: str, jours: app_commands.Range[int, 0, 3650],
                  raison: app_commands.Range[str, 3, 200]):
    if await refuse_if_not_moderator(interaction):
        return
    result = await call_api(interaction, api.ban, pseudo, jours, raison)
    if result is None:
        return
    await answer(interaction, "⛔ **{}** suspendu {} : {}".format(result.get("pseudo", pseudo),
                                                                 ban_duration_text(jours), raison), private=True)
    await mod_log(interaction.guild, "⛔ {} a suspendu **{}** {} — {}".format(
        interaction.user.mention, result.get("pseudo", pseudo), ban_duration_text(jours), raison))


@moderation.command(name="unban", description="Réactiver un compte Splendor")
async def mod_unban(interaction: discord.Interaction, pseudo: str):
    if await refuse_if_not_moderator(interaction):
        return
    result = await call_api(interaction, api.unban, pseudo)
    if result is None:
        return
    await answer(interaction, "✅ **{}** réactivé.".format(result.get("pseudo", pseudo)), private=True)
    await mod_log(interaction.guild, "✅ {} a réactivé **{}**".format(interaction.user.mention, result.get("pseudo", pseudo)))


@moderation.command(name="historique", description="15 dernières parties d'un joueur (repérer la triche)")
async def mod_historique(interaction: discord.Interaction, pseudo: str):
    if await refuse_if_not_moderator(interaction):
        return
    result = await call_api(interaction, api.history, pseudo=pseudo)
    if result is not None:
        await answer(interaction, format_history(result), private=True)


@moderation.command(name="annuler-partie", description="Annuler l'Elo d'une partie truquée (numéro de /mod historique)")
async def mod_annuler(interaction: discord.Interaction, numero: app_commands.Range[int, 1]):
    if await refuse_if_not_moderator(interaction):
        return
    if await call_api(interaction, api.cancel_game, numero) is None:
        return
    await answer(interaction, "↩️ Partie #{} annulée : Elo, parties et victoires rendus.".format(numero), private=True)
    await mod_log(interaction.guild, "↩️ {} a annulé la partie #{}".format(interaction.user.mention, numero))


@moderation.command(name="supprimer-compte", description="Supprimer un compte Splendor (demande RGPD)")
async def mod_supprimer(interaction: discord.Interaction, pseudo: str):
    if await refuse_if_not_moderator(interaction):
        return
    if await call_api(interaction, api.delete_account, pseudo=pseudo) is None:
        return
    await answer(interaction, "🗑️ Compte **{}** supprimé.".format(pseudo), private=True)
    await mod_log(interaction.guild, "🗑️ {} a supprimé le compte **{}**".format(interaction.user.mention, pseudo))


tree.add_command(moderation)


# ---------------------------------------------------------------- Démarrage

@client.event
async def on_ready():
    log.info("Connecté en tant que %s", client.user)
    # Salons créés avant un redémarrage : on les reprend, et on supprime ceux restés vides
    state = load_state(state_file)
    ranking_messages.update(state["ranking"])
    for channel_id in state["channels"]:
        if channel_id in temp_channels:
            continue
        temp_channels[channel_id] = TempChannel(0)
        asyncio.create_task(delete_if_still_empty(channel_id))
    if not getattr(client, "started", False):
        client.started = True
        asyncio.create_task(follow_events())
        asyncio.create_task(update_rankings())
        # Commandes publiées sur chaque serveur (disponibles tout de suite)
        for guild in client.guilds:
            try:
                tree.copy_global_to(guild=guild)
                await tree.sync(guild=guild)
            except discord.HTTPException as error:
                log.warning("Commandes non publiées sur %s : %s", guild.id, error)
        log.info("Commandes publiées sur %d serveur(s)", len(client.guilds))


@client.event
async def on_guild_join(guild):
    tree.copy_global_to(guild=guild)
    await tree.sync(guild=guild)


if __name__ == "__main__":
    client.run(os.environ["DISCORD_BOT_TOKEN"], log_handler=None)
