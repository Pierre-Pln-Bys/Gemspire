"""Bot Discord « Crée ta partie » pour Splendor.

Réglages (variables d'environnement) :
  DISCORD_BOT_TOKEN    jeton du bot (portail développeur Discord)
  SPLENDOR_API         adresse du serveur Splendor (défaut http://127.0.0.1:8080)
  SPLENDOR_BOT_KEY     clé partagée avec le serveur (même valeur des deux côtés)
  CREATE_CHANNEL_NAME  nom du salon vocal « déclencheur » (défaut « Crée ta partie »)

Permissions du bot sur le serveur Discord : gérer les salons, déplacer des
membres, voir les salons, envoyer des messages.
"""

import asyncio
import logging
import os

import discord

from api import SplendorApi
from logic import (CREATE_CHANNEL_NAME, OFFER_RETRY_COUNT, OFFER_RETRY_SECONDS, TempChannel, apply_event,
                   is_create_channel, offer_message, should_delete, temp_channel_name)

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
log = logging.getLogger("splendor-bot")

api = SplendorApi(os.environ.get("SPLENDOR_API", "http://127.0.0.1:8080"), os.environ["SPLENDOR_BOT_KEY"])
create_name = os.environ.get("CREATE_CHANNEL_NAME", CREATE_CHANNEL_NAME)

intents = discord.Intents.none()
intents.guilds = True
intents.voice_states = True
client = discord.Client(intents=intents)

temp_channels = {}  # identifiant du salon vocal temporaire -> TempChannel


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


@client.event
async def on_voice_state_update(member, before, after):
    # Entrée dans « Crée ta partie » : salon temporaire, puis proposition dans le jeu
    if after.channel is not None and before.channel != after.channel and is_create_channel(after.channel.name, create_name):
        guild = after.channel.guild
        channel = await guild.create_voice_channel(temp_channel_name(member.display_name),
                                                   category=after.channel.category,
                                                   reason="Splendor : Crée ta partie")
        temp_channels[channel.id] = TempChannel(member.id)
        await member.move_to(channel)
        log.info("Salon temporaire %s créé pour %s", channel.id, member.id)
        asyncio.create_task(offer_game(member, channel))

    # Sortie d'un salon temporaire : suppression s'il est vide
    if before.channel is not None and before.channel != after.channel:
        if should_delete(before.channel.id in temp_channels, len(before.channel.members)):
            temp_channels.pop(before.channel.id, None)
            await before.channel.delete(reason="Splendor : salon temporaire vide")
            log.info("Salon temporaire %s supprimé", before.channel.id)


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


@client.event
async def on_ready():
    log.info("Connecté en tant que %s", client.user)
    if not getattr(client, "events_task", None):
        client.events_task = asyncio.create_task(follow_events())


if __name__ == "__main__":
    client.run(os.environ["DISCORD_BOT_TOKEN"], log_handler=None)
