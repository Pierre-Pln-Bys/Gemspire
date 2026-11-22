#include "discord_rpc.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <thread>
#include <vector>

#include "json.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

bool PresenceInfo::operator==(const PresenceInfo& other) const
{
    return details == other.details && state == other.state && start == other.start && party_id == other.party_id
           && party_size == other.party_size && party_max == other.party_max && join_secret == other.join_secret;
}

// ---------------------------------------------------------------- Connexion locale à Discord

// Une connexion à l'application Discord (socket Unix ou tube nommé)
class Pipe {
public:
    ~Pipe() { close(); }

#ifdef _WIN32
    bool open()
    {
        for (int i = 0; i < 10; i++) {
            std::string name = "\\\\.\\pipe\\discord-ipc-" + std::to_string(i);
            handle_ = CreateFileA(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
            if (handle_ != INVALID_HANDLE_VALUE) {
                return true;
            }
        }
        return false;
    }
    void close()
    {
        if (handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_);
            handle_ = INVALID_HANDLE_VALUE;
        }
    }
    bool is_open() const { return handle_ != INVALID_HANDLE_VALUE; }
    bool write_all(const std::vector<unsigned char>& data)
    {
        DWORD written = 0;
        return WriteFile(handle_, data.data(), static_cast<DWORD>(data.size()), &written, nullptr) != 0
               && written == data.size();
    }
    // Attend des données au plus `milliseconds` ; false si rien
    bool wait_readable(int milliseconds)
    {
        for (int waited = 0; waited <= milliseconds; waited += 50) {
            DWORD available = 0;
            if (!PeekNamedPipe(handle_, nullptr, 0, nullptr, &available, nullptr)) {
                close();
                return false;
            }
            if (available > 0) {
                return true;
            }
            Sleep(50);
        }
        return false;
    }
    bool read_all(unsigned char* data, size_t size)
    {
        size_t done = 0;
        while (done < size) {
            DWORD read = 0;
            if (!ReadFile(handle_, data + done, static_cast<DWORD>(size - done), &read, nullptr) || read == 0) {
                close();
                return false;
            }
            done += read;
        }
        return true;
    }

private:
    HANDLE handle_ = INVALID_HANDLE_VALUE;
#else
    bool open()
    {
        // Emplacements possibles du socket (normal, flatpak, snap)
        std::vector<std::string> folders;
        const char* custom = std::getenv("SPLENDOR_DISCORD_IPC_DIR");  // pour les tests
        if (custom != nullptr) {
            folders.push_back(custom);
        } else {
            for (const char* variable : {"XDG_RUNTIME_DIR", "TMPDIR", "TMP", "TEMP"}) {
                const char* value = std::getenv(variable);
                if (value != nullptr) {
                    folders.push_back(value);
                    folders.push_back(std::string(value) + "/app/com.discordapp.Discord");
                    folders.push_back(std::string(value) + "/snap.discord");
                }
            }
            folders.push_back("/tmp");
        }
        for (const std::string& folder : folders) {
            for (int i = 0; i < 10; i++) {
                std::string path = folder + "/discord-ipc-" + std::to_string(i);
                sockaddr_un address = {};
                address.sun_family = AF_UNIX;
                if (path.size() >= sizeof(address.sun_path)) {
                    continue;
                }
                path.copy(address.sun_path, path.size());
                socket_ = socket(AF_UNIX, SOCK_STREAM, 0);
                if (socket_ < 0) {
                    return false;
                }
                if (connect(socket_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0) {
                    return true;
                }
                ::close(socket_);
                socket_ = -1;
            }
        }
        return false;
    }
    void close()
    {
        if (socket_ >= 0) {
            ::close(socket_);
            socket_ = -1;
        }
    }
    bool is_open() const { return socket_ >= 0; }
    bool write_all(const std::vector<unsigned char>& data)
    {
        size_t done = 0;
        while (done < data.size()) {
            ssize_t sent = send(socket_, data.data() + done, data.size() - done, MSG_NOSIGNAL);
            if (sent <= 0) {
                close();
                return false;
            }
            done += static_cast<size_t>(sent);
        }
        return true;
    }
    bool wait_readable(int milliseconds)
    {
        pollfd fd = {socket_, POLLIN, 0};
        return poll(&fd, 1, milliseconds) > 0;
    }
    bool read_all(unsigned char* data, size_t size)
    {
        size_t done = 0;
        while (done < size) {
            ssize_t got = recv(socket_, data + done, size - done, 0);
            if (got <= 0) {
                close();
                return false;
            }
            done += static_cast<size_t>(got);
        }
        return true;
    }

private:
    int socket_ = -1;
#endif

public:
    // Une trame : code (4 octets), longueur (4 octets), puis du JSON
    bool send_frame(uint32_t opcode, const std::string& payload)
    {
        std::vector<unsigned char> data(8 + payload.size());
        uint32_t length = static_cast<uint32_t>(payload.size());
        for (int i = 0; i < 4; i++) {
            data[i] = static_cast<unsigned char>((opcode >> (8 * i)) & 0xFF);
            data[4 + i] = static_cast<unsigned char>((length >> (8 * i)) & 0xFF);
        }
        payload.copy(reinterpret_cast<char*>(data.data() + 8), payload.size());
        return write_all(data);
    }

    bool read_frame(uint32_t& opcode, std::string& payload)
    {
        unsigned char header[8];
        if (!read_all(header, 8)) {
            return false;
        }
        opcode = 0;
        uint32_t length = 0;
        for (int i = 0; i < 4; i++) {
            opcode |= static_cast<uint32_t>(header[i]) << (8 * i);
            length |= static_cast<uint32_t>(header[4 + i]) << (8 * i);
        }
        if (length > 64 * 1024) {
            close();
            return false;
        }
        payload.assign(length, '\0');
        return length == 0 || read_all(reinterpret_cast<unsigned char*>(&payload[0]), length);
    }
};

// ---------------------------------------------------------------- Fil Discord

struct DiscordState {
    std::mutex mutex;
    std::condition_variable wake;
    std::string application_id;
    PresenceInfo wanted;
    bool has_wanted = false;
    std::vector<std::string> joins;
    std::atomic<bool> stop{false};
    std::atomic<bool> connected{false};
    std::thread thread;
};

static int process_id()
{
#ifdef _WIN32
    return static_cast<int>(GetCurrentProcessId());
#else
    return static_cast<int>(getpid());
#endif
}

static nlohmann::json activity_json(const PresenceInfo& info)
{
    nlohmann::json activity = nlohmann::json::object();
    if (!info.details.empty()) {
        activity["details"] = info.details;
    }
    if (!info.state.empty()) {
        activity["state"] = info.state;
    }
    if (info.start > 0) {
        activity["timestamps"] = {{"start", info.start}};
    }
    if (!info.party_id.empty()) {
        activity["party"] = {{"id", info.party_id}, {"size", {info.party_size, info.party_max}}};
    }
    if (!info.join_secret.empty()) {
        activity["secrets"] = {{"join", info.join_secret}};
    }
    activity["instance"] = false;
    return activity;
}

static void run(std::shared_ptr<DiscordState> state)
{
    Pipe pipe;
    PresenceInfo sent;
    bool sent_once = false;
    int nonce = 0;
    while (!state->stop) {
        if (!pipe.is_open()) {
            state->connected = false;
            sent_once = false;
            // Connexion et poignée de main (Discord peut être lancé plus tard)
            if (pipe.open()) {
                nlohmann::json hello = {{"v", 1}, {"client_id", state->application_id}};
                uint32_t opcode = 0;
                std::string payload;
                if (pipe.send_frame(0, hello.dump()) && pipe.wait_readable(3000) && pipe.read_frame(opcode, payload)
                    && opcode == 1) {
                    state->connected = true;
                    nlohmann::json subscribe = {{"cmd", "SUBSCRIBE"}, {"evt", "ACTIVITY_JOIN"},
                                                {"nonce", std::to_string(++nonce)}};
                    pipe.send_frame(1, subscribe.dump());
                } else {
                    pipe.close();
                }
            }
            if (!pipe.is_open()) {
                std::unique_lock<std::mutex> lock(state->mutex);
                state->wake.wait_for(lock, std::chrono::seconds(15), [&]() { return state->stop.load(); });
                continue;
            }
        }
        // Envoie la présence si elle a changé
        PresenceInfo wanted;
        bool has_wanted = false;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            wanted = state->wanted;
            has_wanted = state->has_wanted;
        }
        if (has_wanted && (!sent_once || !(wanted == sent))) {
            nlohmann::json command = {{"cmd", "SET_ACTIVITY"},
                                      {"args", {{"pid", process_id()}, {"activity", activity_json(wanted)}}},
                                      {"nonce", std::to_string(++nonce)}};
            if (pipe.send_frame(1, command.dump())) {
                sent = wanted;
                sent_once = true;
            }
        }
        // Messages de Discord : invitations acceptées, ping
        if (pipe.wait_readable(500)) {
            uint32_t opcode = 0;
            std::string payload;
            if (!pipe.read_frame(opcode, payload)) {
                continue;
            }
            if (opcode == 3) {  // ping -> pong
                pipe.send_frame(4, payload);
            } else if (opcode == 2) {  // fermeture
                pipe.close();
            } else if (opcode == 1) {
                // Attention : Discord répond à chaque commande avec "evt": null
                nlohmann::json message = nlohmann::json::parse(payload, nullptr, false);
                if (message.is_object() && message.contains("evt") && message["evt"] == "ACTIVITY_JOIN"
                    && message.contains("data") && message["data"].contains("secret")
                    && message["data"]["secret"].is_string()) {
                    std::lock_guard<std::mutex> lock(state->mutex);
                    state->joins.push_back(message["data"]["secret"].get<std::string>());
                }
            }
        }
    }
    pipe.close();
}

// ---------------------------------------------------------------- DiscordPresence

DiscordPresence::DiscordPresence() : state_(std::make_shared<DiscordState>()) {}

DiscordPresence::~DiscordPresence()
{
    state_->stop = true;
    state_->wake.notify_all();
    if (state_->thread.joinable()) {
        state_->thread.join();
    }
}

void DiscordPresence::start(const std::string& application_id)
{
    if (application_id.empty() || application_id.find_first_not_of("0123456789") != std::string::npos
        || state_->thread.joinable()) {
        return;
    }
    state_->application_id = application_id;
    state_->thread = std::thread(run, state_);
}

void DiscordPresence::update(const PresenceInfo& info)
{
    std::lock_guard<std::mutex> lock(state_->mutex);
    state_->wanted = info;
    state_->has_wanted = true;
}

bool DiscordPresence::take_join(std::string& secret)
{
    std::lock_guard<std::mutex> lock(state_->mutex);
    if (state_->joins.empty()) {
        return false;
    }
    secret = state_->joins.front();
    state_->joins.erase(state_->joins.begin());
    return true;
}

bool DiscordPresence::connected() const
{
    return state_->connected;
}
