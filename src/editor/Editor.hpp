#pragma once

#include <string>
#include <filesystem>
#include <memory>
#include <raylib.h>
#include <pybind11/pybind11.h>

#include "scene/Scene.hpp"
#include "scene/GameObject.hpp"

#include "editor/EditorViewMode.hpp"

// Editor Panels
#include "editor/EditorConsole.hpp"
#include "editor/MainMenuBar.hpp"
#include "editor/panels/HierarchyPanel.hpp"
#include "editor/panels/InspectorPanel.hpp"
#include "editor/panels/ProjectPanel.hpp"
#include "editor/panels/SceneViewPanel.hpp"
#include "editor/panels/GameViewPanel.hpp"
#include "editor/panels/ToolbarPanel.hpp"
#include "editor/panels/InputSettingsPanel.hpp"
#include "editor/panels/TilePalettePanel.hpp"
#include "editor/panels/GameStatePanel.hpp"
#include "editor/panels/ProjectHubPanel.hpp"
#include "editor/panels/PerformancePanel.hpp"
#include "editor/panels/CodeEditorPanel.hpp"
#include "editor/panels/PrefabEditorPanel.hpp"

class [[gnu::visibility("default")]] Editor {
    public:
        Editor(int windowWidth, int windowHeight);
        ~Editor();

        // The main function that orchestrates all editor rendering
        void Draw(Scene& activeScene, RenderTexture2D& gameTexture, bool& isRunning, std::string& currentScenePath, nlohmann::json& sceneBackup);

    private:
        void ApplyModernTheme();

        bool m_isProjectLoaded = false;
        ProjectHubPanel m_projectHub;

        void OnProjectLoaded();

        // Editor State
        GameObject* m_selectedObject = nullptr;

        // Track the currently selected ScriptableObject
        pybind11::object m_selectedAsset = pybind11::none();
        
        // Track where it's saved
        std::string m_selectedAssetPath = ""; 

        std::string m_imguiIniPath;

        EditorViewMode m_currentViewMode = EditorViewMode::Scene;

        bool m_showGlobalGrid = true;
        
        // TileMap Painting State
        int m_selectedTileID = -1;
        int m_selectedLayer = 0;
        TileTool m_currentTileTool = TileTool::Brush; // Tracks the active painting tool
        
        std::filesystem::path m_assetsPath = "assets";

        // Editor Specific Rendering
        RenderTexture2D m_sceneTexture; // Only the editor needs to render the scene with gizmos
        std::unique_ptr<EditorCamera> m_editorCamera;

        // Console Redirection
        std::unique_ptr<ConsoleSink> m_coutRedirect;
        std::unique_ptr<ConsoleSink> m_cerrRedirect;

        // Panels
        EditorConsole m_console;
        ToolbarPanel m_toolbarPanel;
        SceneViewPanel m_sceneViewPanel;
        GameViewPanel m_gameViewPanel;
        HierarchyPanel m_hierarchyPanel;
        InspectorPanel m_inspectorPanel;
        ProjectPanel m_projectPanel;
        InputSettingsPanel m_inputSettingsPanel;
        GameStatePanel m_gameStatePanel;
        MainMenuBar m_mainMenuBar;
        TilePalettePanel m_tilePalettePanel;
        PerformancePanel m_performancePanel;
        CodeEditorPanel m_codeEditorPanel;
        PrefabEditorPanel m_prefabPanel;
};
