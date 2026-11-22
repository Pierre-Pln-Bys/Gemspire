// Version navigateur (WebAssembly) de NetClient : remplace net.cpp dans la
// compilation web. Les requêtes passent par fetch() du navigateur (API
// emscripten_fetch), sans fil d'exécution : la réponse arrive dans un rappel,
// puis la boucle de jeu la récupère avec poll(), comme dans la version de
// bureau. Le jeu est servi par le serveur lui-même (/play) : les adresses sont
// relatives, donc toujours vers le bon serveur.
#include "net.hpp"

#include <cstring>
#include <mutex>
#include <random>

#include <emscripten/fetch.h>

#include "version.hpp"

struct NetQueue {
    std::vector<NetResponse> done;
    int pending = 0;
};

// Ce qu'il faut retenir d'une requête en cours (libéré dans le rappel)
struct WebRequest {
    std::shared_ptr<NetQueue> queue;
    int tag;
    bool background;
    std::string body;                 // doit vivre jusqu'à la fin de la requête
    std::vector<std::string> headers; // idem (fetch garde des pointeurs)
    std::vector<const char*> header_pointers;
};

static void finished(emscripten_fetch_t* fetch)
{
    WebRequest* request = static_cast<WebRequest*>(fetch->userData);
    NetResponse response = {request->tag, fetch->status, std::string(fetch->data, fetch->numBytes)};
    if (!request->background) {
        request->queue->pending--;
    }
    request->queue->done.push_back(response);
    emscripten_fetch_close(fetch);
    delete request;
}

NetClient::NetClient() : queue_(std::make_shared<NetQueue>()) {}

void NetClient::set_server(const std::string& url)
{
    server_ = url;  // gardée pour l'affichage ; les requêtes restent relatives
}

void NetClient::set_token(const std::string& token)
{
    token_ = token;
}

const std::string& NetClient::server() const
{
    return server_;
}

void NetClient::send(int tag, const std::string& method, const std::string& path, const std::string& body,
                     bool background)
{
    WebRequest* request = new WebRequest{queue_, tag, background, body.empty() ? "{}" : body, {}, {}};
    request->headers = {"X-Gemspire-Version", SPLENDOR_VERSION, "Content-Type", "application/json"};
    if (!token_.empty()) {
        request->headers.push_back("Authorization");
        request->headers.push_back("Bearer " + token_);
    }
    for (const std::string& header : request->headers) {
        request->header_pointers.push_back(header.c_str());
    }
    request->header_pointers.push_back(nullptr);
    if (!background) {
        queue_->pending++;
    }

    emscripten_fetch_attr_t attributes;
    emscripten_fetch_attr_init(&attributes);
    std::strncpy(attributes.requestMethod, method.c_str(), sizeof(attributes.requestMethod) - 1);
    attributes.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attributes.onsuccess = finished;
    attributes.onerror = finished;  // status 0 si le serveur n'a pas répondu
    attributes.userData = request;
    attributes.requestHeaders = request->header_pointers.data();
    attributes.timeoutMSecs = 40000;  // plus long que l'attente longue du serveur
    if (method == "POST") {
        attributes.requestData = request->body.c_str();
        attributes.requestDataSize = request->body.size();
    }
    emscripten_fetch(&attributes, path.c_str());
}

std::vector<NetResponse> NetClient::poll()
{
    std::vector<NetResponse> done;
    done.swap(queue_->done);
    return done;
}

int NetClient::pending() const
{
    return queue_->pending;
}

std::string random_login_id()
{
    const std::string letters = "abcdefghijklmnopqrstuvwxyz0123456789";
    std::random_device device;
    std::string id;
    for (int i = 0; i < 32; i++) {
        id += letters[device() % letters.size()];
    }
    return id;
}
