#include "HBoxContainer.hpp"
#include "ui/core/RectTransform.hpp"
#include "scene/GameObject.hpp"

#ifndef STANDALONE_MODE
#include "editor/EditorUI.hpp"
#include "editor/commands/CommandHistory.hpp"
#include "editor/commands/ModifyComponentCommand.hpp"
#endif

HBoxContainer::HBoxContainer() 
    : spacing(10.0f), paddingLeft(10.0f), paddingRight(10.0f), verticalAlignment(0.5f) {}

void HBoxContainer::Update(float deltaTime) {
    if (!owner) return;

    // Start placing children after the left padding margin
    float currentX = paddingLeft;

    // Retrieve all child GameObjects directly from the owner's hierarchy
    const auto& children = owner->GetChildren(); 
    
    for (GameObject* child : children) {
        // Only attempt to lay out children that possess a UI spatial component
        if (RectTransform* childRect = child->GetComponent<RectTransform>()) {
            
            // 1. Force the anchor to the left (0.0f on X).
            // The Y axis is driven by the verticalAlignment variable.
            childRect->anchor = { 0.0f, verticalAlignment };
            
            // 2. Force the pivot to match the anchor so the position offset 
            // works predictably (e.g., from center-left to center-left).
            childRect->pivot = { 0.0f, verticalAlignment };
            
            // 3. Set the definitive local coordinates.
            // Y is 0 because the alignment is entirely handled by the anchor/pivot.
            childRect->position.x = currentX; 
            childRect->position.y = 0.0f; 
            
            // 4. Advance the cursor for the next iteration, accounting for
            // the current child's width plus the requested spacing between items.
            currentX += childRect->size.x + spacing;
        }
    }
}

std::string HBoxContainer::GetName() const {
    return "HBox Container";
}

#ifndef STANDALONE_MODE
void HBoxContainer::OnInspector() {
    static nlohmann::json initialState;

    // Helper lambda to track changes for Undo/Redo commands
    auto HandleUndoRedo = [&]() {
        if (ImGui::IsItemActivated()) {
            initialState = Serialize();
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, initialState, Serialize()));
        }
    };

    ImGui::TextDisabled("Auto-aligns child RectTransforms horizontally");
    ImGui::Separator();

    ImGui::DragFloat("Spacing", &spacing, 1.0f, 0.0f, 200.0f);
    HandleUndoRedo();
    
    ImGui::DragFloat("Padding Left", &paddingLeft, 1.0f, 0.0f, 200.0f);
    HandleUndoRedo();

    ImGui::DragFloat("Padding Right", &paddingRight, 1.0f, 0.0f, 200.0f);
    HandleUndoRedo();

    ImGui::DragFloat("Vertical Align", &verticalAlignment, 0.05f, 0.0f, 1.0f);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("0.0 = Top, 0.5 = Center, 1.0 = Bottom");
    }
    HandleUndoRedo();
}
#endif

nlohmann::json HBoxContainer::Serialize() const {
    return {
        {"type", "HBoxContainer"},
        {"spacing", spacing},
        {"paddingLeft", paddingLeft},
        {"paddingRight", paddingRight},
        {"verticalAlignment", verticalAlignment}
    };
}

void HBoxContainer::Deserialize(const nlohmann::json& j) {
    spacing = j.value("spacing", 10.0f);
    paddingLeft = j.value("paddingLeft", 10.0f);
    paddingRight = j.value("paddingRight", 10.0f);
    verticalAlignment = j.value("verticalAlignment", 0.5f);
}
