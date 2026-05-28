#pragma once

#include <raylib.h>
#include "EditorCamera.hpp"
#include <world/Scene.hpp>

#ifndef STANDALONE_MODE
#include <imgui.h>
#endif
#include "EditorViewMode.hpp"
#include "editor/TilePalettePanel.hpp" // Includes TileTool enum

// Panel to display the rendered game texture
class SceneViewPanel {
    public:
        void Draw(RenderTexture2D& sceneTexture, EditorCamera& camera, Scene& activeScene, GameObject*& selectedObject, int selectedTileID, int selectedLayer, TileTool currentTileTool, EditorViewMode& currentViewMode);
};
