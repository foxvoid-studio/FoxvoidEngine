#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <filesystem>
#include "../world/Scene.hpp"
#include "InputSettingsPanel.hpp"
#include "GameStatePanel.hpp"

class MainMenuBar {
    public:
        MainMenuBar() = default;
        ~MainMenuBar() = default;

        // Draws the menu bar and handles the Save as popup
        void Draw(Scene& activeScene, std::string& currentScenePath, bool& isRunning, GameObject*& selectedObject, InputSettingsPanel& inputPanel, GameStatePanel& gameStatePanel, bool& showGrid2D, bool& showGrid3D);
    private:
        bool m_openSavePopup = false;

        // Flag to control the build configuration modal
        bool m_openBuildPopup = false;

        // Multi-threading and progress tracking for the Build system
        bool m_openBuildProgressPopup = false;
};
