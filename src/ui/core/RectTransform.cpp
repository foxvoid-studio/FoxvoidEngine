#include "RectTransform.hpp"
#include <raymath.h>
#include "core/Engine.hpp"

#ifndef STANDALONE_MODE
#include <imgui.h>
#include "editor/commands/CommandHistory.hpp"
#include "editor/commands/ModifyComponentCommand.hpp"
#endif

RectTransform::RectTransform() 
    : size{100.0f, 40.0f}, 
      position{0.0f, 0.0f}, 
      anchor{0.5f, 0.5f},  // Default anchored to screen center
      pivot{0.5f, 0.5f}    // Default pivot at the center of the element
{
}

Rectangle RectTransform::GetScreenRect() const {
    // Default to the whole screen size and origin (0, 0)
    float parentX = 0.0f;
    float parentY = 0.0f;
    float parentWidth = 1280.0f; 
    float parentHeight = 720.0f;

    if (Engine::Get()) {
        parentWidth = (float)Engine::Get()->GetTargetWidth();
        parentHeight = (float)Engine::Get()->GetTargetHeight();
    }

    // If we have a parent, and that parent has a RectTransform,
    // our coordinates become relative to the parent's rectangle.
    if (owner && owner->GetParent()) {
        if (RectTransform* parentRect = owner->GetParent()->GetComponent<RectTransform>()) {
            // Recursive call. This climbs up the tree until it hits a root object
            Rectangle pRect = parentRect->GetScreenRect();

            parentX = pRect.x;
            parentY = pRect.y;
            parentWidth = pRect.width;
            parentHeight = pRect.height;
        }
    }

    // 1. Calculate the exact pixel coordinate of our Anchor relative to the Parent
    float anchorPixelX = parentX + (parentWidth * anchor.x);
    float anchorPixelY = parentY + (parentHeight * anchor.y);

    // 2. Add our local position offset
    float pivotPixelX = anchorPixelX + position.x;
    float pivotPixelY = anchorPixelY + position.y;

    // 3. Subtract the pivot offset to find the absolute Top-Left corner for Raylib
    float topLeftX = pivotPixelX - (size.x * pivot.x);
    float topLeftY = pivotPixelY - (size.y * pivot.y);

    return Rectangle{ topLeftX, topLeftY, size.x, size.y };
}

std::string RectTransform::GetName() const {
    return "Rect Transform";
}

#ifndef STANDALONE_MODE
void RectTransform::OnInspector() {
    static nlohmann::json initialState;

    auto HandleUndoRedo = [&]() {
        if (ImGui::IsItemActivated()) {
            initialState = Serialize();
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, initialState, Serialize()));
        }
    };

    ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "UI Spatial Data");
    ImGui::Spacing();

    ImGui::DragFloat2("Position", &position.x, 1.0f);
    HandleUndoRedo();

    ImGui::DragFloat2("Size", &size.x, 1.0f, 0.0f, 10000.0f);
    HandleUndoRedo();

    ImGui::Separator();

    ImGui::TextDisabled("Normalized Values (0.0 to 1.0)");
    
    ImGui::DragFloat2("Anchor", &anchor.x, 0.05f, 0.0f, 1.0f);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("0,0 is Top-Left of screen. 1,1 is Bottom-Right.");
    HandleUndoRedo();

    ImGui::DragFloat2("Pivot", &pivot.x, 0.05f, 0.0f, 1.0f);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("0,0 is Top-Left of the element. 0.5,0.5 is Center.");
    HandleUndoRedo();

    ImGui::Separator();

    // Debug: Show calculated values
    Rectangle screenRect = GetScreenRect();
    ImGui::TextDisabled("Screen Output:");
    ImGui::TextDisabled("X: %.1f | Y: %.1f", screenRect.x, screenRect.y);
    ImGui::TextDisabled("W: %.1f | H: %.1f", screenRect.width, screenRect.height);
}
#endif

nlohmann::json RectTransform::Serialize() const {
    return {
        { "type", "RectTransform" },
        { "size_x", size.x }, { "size_y", size.y },
        { "pos_x", position.x }, { "pos_y", position.y },
        { "anchor_x", anchor.x }, { "anchor_y", anchor.y },
        { "pivot_x", pivot.x }, { "pivot_y", pivot.y }
    };
}

void RectTransform::Deserialize(const nlohmann::json& j) {
    size.x = j.value("size_x", 100.0f);
    size.y = j.value("size_y", 40.0f);
    
    position.x = j.value("pos_x", 0.0f);
    position.y = j.value("pos_y", 0.0f);
    
    anchor.x = j.value("anchor_x", 0.5f);
    anchor.y = j.value("anchor_y", 0.5f);
    
    pivot.x = j.value("pivot_x", 0.5f);
    pivot.y = j.value("pivot_y", 0.5f);
}
