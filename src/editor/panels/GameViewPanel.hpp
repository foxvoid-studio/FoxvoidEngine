#pragma once

#include <raylib.h>
#include "editor/EditorViewMode.hpp"

#ifndef STANDALONE_MODE
#include <imgui.h>
#include <rlImGui.h>
#endif

// Panel to display the renderer game exactly as the player will see it
class GameViewPanel {
    public:
        void Draw(RenderTexture2D& gameTexture, EditorViewMode& currentViewMode);
};
