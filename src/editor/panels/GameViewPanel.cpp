#include "GameViewPanel.hpp"
#include "input/Mouse.hpp"
#include "editor/EditorViewMode.hpp"

void GameViewPanel::Draw(RenderTexture2D& gameTexture, EditorViewMode& currentViewMode) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    
    // The name of the window matters for ImGui docking
    ImGui::Begin("Game View");
    
    ImVec2 windowSize = ImGui::GetContentRegionAvail();
    if (windowSize.x > 0.0f && windowSize.y > 0.0f) {
        // Aspect Ratio Calculation
        float texWidth = (float)gameTexture.texture.width;
        float texHeight = (float)gameTexture.texture.height;
        float targetAspect = texWidth / texHeight;
        float windowAspect = windowSize.x / windowSize.y;

        ImVec2 drawSize;
        if (windowAspect > targetAspect) {
            drawSize.y = windowSize.y;
            drawSize.x = windowSize.y * targetAspect;
        } else {
            drawSize.x = windowSize.x;
            drawSize.y = windowSize.x / targetAspect;
        }

        // Calculate centered position
        ImVec2 cursorPos = ImGui::GetCursorPos();
        cursorPos.x += (windowSize.x - drawSize.x) * 0.5f;
        cursorPos.y += (windowSize.y - drawSize.y) * 0.5f;
        ImGui::SetCursorPos(cursorPos);

        // Virtual mouse calculation with aspect ratio
        // Get the global screen position where the image actually starts drawing (after centering)
        ImVec2 imageScreenPos = ImGui::GetCursorScreenPos();
        ImVec2 mousePos = ImGui::GetMousePos();

        // Calculate relative position of the mouse inside the rendered image area
        float relativeX = mousePos.x - imageScreenPos.x;
        float relativeY = mousePos.y - imageScreenPos.y;

        // Apply the scaling ratio based on the actual drawn size vs real texture size
        float scaleX = texWidth / drawSize.x;
        float scaleY = texHeight / drawSize.y;

        Vector2 gameMousePos = { relativeX * scaleX, relativeY * scaleY };
        
        // Inject it into the Engine only if the mouse is hovering the Game View window
        if (ImGui::IsWindowHovered()) {
            Mouse::SetVirtualPosition(gameMousePos);
        }

        Rectangle sourceRec = { 0.0f, 0.0f, texWidth, -texHeight };
        rlImGuiImageRect(&gameTexture.texture, (int)drawSize.x, (int)drawSize.y, sourceRec);
    }
    
    ImGui::End();
    ImGui::PopStyleVar();
}
