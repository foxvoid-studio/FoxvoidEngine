#pragma once

#include <string>
#include <functional>
#include <unordered_map>
#include <vector>
#include <nlohmann/json.hpp>

// Define the callback signature. Every event can carry an optional JSON payload.
using EventCallback = std::function<void(const nlohmann::json&)>;

class EventBus {
    public:
        // Subscribe to an event, returns a unique Listener ID (useful for unsubscribing)
        static uint64_t Subscribe(const std::string& eeventName, EventCallback callback);

        // Remove a specific listener using its ID
        static void Unsubscribe(const std::string& eventName, uint64_t listenerId);

        // Publish an event to all subscribers with an optional JSON payload
        static void Publish(const std::string& eventName, const nlohmann::json& payload = nlohmann::json::object());

        // Clears all active events and listeners (Crucial when unloading a Scene)
        static void Clear();

    private:
        struct Listener {
            uint64_t id;
            EventCallback callback;
        };

        // Stores a list of listeners for each event name
        static std::unordered_map<std::string, std::vector<Listener>> s_listeners;
        static uint64_t s_nextListenerId;
};
