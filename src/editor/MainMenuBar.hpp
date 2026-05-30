#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <filesystem>
#include "scene/Scene.hpp"
#include "editor/panels/InputSettingsPanel.hpp"
#include "editor/panels/GameStatePanel.hpp"

class MainMenuBar {
    public:
        MainMenuBar() = default;
        ~MainMenuBar() = default;

        // Draws the menu bar and handles the Save as popup
        void Draw(Scene& activeScene, std::string& currentScenePath, bool& isRunning, GameObject*& selectedObject, InputSettingsPanel& inputPanel, GameStatePanel& gameStatePanel, bool& showGlobalGrid);
    private:
        bool m_openSavePopup = false;

        // Flag to control the build configuration modal
        bool m_openBuildPopup = false;

        // Multi-threading and progress tracking for the Build system
        bool m_openBuildProgressPopup = false;
};
