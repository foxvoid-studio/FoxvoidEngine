#pragma once

#include <vector>
#include <scene/Scene.hpp>

class PerformancePanel {
    public:
        PerformancePanel();
        ~PerformancePanel() = default;

        // Draws the performance UI window
        // Takes a reference to a boolean so the window can close itself
        void Draw(const Scene& activeScene);

    private:
        // History buffers for the graphs
        std::vector<float> m_fpsHistory;
        std::vector<float> m_frameTimeHistory;

        int m_maxHistory; // How many points to keep in the graph
        float m_updateTimer;   // Timer to slow down graph updates for readability

        bool m_isPaused; // Freezes the graph updates
        int m_targetFPS; // Used to change the engine's target framerate
};
