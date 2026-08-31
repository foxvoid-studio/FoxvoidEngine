#pragma once

#include <vector>
#include <functional>
#include <mutex>

class MainThreadDispatcher {
    public:
        // Pushes a function into the queue to be safely executed on the main thread
        static void RunOnMainThread(std::function<void()> callback);

        // Executes all queued functions. Must be called once per frame in the main game loop!
        static void Update();

    private:
        static std::mutex s_mutex;
        static std::vector<std::function<void()>> s_callbacks;
};
