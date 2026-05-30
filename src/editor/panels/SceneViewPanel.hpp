#pragma once

#include <raylib.h>
#include "editor/EditorCamera.hpp"
#include <scene/Scene.hpp>

#ifndef STANDALONE_MODE
#include <imgui.h>
#endif
#include "editor/EditorViewMode.hpp"
#include "editor/panels/TilePalettePanel.hpp" // Includes TileTool enum

// Panel to display the rendered game texture
class SceneViewPanel {
    public:
        void Draw(RenderTexture2D& sceneTexture, EditorCamera& camera, Scene& activeScene, GameObject*& selectedObject, int& selectedTileID, int& selectedLayer, TileTool& currentTileTool, EditorViewMode& currentViewMode);
};
