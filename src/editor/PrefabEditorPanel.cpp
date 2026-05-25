#include "PrefabEditorPanel.hpp"
#include <imgui.h>
#include <rlImGui.h>
#include <iostream>
#include <fstream>
#include "extras/IconsFontAwesome6.h"

PrefabEditorPanel::PrefabEditorPanel() : m_isOpen(false), m_camera(1920.0f, 1080.0f) {
    // Create a high-res texture for the prefab view. 
    // It will be scaled down by ImGui automatically.
    m_renderTexture = LoadRenderTexture(1920, 1080);
}

PrefabEditorPanel::~PrefabEditorPanel() {
    UnloadRenderTexture(m_renderTexture);
    m_prefabScene.Clear();
}

void PrefabEditorPanel::OpenPrefab(const std::string& filepath) {
    m_currentFilepath = filepath;
    m_isOpen = true;

    // Clear previous data
    m_prefabScene.Clear();

    // Since your .prefab files share the exact same JSON structure as .scene files 
    // (they both have a "gameObjects" array), we can just use the Scene loader!
    m_prefabScene.LoadFromFile(filepath);

    m_camera = EditorCamera(1920.0f, 1080.0f);

    ImGui::SetWindowFocus("Prefab Editor");
    
    std::cout << "[PrefabEditor] Opened: " << filepath << std::endl;
}

void PrefabEditorPanel::SavePrefab() {
    if (m_currentFilepath.empty()) return;

    nlohmann::json prefabJson;
    prefabJson["gameObjects"] = nlohmann::json::array();

    // Serialize every object in the isolated scene back to JSON
    for (const auto& go : m_prefabScene.GetGameObjects()) {
        prefabJson["gameObjects"].push_back(go->Serialize());
    }

    std::ofstream file(m_currentFilepath);
    if (file.is_open()) {
        file << prefabJson.dump(4);
        file.close();
        std::cout << "[PrefabEditor] Saved successfully!" << std::endl;
    } else {
        std::cerr << "[PrefabEditor] Failed to save to " << m_currentFilepath << std::endl;
    }
}

void PrefabEditorPanel::ClosePrefab() {
    m_prefabScene.Clear();
    m_currentFilepath = "";
    m_isOpen = false;
}

void PrefabEditorPanel::Draw(GameObject*& selectedObject, EditorViewMode& currentViewMode) {
    if (!m_isOpen) return;

    // 1. RENDER PASS (Virtual Environment)
    // We render the prefab into its dedicated texture BEFORE drawing the ImGui window
    BeginTextureMode(m_renderTexture);
        ClearBackground(DARKGRAY); // Distinct background color for prefab mode

        m_camera.Begin();
            m_prefabScene.Render();
        m_camera.End();
    EndTextureMode();

    // 2. UI PASS
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    if (ImGui::Begin("Prefab Editor", &m_isOpen, ImGuiWindowFlags_MenuBar)) {
        
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
            currentViewMode = EditorViewMode::Prefab;
        }

        // --- Menu Bar ---
        if (ImGui::BeginMenuBar()) {
            if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK " Save")) {
                SavePrefab();
            }
            if (ImGui::MenuItem(ICON_FA_XMARK " Close")) {
                ClosePrefab();
                selectedObject = nullptr; // Clear selection to avoid dangling pointers
            }
            
            // Display filename
            ImGui::SameLine(ImGui::GetWindowWidth() - 200);
            ImGui::TextDisabled("%s", std::filesystem::path(m_currentFilepath).filename().string().c_str());
            
            ImGui::EndMenuBar();
        }

        // --- Camera & Viewport ---
        bool isHovered = ImGui::IsWindowHovered();
        m_camera.Update(isHovered);

        ImVec2 windowSize = ImGui::GetContentRegionAvail();
        
        // Draw the render texture inverted on Y (OpenGL requirement)
        Rectangle sourceRec = { 0.0f, 0.0f, (float)m_renderTexture.texture.width, -(float)m_renderTexture.texture.height };
        rlImGuiImageRect(&m_renderTexture.texture, (int)windowSize.x, (int)windowSize.y, sourceRec);

        // Process additions/deletions in the isolated scene safely
        m_prefabScene.Flush();
    }

    ImGui::End();
    ImGui::PopStyleVar();

    // Handle the X button on the window being clicked
    if (!m_isOpen) {
        ClosePrefab();
        selectedObject = nullptr;
    }
}
