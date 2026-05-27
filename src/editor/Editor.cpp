#include "Editor.hpp"
#include "extras/IconsFontAwesome6.h"
#include "graphics/TileMap.hpp"
#include "physics/PhysicsEngine.hpp"
#include <iostream>
#include <core/ProjectSettings.hpp>
#include <scripting/ScriptEngine.hpp>
#include <core/InputManager.hpp>
#include <core/GameStateManager.hpp>
#include "core/Engine.hpp"

#ifndef STANDALONE_MODE
#include <imgui.h>
#include <rlImGui.h>
#include "ImGuizmo.h"
#endif
#include <core/AssetRegistry.hpp>
#include <graphics/light/LightingSystem.hpp>

Editor::Editor(int windowWidth, int windowHeight) {
    // Initialize Console Redirects
    m_coutRedirect = std::make_unique<ConsoleSink>(std::cout, LogLevel::Info, m_console);
    m_cerrRedirect = std::make_unique<ConsoleSink>(std::cerr, LogLevel::Error, m_console);

    // Initialize ImGui
    rlImGuiSetup(true); 
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    m_imguiIniPath = (std::filesystem::current_path() / "imgui.ini").string();
    ImGui::GetIO().IniFilename = m_imguiIniPath.c_str();
        
    // Load Fonts
    ImGuiIO& io = ImGui::GetIO();
    float baseFontSize = 28.0f;
    ImFont* font = io.Fonts->AddFontFromFileTTF("assets/fonts/Roboto-Regular.ttf", baseFontSize);
    io.FontDefault = font;

    float iconFontSize = baseFontSize * 0.8f;
    ImFontConfig config;
    config.MergeMode = true;
    config.PixelSnapH = true;
    config.GlyphMinAdvanceX = iconFontSize;
    static const ImWchar icon_ranges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };
    io.Fonts->AddFontFromFileTTF("assets/fonts/fa-solid-900.ttf", iconFontSize, &config, icon_ranges);

    ApplyModernTheme();

    // Create Editor Resources
    m_sceneTexture = LoadRenderTexture(windowWidth, windowHeight);
    m_editorCamera = std::make_unique<EditorCamera>((float)windowWidth, (float)windowHeight);
}

Editor::~Editor() {
    rlImGuiShutdown();
    UnloadRenderTexture(m_sceneTexture);
}

void Editor::Draw(Scene& activeScene, RenderTexture2D& gameTexture, bool& isRunning, std::string& currentScenePath, nlohmann::json& sceneBackup) {
    // Hub interception
    // If no project is loaded, only draw the Hub. Stop the rest of the editor rendering.
    if (!m_isProjectLoaded) {
        rlImGuiBegin();
            if (m_projectHub.Draw()) {
                OnProjectLoaded();
            }
        rlImGuiEnd();
        return;
    }
    
    // Pass 1: Render the Scene View (what the editor sees)
    BeginTextureMode(m_sceneTexture);
        ClearBackground(Color{ 40, 40, 40, 255 });

        if (activeScene.Has3DCamera()) {
            // Update lighing parameters right before rendering the 3D scene
            LightingSystem::Update(activeScene, m_editorCamera->GetCamera3D());

            m_editorCamera->Begin3D();
                if (m_showGrid3D) m_editorCamera->DrawGrid3D(100, 1.0f);
                activeScene.Render3D();
            m_editorCamera->End3D();
        }
        
        m_editorCamera->Begin2D();
            if (m_showGrid2D) m_editorCamera->DrawGrid2D(100, 50.0f);

            activeScene.Render2D();    
            PhysicsEngine::RenderDebug(activeScene);

            if (m_selectedObject) {
                if (auto tilemap = m_selectedObject->GetComponent<TileMap>()) {
                    tilemap->RenderGrid();
                }
            }
        m_editorCamera->End2D();
        activeScene.RenderHUD();
    EndTextureMode();

    // Pass 2: Render the ImGui Interface
    rlImGuiBegin();
        ImGuizmo::BeginFrame();
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport()); 

        m_mainMenuBar.Draw(activeScene, currentScenePath, isRunning, m_selectedObject, m_inputSettingsPanel, m_gameStatePanel, m_showGrid2D, m_showGrid3D);

        m_inputSettingsPanel.Draw();
        m_gameStatePanel.Draw();

        TileMap* activeTileMap = m_selectedObject ? m_selectedObject->GetComponent<TileMap>() : nullptr;
        m_tilePalettePanel.Draw(m_selectedTileID, m_selectedLayer, activeTileMap);

        m_toolbarPanel.Draw(activeScene, m_selectedObject, sceneBackup, m_currentViewMode);
        
        m_sceneViewPanel.Draw(m_sceneTexture, *m_editorCamera, activeScene, m_selectedObject, m_selectedTileID, m_selectedLayer, m_currentViewMode);
        m_gameViewPanel.Draw(gameTexture, m_currentViewMode);
        m_codeEditorPanel.Draw(m_currentViewMode);

        // Keep track of the selected object before drawing the hierarchy
        GameObject* prevSelectedObject = m_selectedObject;

        if (m_prefabPanel.IsOpen()) {
            m_prefabPanel.Draw(m_selectedObject, m_currentViewMode, m_showGrid2D, m_showGrid3D);
        }

        if (m_currentViewMode == EditorViewMode::Prefab && m_prefabPanel.IsOpen()) {
            m_hierarchyPanel.Draw(m_prefabPanel.GetPrefabScene(), m_selectedObject);
        }
        else {
            m_hierarchyPanel.Draw(activeScene, m_selectedObject);
        }
        
        // UX LOGIC: If the user just clicked a new GameObject in the Hierarchy, 
        // we must clear the selected Asset to prioritize the Scene Object in the Inspector!
        if (m_selectedObject != prevSelectedObject && m_selectedObject != nullptr) {
            m_selectedAsset = pybind11::none();
        }

        m_console.Draw("Console");
        m_inspectorPanel.Draw(m_selectedObject, m_selectedAsset, m_selectedAssetPath);
        m_projectPanel.Draw(activeScene, m_selectedObject, m_selectedAsset, m_selectedAssetPath, m_assetsPath, currentScenePath, m_codeEditorPanel, m_currentViewMode, m_prefabPanel);
        m_performancePanel.Draw(activeScene);

    rlImGuiEnd();
}

void Editor::OnProjectLoaded() {
    m_isProjectLoaded = true;

    // Update the Raylib window title to match the project name
    std::string title = ProjectSettings::GetProjectName() + " - Foxvoid Engine";
    SetWindowTitle(title.c_str());

    // Apply the project's resolution to the Engine
    if (Engine::Get()) {
        Engine::Get()->UpdateResolution(
            ProjectSettings::GetWindowWidth(), 
            ProjectSettings::GetWindowHeight()
        );
    }

    // Pass the correct assets path to the ProjectPanel
    m_assetsPath = ProjectSettings::GetAssetsPath();

    // Initialize the global Asset Registry to map all UUIDs to their file paths
    AssetRegistry::Initialize(m_assetsPath);

    // Change the OS Current Working Directory
    // This forces Raylib to look for "assets/..." inside the project folder,
    // rather than the engine's build folder.
    std::filesystem::current_path(ProjectSettings::GetProjectRoot());
    std::cout << "[Editor] Working directory changed to: " << std::filesystem::current_path().string() << std::endl;

    // Register the project's script folder in Python
    // This allows Pybind11 to find and import the user's Python components
    ScriptEngine::AddScriptPath(ProjectSettings::GetAssetsPath() / "scripts");

    // Load Project-Specific Settings
    // We construct the absolute paths using our ProjectSettings manager
    std::filesystem::path settingsPath = ProjectSettings::GetAssetsPath() / "settings";

    InputManager::Load((settingsPath / "inputs.json").string());
    GameStateManager::Load((settingsPath / "globals.json").string());

    std::cout << "[Editor] Project settings loaded." << std::endl;
}

void Editor::ApplyModernTheme() {
    // Get the global ImGui style instance
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // =========================================================
    // 1. GEOMETRY (Rounding & Borders)
    // =========================================================
    // Soften all corners to remove the blocky 90s look
    style.WindowRounding    = 6.0f;
    style.ChildRounding     = 4.0f;
    style.FrameRounding     = 4.0f;
    style.PopupRounding     = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding      = 3.0f;
    style.TabRounding       = 4.0f;

    // Remove harsh borders to make the UI look flat and modern
    style.WindowBorderSize  = 0.0f;
    style.FrameBorderSize   = 0.0f;
    style.PopupBorderSize   = 1.0f; // Keep a small border for popups so they don't blend in

    // =========================================================
    // 2. COLORS (Sleek Dark Theme with Orange Accent)
    // =========================================================
    // Backgrounds (Dark gradients)
    colors[ImGuiCol_WindowBg]           = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
    colors[ImGuiCol_ChildBg]            = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
    colors[ImGuiCol_PopupBg]            = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
    
    // Inputs and Frames (Buttons, Text boxes)
    colors[ImGuiCol_FrameBg]            = ImVec4(0.20f, 0.20f, 0.20f, 1.0f);
    colors[ImGuiCol_FrameBgHovered]     = ImVec4(0.30f, 0.30f, 0.30f, 1.0f);
    colors[ImGuiCol_FrameBgActive]      = ImVec4(0.40f, 0.40f, 0.40f, 1.0f);
    
    // Window Headers (Title bars)
    colors[ImGuiCol_TitleBg]            = ImVec4(0.09f, 0.09f, 0.09f, 1.0f);
    colors[ImGuiCol_TitleBgActive]      = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
    
    // Tabs (Docking space)
    colors[ImGuiCol_Tab]                = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
    colors[ImGuiCol_TabHovered]         = ImVec4(0.30f, 0.30f, 0.30f, 1.0f);
    colors[ImGuiCol_TabActive]          = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
    colors[ImGuiCol_TabUnfocused]       = ImVec4(0.10f, 0.10f, 0.10f, 1.0f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
    
    // Standard Buttons
    colors[ImGuiCol_Button]             = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
    colors[ImGuiCol_ButtonHovered]      = ImVec4(0.35f, 0.35f, 0.35f, 1.0f);
    colors[ImGuiCol_ButtonActive]       = ImVec4(0.45f, 0.45f, 0.45f, 1.0f);
    
    // --- ACCENT COLOR (Foxvoid Orange) ---
    // This color is used for Checkboxes, Sliders, and active elements
    ImVec4 accentColor          = ImVec4(0.90f, 0.45f, 0.10f, 1.0f); 
    ImVec4 accentHovered        = ImVec4(1.00f, 0.55f, 0.20f, 1.0f);
    
    colors[ImGuiCol_CheckMark]          = accentColor;
    colors[ImGuiCol_SliderGrab]         = accentColor;
    colors[ImGuiCol_SliderGrabActive]   = accentHovered;
    colors[ImGuiCol_Header]             = ImVec4(0.30f, 0.30f, 0.30f, 1.0f); // Used for CollapsingHeaders
    colors[ImGuiCol_HeaderHovered]      = ImVec4(0.40f, 0.40f, 0.40f, 1.0f);
    colors[ImGuiCol_HeaderActive]       = ImVec4(0.45f, 0.45f, 0.45f, 1.0f);
}
