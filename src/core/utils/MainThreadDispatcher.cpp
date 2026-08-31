#include "MainThreadDispatcher.hpp"

std::mutex MainThreadDispatcher::s_mutex;
std::vector<std::function<void()>> MainThreadDispatcher::s_callbacks;

void MainThreadDispatcher::RunOnMainThread(std::function<void()> callback) {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_callbacks.push_back(callback);
}

void MainThreadDispatcher::Update() {
    std::vector<std::function<void()>> callbacksToRun;
    
    {
        // Quickly swap the queue to minimize the time the mutex is locked
        std::lock_guard<std::mutex> lock(s_mutex);
        callbacksToRun = std::move(s_callbacks);
        s_callbacks.clear();
    }

    // Execute all callbacks safely on the main thread
    for (auto& cb : callbacksToRun) {
        cb();
    }
}
