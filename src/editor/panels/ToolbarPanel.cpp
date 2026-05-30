#include "ToolbarPanel.hpp"
#include <iostream>
#include "core/GameStateManager.hpp"
#include "core/Engine.hpp"

void ToolbarPanel::Draw(Scene& activeScene, GameObject*& selectedObject, nlohmann::json& sceneBackup, EditorViewMode& currentViewMode) {
    ImGui::Begin("Toolbar");

    // Change button color and text based on the current state
    if (!Engine::Get()->IsPlaying()) {
        // Green Play Button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
        if (ImGui::Button("PLAY")) {
            Engine::Get()->SetPlaying(true);
            // Set the flag to switch to the Game view
            currentViewMode = EditorViewMode::Game;
            
            std::cout << "[Editor] Entered PLAY mode." << std::endl;
            
            selectedObject = nullptr;

            m_savedScenePathBeforePlay = Engine::Get()->GetCurrentScenePath();
            
            // Backup the scene before any scripts modify it
            sceneBackup = activeScene.Serialize();

            // Reload the fresh global variables from disk before starting
            GameStateManager::Load();

            // Start the game logic
            activeScene.Start();
        }
        ImGui::PopStyleColor();
    } else {
        // Red Stop Button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("STOP")) {
            Engine::Get()->SetPlaying(false);
            currentViewMode = EditorViewMode::Scene;

            std::cout << "[Editor] Entered EDIT mode." << std::endl;
            
            selectedObject = nullptr;

            // Restore the scene to its initial state
            activeScene.Deserialize(sceneBackup, false);

            Engine::Get()->SetCurrentScenePath(m_savedScenePathBeforePlay);

            // Restore the global variables so the Editor UI reflects the defaults
            GameStateManager::Load();
        }
        ImGui::PopStyleColor();
    }
    
    ImGui::End();
}
