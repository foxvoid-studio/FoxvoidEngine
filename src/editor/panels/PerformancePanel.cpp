#include "PerformancePanel.hpp"
#include <imgui.h>
#include <raylib.h>
#include <string>
#include <numeric>
#include <algorithm>
#include "extras/IconsFontAwesome6.h"

PerformancePanel::PerformancePanel() 
    : m_maxHistory(120), m_updateTimer(0.0f), m_isPaused(false), m_targetFPS(60) 
{
    m_fpsHistory.resize(m_maxHistory, 0.0f);
    m_frameTimeHistory.resize(m_maxHistory, 0.0f);
}

void PerformancePanel::Draw(const Scene& activeScene) {
    ImGui::SetNextWindowSize(ImVec2(400, 380), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Performance Profiler")) {
        // Engine Controls
        if (ImGui::Button(m_isPaused ? ICON_FA_PLAY " Resume" : ICON_FA_PAUSE " Pause")) {
            m_isPaused = !m_isPaused;
        }
        
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 240);
        
        ImGui::SetNextItemWidth(120);
        if (ImGui::Combo("Target FPS", &m_targetFPS, "30\0 60\0 120\0 144\0 Unlimited\0\0")) {
            // Map the combo box index to actual FPS values
            int fpsValues[] = { 30, 60, 120, 144, 0 }; // 0 means unlimited in Raylib
            SetTargetFPS(fpsValues[m_targetFPS]);
        }

        ImGui::Separator();
        ImGui::Spacing();

        // Retrieve current metrics
        int currentFPS = GetFPS();
        float currentFrameTime = GetFrameTime() * 1000.0f;

        // Data updating
        if (!m_isPaused) {
            m_updateTimer += GetFrameTime();
            if (m_updateTimer >= 0.05f) {
                m_fpsHistory.erase(m_fpsHistory.begin());
                m_frameTimeHistory.erase(m_frameTimeHistory.begin());
                
                m_fpsHistory.push_back((float)currentFPS);
                m_frameTimeHistory.push_back(currentFrameTime);
                
                m_updateTimer = 0.0f;
            }
        }

        // Statistics Calculations
        // Calculate Min, Max and Average from the history buffer
        float avgFPS = std::accumulate(m_fpsHistory.begin(), m_fpsHistory.end(), 0.0f) / m_fpsHistory.size();
        float minFPS = *std::min_element(m_fpsHistory.begin(), m_fpsHistory.end());
        float maxFPS = *std::max_element(m_fpsHistory.begin(), m_fpsHistory.end());

        // Text Metrics
        ImGui::Text("Current FPS: %d", currentFPS);
        ImGui::SameLine(150);
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Avg: %.1f | Min: %.0f | Max: %.0f", avgFPS, minFPS, maxFPS);

        if (currentFrameTime > 16.6f) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Frame Time: %.2f ms", currentFrameTime);
        } else {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Frame Time: %.2f ms", currentFrameTime);
        }

        ImGui::Separator();

        // Graphs
        std::string fpsOverlay = "FPS (Avg: " + std::to_string((int)avgFPS) + ")";
        ImGui::PlotLines("##FPS", m_fpsHistory.data(), (int)m_fpsHistory.size(), 0, fpsOverlay.c_str(), 0.0f, 160.0f, ImVec2(ImGui::GetContentRegionAvail().x, 60));
        
        ImGui::Spacing();

        std::string timeOverlay = std::to_string(currentFrameTime).substr(0, 4) + " ms";
        ImGui::PlotHistogram("##FrameTime", m_frameTimeHistory.data(), (int)m_frameTimeHistory.size(), 0, timeOverlay.c_str(), 0.0f, 33.3f, ImVec2(ImGui::GetContentRegionAvail().x, 60));

        ImGui::Spacing();
        ImGui::Separator();

        // Scene Metrics
        ImGui::TextDisabled("Scene Data");
        
        // Count active GameObjects safely
        size_t objectCount = activeScene.GetGameObjects().size();
        ImGui::Text("Active GameObjects: %zu", objectCount);
    }
    ImGui::End();
}
