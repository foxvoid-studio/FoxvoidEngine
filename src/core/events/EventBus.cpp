#include "EventBus.hpp"
#include <iostream>

// Static variables initialization
std::unordered_map<std::string, std::vector<EventBus::Listener>> EventBus::s_listeners;
uint64_t EventBus::s_nextListenerId = 1;

uint64_t EventBus::Subscribe(const std::string& eventName, EventCallback callback) {
    uint64_t id = s_nextListenerId++;
    s_listeners[eventName].push_back({id, std::move(callback)});
    return id;
}

void EventBus::Unsubscribe(const std::string& eventName, uint64_t listenerId) {
    auto it = s_listeners.find(eventName);
    if (it != s_listeners.end()) {
        auto& listeners = it->second;
        // C++20 erase_if
        std::erase_if(listeners, [listenerId](const Listener& listener) {
            return listener.id == listenerId;
        });
    }
}

void EventBus::Publish(const std::string& eventName, const nlohmann::json& payload) {
    auto it = s_listeners.find(eventName);
    if (it != s_listeners.end()) {
        // IMPORTANT: We make a copy of the listeners list before iterating!
        // If a callback decides to Subscribe or Unsubscribe mid-loop, 
        // modifying the original vector would invalidate iterators and crash the engine.
        auto listenersCopy = it->second;
        for (const auto& listener : listenersCopy) {
            if (listener.callback) {
                listener.callback(payload);
            }
        }
    }
}

void EventBus::Clear() {
    s_listeners.clear();
    s_nextListenerId = 1;
}
