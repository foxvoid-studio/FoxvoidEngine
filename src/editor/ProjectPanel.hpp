#pragma once

#ifndef STANDALONE_MODE
#include <imgui.h>
#endif

#include <filesystem>
#include <iostream>
#include <unordered_map>
#include <pybind11/pybind11.h>
#include <raylib.h>
#include "world/Scene.hpp"
#include "world/GameObject.hpp"

#include "editor/EditorViewMode.hpp"

class CodeEditorPanel;
class PrefabEditorPanel;

namespace fs = std::filesystem;

// Define the types of scripts we can generate
enum class ScriptType {
    Component,
    ScriptableObject
};

class ProjectPanel {
    public:
        ProjectPanel() = default;
        ~ProjectPanel();
    
        void Draw(Scene& activeScene, GameObject*& selectedObject, pybind11::object& selectedAsset, std::string& selectedAssetPath, const fs::path& assetsPath, std::string& currentScenePath, CodeEditorPanel& codeEditorPanel, EditorViewMode& currentViewMode, PrefabEditorPanel& prefabEditorPanel);

    private:
        // Recursive function to read and display the folder tree
        void DrawDirectoryNode(Scene& activeScene, GameObject*& selectedObject, pybind11::object& selectedAsset, std::string& selectedAssetPath, const fs::path& path, std::string& currentScenePath, CodeEditorPanel& codeEditorPanel, EditorViewMode& currentViewMode, PrefabEditorPanel& prefabEditorPanel);

        // Helpers for the visual asset thumbnails
        Texture2D GetOrLoadThumbnail(const std::string& path);
        void ClearThumbnails();

        void HandleDragAndDropMove(const fs::path& targetDirectory);

        fs::path m_currentDirectory = "";

        // Flags to trigger the modals safely
        bool m_requestScriptModal = false;
        bool m_requestAssetModal = false;
        bool m_requestFolderModal = false;

        // Flags for Rename and Delete modals
        bool m_requestRenameModal = false;
        bool m_requestDeleteModal = false;

        // Track which type of script the modal should generate
        ScriptType m_pendingScriptType = ScriptType::Component;

        // Buffers to pre-fill the Asset Modal
        char m_assetPrefillModule[64] = "";
        char m_assetPrefillClass[64] = "";

        // Buffer for the search bar
        char m_searchBuffer[128] = "";

        // Shared state for renaming and deleting items
        fs::path m_actionTarget = ""; 
        char m_renameBuffer[128] = "";

        // The cache storing our resized thumbnails (Path -> Texture)
        std::unordered_map<std::string, Texture2D> m_thumbnailCache;

        // Image Preview variables
        bool m_showImagePreview = false;
        Texture2D m_previewTexture = {0};
        std::string m_previewPath = "";
};
