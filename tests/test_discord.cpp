// Test de la Rich Presence avec un faux Discord local (socket Unix, Linux).
// Vérifie : poignée de main, abonnement aux invitations, présence envoyée,
// et réception d'un « Rejoindre » venu de Discord. Lancer : make test-discord
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "json.hpp"

#include "../src/discord_rpc.hpp"

using nlohmann::json;

static bool read_exact(int fd, unsigned char* data, size_t size)
{
    size_t done = 0;
    while (done < size) {
        ssize_t got = recv(fd, data + done, size - done, 0);
        if (got <= 0) {
            return false;
        }
        done += static_cast<size_t>(got);
    }
    return true;
}

static bool read_frame(int fd, uint32_t& opcode, std::string& payload)
{
    unsigned char header[8];
    if (!read_exact(fd, header, 8)) {
        return false;
    }
    opcode = header[0] | header[1] << 8 | header[2] << 16 | static_cast<uint32_t>(header[3]) << 24;
    uint32_t length = header[4] | header[5] << 8 | header[6] << 16 | static_cast<uint32_t>(header[7]) << 24;
    payload.assign(length, '\0');
    return read_exact(fd, reinterpret_cast<unsigned char*>(&payload[0]), length);
}

static void write_frame(int fd, uint32_t opcode, const std::string& payload)
{
    unsigned char header[8];
    uint32_t length = static_cast<uint32_t>(payload.size());
    for (int i = 0; i < 4; i++) {
        header[i] = static_cast<unsigned char>(opcode >> (8 * i));
        header[4 + i] = static_cast<unsigned char>(length >> (8 * i));
    }
    send(fd, header, 8, 0);
    send(fd, payload.data(), payload.size(), 0);
}

int main()
{
    char folder_template[] = "/tmp/splendor-discord-XXXXXX";
    std::string folder = mkdtemp(folder_template);
    std::string path = folder + "/discord-ipc-0";
    setenv("SPLENDOR_DISCORD_IPC_DIR", folder.c_str(), 1);

    // Faux Discord : écoute sur discord-ipc-0
    int listener = socket(AF_UNIX, SOCK_STREAM, 0);
    sockaddr_un address = {};
    address.sun_family = AF_UNIX;
    path.copy(address.sun_path, path.size());
    assert(bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0);
    assert(listen(listener, 1) == 0);

    std::string handshake_client;
    bool subscribed = false;
    std::string details;
    std::string secret;
    std::thread fake([&]() {
        int client = accept(listener, nullptr, nullptr);
        uint32_t opcode = 0;
        std::string payload;
        assert(read_frame(client, opcode, payload) && opcode == 0);
        handshake_client = json::parse(payload)["client_id"];
        write_frame(client, 1, json({{"cmd", "DISPATCH"}, {"evt", "READY"}}).dump());
        // Lit les commandes jusqu'à recevoir la présence attendue
        while (read_frame(client, opcode, payload)) {
            json message = json::parse(payload);
            if (message.value("cmd", std::string()) == "SUBSCRIBE" && message["evt"] == "ACTIVITY_JOIN") {
                subscribed = true;
            }
            if (message.value("cmd", std::string()) == "SET_ACTIVITY") {
                json activity = message["args"]["activity"];
                details = activity.value("details", std::string());
                secret = activity.contains("secrets") ? activity["secrets"].value("join", std::string()) : "";
                if (details == "Dans un salon") {
                    // Un ami clique « Rejoindre » dans Discord
                    write_frame(client, 1,
                                json({{"cmd", "DISPATCH"}, {"evt", "ACTIVITY_JOIN"}, {"data", {{"secret", "K7Q2M"}}}}).dump());
                    break;
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        close(client);
    });

    {
        DiscordPresence presence;
        presence.start("123456789012345678");
        PresenceInfo menu;
        menu.details = "Dans les menus";
        presence.update(menu);
        std::this_thread::sleep_for(std::chrono::milliseconds(700));
        PresenceInfo lobby;
        lobby.details = "Dans un salon";
        lobby.state = "En attente de joueurs";
        lobby.party_id = "K7Q2M";
        lobby.party_size = 1;
        lobby.party_max = 3;
        lobby.join_secret = "K7Q2M";
        presence.update(lobby);

        std::string join;
        for (int i = 0; i < 50 && !presence.take_join(join); i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        assert(join == "K7Q2M");
        fake.join();
    }
    assert(handshake_client == "123456789012345678");
    assert(subscribed);
    assert(details == "Dans un salon" && secret == "K7Q2M");

    close(listener);
    std::remove(path.c_str());
    rmdir(folder.c_str());
    std::cout << "Discord : la Rich Presence et les invitations fonctionnent avec le faux Discord.\n";
    return 0;
}
