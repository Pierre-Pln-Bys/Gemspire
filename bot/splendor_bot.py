"""Bot Discord de Splendor : « Create a game », #leaderboard et commandes.

Commandes et textes en anglais, traduits en français pour les membres dont
Discord est en français (texts.py).

Réglages (variables d'environnement) :
  DISCORD_BOT_TOKEN    jeton du bot (portail développeur Discord)
  SPLENDOR_API         adresse du serveur Splendor (défaut http://127.0.0.1:8080)
  SPLENDOR_BOT_KEY     clé partagée avec le serveur (même valeur des deux côtés)
  CREATE_CHANNEL_NAME  salon vocal « déclencheur » (défaut « Create a game » ; « Crée ta partie » marche aussi)
  SPLENDOR_BOT_STATE   fichier d'état du bot (défaut temp_channels.json)
  RANKING_CHANNEL      salon du classement mis à jour (défaut « leaderboard » ; « classement » marche aussi)
  MOD_LOG_CHANNEL      salon du journal de modération (défaut « moderation »)
  MOD_ROLE             rôle des modérateurs (défaut « Modérateur »)
  PLAYER_ROLE          rôle donné par /link (défaut « Joueur »)
  CREATE_CHANNEL_ID, RANKING_CHANNEL_ID, MOD_LOG_CHANNEL_ID, MOD_ROLE_ID,
  PLAYER_ROLE_ID       identifiants Discord (clic droit > Copier l'identifiant) ;
                       s'ils sont réglés, ils remplacent les noms ci-dessus

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
                   RANKING_SECONDS, TempChannel, apply_event, ban_duration_text, can_moderate, channel_matches,
                   format_history, format_profile, format_ranking, is_create_channel, load_state, offer_message,
                   parse_id, save_state, should_delete, site_page, temp_channel_name)
from texts import COMMAND_FR, lang_of, tr

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
log = logging.getLogger("splendor-bot")

api = SplendorApi(os.environ.get("SPLENDOR_API", "http://127.0.0.1:8080"), os.environ["SPLENDOR_BOT_KEY"])
create_name = os.environ.get("CREATE_CHANNEL_NAME", CREATE_CHANNEL_NAME)
state_file = os.environ.get("SPLENDOR_BOT_STATE", "temp_channels.json")
ranking_channel_names = [os.environ.get("RANKING_CHANNEL", "leaderboard"), "classement"]
mod_log_name = os.environ.get("MOD_LOG_CHANNEL", "moderation")
mod_role_name = os.environ.get("MOD_ROLE", "Modérateur")
player_role_name = os.environ.get("PLAYER_ROLE", "Joueur")
# Identifiants (plus sûrs que les noms : un salon renommé reste reconnu)
create_channel_id = parse_id(os.environ.get("CREATE_CHANNEL_ID"))
ranking_channel_id = parse_id(os.environ.get("RANKING_CHANNEL_ID"))
mod_log_channel_id = parse_id(os.environ.get("MOD_LOG_CHANNEL_ID"))
mod_role_id = parse_id(os.environ.get("MOD_ROLE_ID"))
player_role_id = parse_id(os.environ.get("PLAYER_ROLE_ID"))

intents = discord.Intents.none()
intents.guilds = True
intents.voice_states = True
client = discord.Client(intents=intents)
tree = app_commands.CommandTree(client)
_ = app_commands.locale_str  # textes de commande traduisibles


class FrenchTranslator(app_commands.Translator):
    """Noms et descriptions des commandes en français pour les Discord en français."""

    async def translate(self, string, locale, context):
        if locale == discord.Locale.french:
            return COMMAND_FR.get(string.message)
        return None

temp_channels = {}      # identifiant du salon vocal temporaire -> TempChannel
ranking_messages = {}   # identifiant du serveur Discord -> message du classement


def remember():
    """Enregistre l'état (pour s'en souvenir après un redémarrage)."""
    try:
        save_state(state_file, temp_channels.keys(), ranking_messages)
    except OSError as error:
        log.warning("Impossible d'enregistrer l'état du bot : %s", error)


def find_text_channel(guild, wanted, channel_id=None):
    """Le salon texte `channel_id` s'il est réglé, sinon le premier dont le nom
    contient l'un des noms voulus."""
    if channel_id is not None:
        channel = guild.get_channel(channel_id)
        return channel if isinstance(channel, discord.TextChannel) else None
    names = wanted if isinstance(wanted, list) else [wanted]
    for channel in guild.text_channels:
        if any(channel_matches(channel.name, name) for name in names):
            return channel
    return None


def guild_lang(guild):
    """Langue du serveur Discord (messages postés dans les salons)."""
    return lang_of(getattr(guild, "preferred_locale", None))


def user_lang(interaction):
    """Langue du membre qui utilise une commande."""
    return lang_of(interaction.locale)


async def mod_log(guild, text):
    """Écrit dans le salon de modération (s'il existe)."""
    channel = find_text_channel(guild, mod_log_name, mod_log_channel_id) if guild else None
    if channel is not None:
        try:
            await channel.send(text, allowed_mentions=discord.AllowedMentions.none())
        except discord.HTTPException as error:
            log.warning("Journal de modération : %s", error)


# ---------------------------------------------------------------- Salons vocaux « Crée ta partie »

def is_create_voice(channel):
    """Le salon vocal « déclencheur » : par identifiant s'il est réglé, sinon par nom."""
    if create_channel_id is not None:
        return channel.id == create_channel_id
    return is_create_channel(channel.name, create_name)

async def offer_game(member, channel):
    """Propose la partie dans le jeu ; réessaie tant que le jeu n'est pas ouvert."""
    for attempt in range(OFFER_RETRY_COUNT):
        if channel.id not in temp_channels:
            return  # salon supprimé entre-temps
        presence = await asyncio.to_thread(api.presence, member.id)
        if attempt == 0:
            await channel.send(offer_message(presence, guild_lang(channel.guild)))
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
    if after.channel is not None and before.channel != after.channel and is_create_voice(after.channel):
        guild = after.channel.guild
        lang = guild_lang(guild)
        channel = await guild.create_voice_channel(temp_channel_name(member.display_name, lang),
                                                   category=after.channel.category,
                                                   reason="Splendor: create a game")
        temp_channels[channel.id] = TempChannel(member.id, lang)
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
            for guild in client.guilds:
                text = format_ranking(players, time.time(), guild_lang(guild))
                channel = find_text_channel(guild, ranking_channel_names, ranking_channel_id)
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
        await answer(interaction, tr("server_down", user_lang(interaction)), private=True)
    return None


@tree.command(name=_("leaderboard"), description=_("Top 10 Splendor online players"))
async def leaderboard(interaction: discord.Interaction):
    players = await call_api(interaction, api.leaderboard)
    if players is not None:
        await answer(interaction, format_ranking(players, lang=user_lang(interaction)))


@tree.command(name=_("profile"), description=_("Elo, rank and wins of a player"))
@app_commands.rename(member=_("member"), username=_("username"))
@app_commands.describe(member=_("Discord member (default: you)"), username=_("Or Splendor username"))
async def profile(interaction: discord.Interaction, member: discord.Member = None, username: str = None):
    lang = user_lang(interaction)
    if username:
        result = await call_api(interaction, api.profile, pseudo=username)
    else:
        result = await call_api(interaction, api.profile, discord_id=(member or interaction.user).id)
    if result is None:
        return
    if not result.get("found") and not username:
        await answer(interaction, tr("not_linked_member", lang), private=True)
        return
    await answer(interaction, format_profile(result, lang))


@tree.command(name=_("link"), description=_("Link your Splendor account to Discord and get the Player role"))
async def link(interaction: discord.Interaction):
    lang = user_lang(interaction)
    presence = await call_api(interaction, api.presence, interaction.user.id)
    if presence is None:
        return
    if not presence.get("linked") or not presence.get("pseudo"):
        await answer(interaction, tr("link_missing", lang), private=True)
        return
    text = tr("link_ok", lang, pseudo=presence["pseudo"])
    guild = interaction.guild
    role = None
    if guild and player_role_id is not None:
        role = guild.get_role(player_role_id)
    elif guild:
        role = discord.utils.find(lambda r: channel_matches(r.name, player_role_name) or channel_matches(r.name, "Joueur"),
                                  guild.roles)
    if role is not None and isinstance(interaction.user, discord.Member):
        try:
            await interaction.user.add_roles(role, reason="Splendor: account linked")
            text += tr("role_added", lang, role=role.name)
        except discord.Forbidden:
            text += tr("role_denied", lang, role=role.name)
    await answer(interaction, text, private=True)


@tree.command(name=_("play"), description=_("Download Splendor or play in your browser"))
async def play(interaction: discord.Interaction):
    lang = user_lang(interaction)
    channel = "<#{}>".format(create_channel_id) if create_channel_id is not None else create_name
    await answer(interaction, tr("play", lang, download=DOWNLOAD_URL, site=site_page("home", lang), channel=channel),
                 private=True)


@tree.command(name=_("rules"), description=_("Splendor rules in short"))
async def rules(interaction: discord.Interaction):
    await answer(interaction, tr("rules", user_lang(interaction),
                                 wiki="https://github.com/Pierre-Pln-Bys/ProjectSplendor/wiki/Regles-du-jeu"),
                 private=True)


@tree.command(name=_("report"), description=_("Report a player to the moderators (cheating, username...)"))
@app_commands.rename(username=_("username"), reason=_("reason"))
@app_commands.describe(username=_("Splendor username"), reason=_("What happened"))
async def report(interaction: discord.Interaction, username: str, reason: app_commands.Range[str, 5, 500]):
    await mod_log(interaction.guild, tr("report_log", guild_lang(interaction.guild), user=interaction.user.mention,
                                        pseudo=discord.utils.escape_markdown(username),
                                        reason=discord.utils.escape_markdown(reason)))
    await answer(interaction, tr("report_ok", user_lang(interaction)), private=True)


# ---------------------------------------------------------------- Commandes des modérateurs

def is_moderator(interaction):
    user = interaction.user
    if not isinstance(user, discord.Member):
        return False
    return can_moderate([role.name for role in user.roles], user.guild_permissions.administrator, mod_role_name,
                        [role.id for role in user.roles], mod_role_id)


async def refuse_if_not_moderator(interaction):
    if is_moderator(interaction):
        return False
    await answer(interaction, tr("mods_only", user_lang(interaction)), private=True)
    return True


moderation = app_commands.Group(name=_("mod"), description=_("Splendor moderation"),
                                default_permissions=discord.Permissions(moderate_members=True), guild_only=True)


@moderation.command(name=_("ban"), description=_("Suspend a Splendor account (0 days = permanent)"))
@app_commands.rename(username=_("username"), days=_("days"), reason=_("reason"))
@app_commands.describe(username=_("Splendor username"), days=_("Duration in days (0 = permanent)"),
                       reason=_("Reason"))
async def mod_ban(interaction: discord.Interaction, username: str, days: app_commands.Range[int, 0, 3650],
                  reason: app_commands.Range[str, 3, 200]):
    if await refuse_if_not_moderator(interaction):
        return
    result = await call_api(interaction, api.ban, username, days, reason)
    if result is None:
        return
    pseudo = result.get("pseudo", username)
    lang, log_lang = user_lang(interaction), guild_lang(interaction.guild)
    await answer(interaction, tr("ban_ok", lang, pseudo=pseudo, duration=ban_duration_text(days, lang), reason=reason),
                 private=True)
    await mod_log(interaction.guild, tr("ban_log", log_lang, user=interaction.user.mention, pseudo=pseudo,
                                        duration=ban_duration_text(days, log_lang), reason=reason))


@moderation.command(name=_("unban"), description=_("Reactivate a Splendor account"))
@app_commands.rename(username=_("username"))
@app_commands.describe(username=_("Splendor username"))
async def mod_unban(interaction: discord.Interaction, username: str):
    if await refuse_if_not_moderator(interaction):
        return
    result = await call_api(interaction, api.unban, username)
    if result is None:
        return
    pseudo = result.get("pseudo", username)
    await answer(interaction, tr("unban_ok", user_lang(interaction), pseudo=pseudo), private=True)
    await mod_log(interaction.guild, tr("unban_log", guild_lang(interaction.guild), user=interaction.user.mention,
                                        pseudo=pseudo))


@moderation.command(name=_("history"), description=_("Last 15 games of a player (spot cheating)"))
@app_commands.rename(username=_("username"))
@app_commands.describe(username=_("Splendor username"))
async def mod_history(interaction: discord.Interaction, username: str):
    if await refuse_if_not_moderator(interaction):
        return
    result = await call_api(interaction, api.history, pseudo=username)
    if result is not None:
        await answer(interaction, format_history(result, lang=user_lang(interaction)), private=True)


@moderation.command(name=_("cancel-game"), description=_("Cancel the Elo of a rigged game (number from /mod history)"))
@app_commands.rename(number=_("number"))
@app_commands.describe(number=_("Game number"))
async def mod_cancel(interaction: discord.Interaction, number: app_commands.Range[int, 1]):
    if await refuse_if_not_moderator(interaction):
        return
    if await call_api(interaction, api.cancel_game, number) is None:
        return
    await answer(interaction, tr("cancel_ok", user_lang(interaction), number=number), private=True)
    await mod_log(interaction.guild, tr("cancel_log", guild_lang(interaction.guild), user=interaction.user.mention,
                                        number=number))


@moderation.command(name=_("delete-account"), description=_("Delete a Splendor account (GDPR request)"))
@app_commands.rename(username=_("username"))
@app_commands.describe(username=_("Splendor username"))
async def mod_delete(interaction: discord.Interaction, username: str):
    if await refuse_if_not_moderator(interaction):
        return
    if await call_api(interaction, api.delete_account, pseudo=username) is None:
        return
    await answer(interaction, tr("delete_ok", user_lang(interaction), pseudo=username), private=True)
    await mod_log(interaction.guild, tr("delete_log", guild_lang(interaction.guild), user=interaction.user.mention,
                                        pseudo=username))


tree.add_command(moderation)


# ---------------------------------------------------------------- Démarrage

async def setup_hook():
    await tree.set_translator(FrenchTranslator())

client.setup_hook = setup_hook


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
