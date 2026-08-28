#include "CloudManager.hpp"
#include "network/HttpClient.hpp"
#include <iostream>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <cstdlib>

EM_JS(char*, GetFoxvoidJWT, (), {
    var token = (window.FoxvoidConfig && window.FoxvoidConfig.jwtToken) ? window.FoxvoidConfig.jwtToken : "";
    var lengthBytes = lengthBytesUTF8(token) + 1;
    var stringOnWasmHeap = _malloc(lengthBytes);
    stringToUTF8(token, stringOnWasmHeap, lengthBytes);
    return stringOnWasmHeap;
});

EM_JS(char*, GetFoxvoidGameKey, (), {
    var key = (window.FoxvoidConfig && window.FoxvoidConfig.gameKey) ? window.FoxvoidConfig.gameKey : "";
    var lengthBytes = lengthBytesUTF8(key) + 1;
    var stringOnWasmHeap = _malloc(lengthBytes);
    stringToUTF8(key, stringOnWasmHeap, lengthBytes);
    return stringOnWasmHeap;
});

#endif

// Static variables initialization
std::string CloudManager::s_apiBaseUrl = "";
std::string CloudManager::s_gameSlug = "";
std::string CloudManager::s_jwtToken = "";
std::string CloudManager::s_gameKey = "";

void CloudManager::Initialize(const std::string& apiBaseUrl, const std::string& gameSlug, const std::string& desktopGameKey, const std::string& desktopJwt) {
    s_apiBaseUrl = apiBaseUrl;
    s_gameSlug = gameSlug;

#ifdef __EMSCRIPTEN__
    // On WebAssembly, extract secrets securely injected in the HTML context
    char* rawToken = GetFoxvoidJWT();
    s_jwtToken = std::string(rawToken);
    free(rawToken);

    char* rawGameKey = GetFoxvoidGameKey();
    s_gameKey = std::string(rawGameKey);
    free(rawGameKey);
#else
    // On Desktop/Mobile, use the provided parameters (could be loaded from disk)
    s_gameKey = desktopGameKey;
    s_jwtToken = desktopJwt;
#endif

    std::cout << "[Cloud Manager] Initialized. Auth Status: " << (IsAuthenticated() ? "Logged in" : "Guest") << std::endl;
}

void CloudManager::SetAuthData(const std::string& jwtToken, const std::string& gameKey) {
    s_jwtToken = jwtToken;
    if (!gameKey.empty()) {
        s_gameKey = gameKey;
    }
}

bool CloudManager::IsAuthenticated() {
    return !s_jwtToken.empty() && !s_gameKey.empty();
}

void CloudManager::PullSave(const std::string& saveKey, CloudSuccessCallback onSuccess, CloudErrorCallback onError) {
    if (!IsAuthenticated()) {
        if (onError) onError("CloudManager Error: Not authenticated.");
        return;
    }

    std::string url = s_apiBaseUrl + "/api/saves/" + s_gameSlug + "/" + saveKey;
    std::unordered_map<std::string, std::string> headers = {
        {"Authorization", "Bearer " + s_jwtToken},
        {"X-Game-Key", s_gameKey},
        {"Accept", "application/json"}
    };

    HttpClient::Get(url, headers, [onSuccess, onError](const HttpResponse& response) {
        if (response.statusCode == 200) {
            try {
                nlohmann::json responseJson = nlohmann::json::parse(response.body);
                if (onSuccess) onSuccess(responseJson["data"]);
            } catch (const std::exception& e) {
                if (onError) onError("CloudManager Error: Invalid JSON parsing. " + std::string(e.what()));
            }
        } else {
            if (onError) onError("CloudManager Error: Pull failed. HTTP " + std::to_string(response.statusCode));
        }
    }, onError);
}

void CloudManager::PushSave(const std::string& saveKey, const nlohmann::json& data, CloudSuccessCallback onSuccess, CloudErrorCallback onError) {
    if (!IsAuthenticated()) {
        if (onError) onError("CloudManager Error: Not authenticated.");
        return;
    }

    std::string url = s_apiBaseUrl + "/api/saves/" + s_gameSlug;
    std::unordered_map<std::string, std::string> headers = {
        {"Authorization", "Bearer " + s_jwtToken},
        {"X-Game-Key", s_gameKey},
        {"Content-Type", "application/json"}
    };

    nlohmann::json payload;
    payload["key"] = saveKey;
    payload["data"] = data;

    HttpClient::Post(url, headers, payload.dump(), [onSuccess, onError](const HttpResponse& response) {
        if (response.statusCode == 200 || response.statusCode == 201) {
            try {
                nlohmann::json responseJson = nlohmann::json::parse(response.body);
                if (onSuccess) onSuccess(responseJson["data"]);
            }
            catch (const std::exception& e) {
                if (onError) onError("CloudManager Error: Invalid JSON parsing. " + std::string(e.what()));
            }
        }
        else {
            if (onError) onError("CloudManager Error: Push failed. HTTP " + std::to_string(response.statusCode));
        }
    }, onError);
}

void CloudManager::PullInventory(CloudSuccessCallback onSuccess, CloudErrorCallback onError) {
    if (!IsAuthenticated()) {
        if (onError) onError("CloudManager Error: Not authenticated");
        return;
    }

    std::string url = s_apiBaseUrl + "/api/inventory/" + s_gameSlug;
    std::unordered_map<std::string, std::string> headers = {
        {"Authorization", "Bearer " + s_jwtToken},
        {"X-Game-Key", s_gameKey},
        {"Accept", "application/json"}
    };

    HttpClient::Get(url, headers, [onSuccess, onError](const HttpResponse& response) {
        if (response.statusCode == 200) {
            try {
                nlohmann::json responseJson = nlohmann::json::parse(response.body);
                if (onSuccess) onSuccess(responseJson);
            }
            catch (const std::exception& e) {
                if (onError) onError("CloudManager Error: Invalid JSON parsing in Inventory. " + std::string(e.what()));
            }
        }
        else {
            if (onError) onError("CloudManager Error: Pull Inventory failed. HTTP " + std::to_string(response.statusCode));
        }
    }, onError);
}
