#pragma once

#include <string>
#include <functional>
#include <nlohmann/json.hpp>

class [[gnu::visibility("default")]] CloudManager {
    public:
        using CloudSuccessCallback = std::function<void(const nlohmann::json&)>;
        using CloudErrorCallback = std::function<void(const std::string&)>;

        // Initializes the cloud service. 
        // On WASM, desktop parameters are ignored (fetches from DOM).
        // On Desktop, requires explicit GameKey (and JWT if already logged in).
        static void Initialize(const std::string& apiBaseUrl, const std::string& gameSlug, const std::string& desktopGameKey = "", const std::string& desktopJwt = "");

        static bool IsAuthenticated();

        // Updates authentication tokens (useful when the player logs in via UI on Desktop)
        static void SetAuthData(const std::string& jwtToken, const std::string& gameKey);

        static void PullSave(const std::string& saveKey, CloudSuccessCallback onSuccess, CloudErrorCallback onError);
        static void PushSave(const std::string& saveKey, const nlohmann::json& data, CloudSuccessCallback onSuccess, CloudErrorCallback onError);

        static void PullInventory(CloudSuccessCallback onSuccess, CloudErrorCallback onError);
        
    private: 
        static std::string s_apiBaseUrl;
        static std::string s_gameSlug;
        static std::string s_jwtToken;
        static std::string s_gameKey; 
};
