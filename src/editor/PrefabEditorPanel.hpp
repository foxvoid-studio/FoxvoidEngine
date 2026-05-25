#pragma once

#include <raylib.h>
#include <string>
#include "world/Scene.hpp"
#include "world/GameObject.hpp"
#include "editor/EditorCamera.hpp"
#include "editor/EditorViewMode.hpp"

class PrefabEditorPanel {
    public:
        PrefabEditorPanel();
        ~PrefabEditorPanel();

        // Main draw loop for the ImGui window
        void Draw(GameObject*& selectedObject, EditorViewMode& currentViewMode);

        // Loads a .prefab file into the isolated scene
        void OpenPrefab(const std::string& filepath);

        // Returns true if the user is currently editing a prefab
        bool IsOpen() const { return m_isOpen; }

        // Exposes the isolated scene to the HierarchyPanel
        Scene& GetPrefabScene() { return m_prefabScene; }

    private:
        void SavePrefab();
        void ClosePrefab();

        bool m_isOpen;
        std::string m_currentFilepath;
        
        // The completely isolated environment!
        Scene m_prefabScene;

        RenderTexture2D m_renderTexture;
        EditorCamera m_camera;
};
