// Version navigateur de la Rich Presence : remplace discord_rpc.cpp dans la
// compilation web. Une page web ne peut pas parler à l'application Discord de
// l'ordinateur : rien n'est envoyé.
#include "discord_rpc.hpp"

struct DiscordState {};

bool PresenceInfo::operator==(const PresenceInfo& other) const
{
    return details == other.details && state == other.state && start == other.start && party_id == other.party_id
           && party_size == other.party_size && party_max == other.party_max && join_secret == other.join_secret;
}

DiscordPresence::DiscordPresence() : state_(std::make_shared<DiscordState>()) {}

DiscordPresence::~DiscordPresence() {}

void DiscordPresence::start(const std::string&) {}

void DiscordPresence::update(const PresenceInfo&) {}

bool DiscordPresence::take_join(std::string&)
{
    return false;
}

bool DiscordPresence::connected() const
{
    return false;
}
