#include "net.hpp"

#include <mutex>
#include <random>
#include <thread>

#include "httplib.h"

struct NetQueue {
    std::mutex mutex;
    std::vector<NetResponse> done;
    int pending = 0;
};

NetClient::NetClient() : queue_(std::make_shared<NetQueue>()) {}

void NetClient::set_server(const std::string& url)
{
    server_ = url;
    while (!server_.empty() && server_.back() == '/') {
        server_.pop_back();
    }
}

void NetClient::set_token(const std::string& token)
{
    token_ = token;
}

const std::string& NetClient::server() const
{
    return server_;
}

void NetClient::send(int tag, const std::string& method, const std::string& path, const std::string& body)
{
    std::shared_ptr<NetQueue> queue = queue_;
    {
        std::lock_guard<std::mutex> lock(queue->mutex);
        queue->pending++;
    }
    std::string server = server_;
    std::string token = token_;
    // Un fil par requête : la fenêtre continue de s'afficher pendant l'attente.
    // Il ne touche qu'à la file partagée (shared_ptr), jamais à l'interface.
    std::thread([queue, server, token, tag, method, path, body]() {
        NetResponse response = {tag, 0, ""};
        try {
            httplib::Client client(server);
            client.set_connection_timeout(5);
            client.set_read_timeout(40);  // plus long que l'attente longue du serveur
            httplib::Headers headers;
            if (!token.empty()) {
                headers.emplace("Authorization", "Bearer " + token);
            }
            httplib::Result result = method == "POST"
                                         ? client.Post(path, headers, body.empty() ? "{}" : body, "application/json")
                                         : client.Get(path, headers);
            if (result) {
                response.status = result->status;
                response.body = result->body;
            }
        } catch (...) {
            response.status = 0;  // adresse invalide, etc.
        }
        std::lock_guard<std::mutex> lock(queue->mutex);
        queue->pending--;
        queue->done.push_back(response);
    }).detach();
}

std::vector<NetResponse> NetClient::poll()
{
    std::lock_guard<std::mutex> lock(queue_->mutex);
    std::vector<NetResponse> done;
    done.swap(queue_->done);
    return done;
}

int NetClient::pending() const
{
    std::lock_guard<std::mutex> lock(queue_->mutex);
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
