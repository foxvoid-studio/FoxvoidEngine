#include "PubManager.hpp"
#include "core/utils/MainThreadDispatcher.hpp"
#include <iostream>
#include <cstdlib>
#include <random>

#ifdef __EMSCRIPTEN__
    #include <emscripten.h>
#elif defined(__ANDROID__)
    // Android headers will be added later
#else
    #include <thread>
    #include <chrono>
    #include <network/httplib.h>
    #include <nlohmann/json.hpp>
#endif

// Generates a 16-character alphanumeric session ID for the Desktop ad flow
#ifndef __EMSCRIPTEN__
#ifndef __ANDROID__
static std::string GenerateAdSessionId() {
    static const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::string tmp_s;
    tmp_s.reserve(16);
    for (int i = 0; i < 16; ++i) {
        tmp_s += alphanum[rand() % (sizeof(alphanum) - 1)];
    }
    return tmp_s;
}
#endif
#endif

void PubManager::Initialize() {
#ifdef __EMSCRIPTEN__
    std::cout << "[PubManager] Initialized for WebAssembly." << std::endl;
#elif defined(__ANDROID__)
    std::cout << "[PubManager] Android implementation deferred." << std::endl;
#else
    std::cout << "[PubManager] Initialized for Desktop (Django Fallback)." << std::endl;
#endif
}

void PubManager::ShowRewardedAd(const std::string& placementId, RewardCallback onReward, ErrorCallback onError) {
#ifdef __EMSCRIPTEN__
    // --- WEBASSEMBLY IMPLEMENTATION ---
    std::cout << "[PubManager] Triggering JS HTML5 Overlay for ad: " << placementId << std::endl;
    
    MAIN_THREAD_EM_ASM({
        if (typeof window.showFoxvoidAd === 'function') {
            window.showFoxvoidAd(UTF8ToString($0))
                .then(() => {
                    console.log("Ad finished, reward granted.");
                })
                .catch((err) => {
                    console.error("Ad failed.", err);
                });
        } else {
            console.error("Ad overlay system not found in JS context!");
        }
    }, placementId.c_str());
    
    // For testing purposes, immediately reward via the dispatcher
    MainThreadDispatcher::RunOnMainThread([onReward]() {
        if (onReward) onReward();
    });

#elif defined(__ANDROID__)
    // --- ANDROID IMPLEMENTATION ---
    std::cout << "[PubManager] AdMob call deferred on Android." << std::endl;
    
    // Safely return an error since it's not implemented yet
    MainThreadDispatcher::RunOnMainThread([onError]() {
        if (onError) onError("Ads are not yet implemented on Android.");
    });
    
#else
    // --- DESKTOP IMPLEMENTATION (Linux / Windows) ---
    std::string sessionId = GenerateAdSessionId();
    
    // Base URL of your Django Server
    std::string baseUrl = "http://127.0.0.1:8000";
    
    // Point to the Django Class-Based View we created in ads/urls.py
    std::string browserUrl = baseUrl + "/ads/watch/?placement=" + placementId + "&session=" + sessionId;

    // 1. Open the system's default browser
    std::string command;
#ifdef _WIN32
    command = "start \"\" \"" + browserUrl + "\""; // Windows
#else
    command = "xdg-open \"" + browserUrl + "\""; // Linux (Fedora/Ubuntu)
#endif
    
    system(command.c_str());
    std::cout << "[PubManager] Opened browser for Desktop Ad. Session: " << sessionId << std::endl;

    // 2. Start a detached thread to poll the Django Ninja Extra API
    std::thread([baseUrl, sessionId, onReward, onError]() {
        httplib::Client cli(baseUrl);
        bool rewardGranted = false;
        int attempts = 0;
        const int maxAttempts = 60; // Timeout after 60 seconds

        while (!rewardGranted && attempts < maxAttempts) {
            // Wait 1 second between each check
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // Poll the Django API Endpoint
            std::string pollPath = "/api/ads/status?session_id=" + sessionId;
            if (auto res = cli.Get(pollPath.c_str())) {
                if (res->status == 200) {
                    try {
                        auto j = nlohmann::json::parse(res->body);
                        if (j.value("status", "") == "completed") {
                            rewardGranted = true;
                            break;
                        }
                    } catch (...) {
                        // Ignore JSON parsing errors and try again next loop
                    }
                }
            }
            attempts++;
        }

        // 3. Return safely to the Main Thread once polling finishes
        MainThreadDispatcher::RunOnMainThread([rewardGranted, onReward, onError]() {
            if (rewardGranted) {
                std::cout << "[PubManager] Desktop Ad completed successfully!" << std::endl;
                if (onReward) onReward();
            } else {
                std::cout << "[PubManager] Desktop Ad timed out." << std::endl;
                if (onError) onError("Ad verification timed out.");
            }
        });
    }).detach();

#endif
}
