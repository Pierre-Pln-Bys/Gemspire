// Fonctions propres au navigateur (voir platform_web.hpp). Compilé seulement
// pour la version WebAssembly.
#include "platform_web.hpp"

#include <cstdlib>

#include <emscripten/emscripten.h>

// Boîte de saisie du navigateur ; renvoie une chaîne allouée (à libérer), ou 0 si annulée
EM_JS(char*, ask_text, (const char* question, const char* current), {
    var answer = window.prompt(UTF8ToString(question), UTF8ToString(current));
    if (answer === null) {
        return 0;
    }
    var size = lengthBytesUTF8(answer) + 1;
    var text = _malloc(size);
    stringToUTF8(answer, text, size);
    return text;
});

// Recopie le système de fichiers (dossier /data) dans IndexedDB
EM_JS(void, sync_files, (), {
    if (typeof FS !== "undefined" && FS.syncfs) {
        FS.syncfs(false, function(error) { if (error) console.warn("Sauvegarde du navigateur :", error); });
    }
});

// Chaîne JavaScript -> chaîne allouée (à libérer)
// Dans une Activité Discord, le serveur du jeu est derrière le proxy de Discord, sur /api
EM_JS(char*, page_origin, (), {
    var text = window.location.origin + (window.gemspireActivity && gemspireActivity.active ? "/api" : "");
    var size = lengthBytesUTF8(text) + 1;
    var copy = _malloc(size);
    stringToUTF8(text, copy, size);
    return copy;
});

EM_JS(char*, browser_language, (), {
    var text = navigator.language || "en";
    var size = lengthBytesUTF8(text) + 1;
    var copy = _malloc(size);
    stringToUTF8(text, copy, size);
    return copy;
});

EM_JS(int, activity_active, (), { return window.gemspireActivity && gemspireActivity.active ? 1 : 0; });

EM_JS(char*, activity_instance, (), {
    var text = window.gemspireActivity ? gemspireActivity.instanceId : "";
    var size = lengthBytesUTF8(text) + 1;
    var copy = _malloc(size);
    stringToUTF8(text, copy, size);
    return copy;
});

EM_JS(void, activity_start, (const char* client_id), {
    if (window.gemspireActivity) {
        gemspireActivity.start(UTF8ToString(client_id));
    }
});

EM_JS(char*, activity_take, (), {
    var text = window.gemspireActivity ? gemspireActivity.take() : "";
    var size = lengthBytesUTF8(text) + 1;
    var copy = _malloc(size);
    stringToUTF8(text, copy, size);
    return copy;
});

EM_JS(void, go_to, (const char* url), { window.location.href = UTF8ToString(url); });

EM_JS(char*, take_login_parameter, (), {
    var params = new URLSearchParams(window.location.search);
    var login = params.get("login") || "";
    if (login) {
        history.replaceState(null, "", window.location.pathname);
    }
    var size = lengthBytesUTF8(login) + 1;
    var copy = _malloc(size);
    stringToUTF8(login, copy, size);
    return copy;
});

// Le secret de connexion est gardé dans le stockage de session (pas dans l'URL)
// pour survivre à l'aller-retour vers Discord / Google dans le même onglet.
EM_JS(void, store_verifier, (const char* value), {
    try { window.sessionStorage.setItem("gemspire_verifier", UTF8ToString(value)); } catch (e) {}
});

EM_JS(char*, take_verifier, (), {
    var value = "";
    try {
        value = window.sessionStorage.getItem("gemspire_verifier") || "";
        window.sessionStorage.removeItem("gemspire_verifier");
    } catch (e) {}
    var size = lengthBytesUTF8(value) + 1;
    var copy = _malloc(size);
    stringToUTF8(value, copy, size);
    return copy;
});

static std::string take_string(char* text)
{
    std::string result = text != nullptr ? text : "";
    std::free(text);
    return result;
}

std::string web_origin()
{
    return take_string(page_origin());
}

void web_go_to(const std::string& url)
{
    go_to(url.c_str());
}

bool web_in_activity()
{
    return activity_active() != 0;
}

std::string web_activity_instance()
{
    return take_string(activity_instance());
}

void web_activity_start(const std::string& client_id)
{
    activity_start(client_id.c_str());
}

std::string web_activity_take()
{
    return take_string(activity_take());
}

std::string web_language()
{
    return take_string(browser_language());
}

std::string web_login_from_address()
{
    return take_string(take_login_parameter());
}

void web_remember_verifier(const std::string& verifier)
{
    store_verifier(verifier.c_str());
}

std::string web_take_verifier()
{
    return take_string(take_verifier());
}

bool web_prompt(const std::string& question, std::string& value)
{
    char* answer = ask_text(question.c_str(), value.c_str());
    if (answer == nullptr) {
        return false;
    }
    value = answer;
    std::free(answer);
    return true;
}

void web_save_files()
{
    sync_files();
}
