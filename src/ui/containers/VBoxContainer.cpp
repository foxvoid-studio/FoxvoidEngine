#include "VBoxContainer.hpp"
#include "ui/core/RectTransform.hpp"
#include "scene/GameObject.hpp"

#ifndef STANDALONE_MODE
#include "editor/EditorUI.hpp"
#include "editor/commands/CommandHistory.hpp"
#include "editor/commands/ModifyComponentCommand.hpp"
#endif

VBoxContainer::VBoxContainer() 
    : spacing(10.0f), paddingTop(10.0f), paddingBottom(10.0f), horizontalAlignment(0.5f) {}

void VBoxContainer::Update(float deltaTime) {
    if (!owner) return;

    // Start placing children below the top padding margin
    float currentY = paddingTop;

    // Retrieve all child GameObjects directly from the owner's hierarchy
    const auto& children = owner->GetChildren(); 
    
    for (GameObject* child : children) {
        // Only attempt to lay out children that possess a UI spatial component
        if (RectTransform* childRect = child->GetComponent<RectTransform>()) {
            
            // 1. Force the anchor to the top (0.0f on Y).
            // The X axis is driven by the horizontalAlignment variable.
            childRect->anchor = { horizontalAlignment, 0.0f };
            
            // 2. Force the pivot to match the anchor so the position offset 
            // works predictably (e.g., from top-center to top-center).
            childRect->pivot = { horizontalAlignment, 0.0f };
            
            // 3. Set the definitive local coordinates.
            // X is 0 because the alignment is entirely handled by the anchor/pivot.
            childRect->position.x = 0.0f; 
            childRect->position.y = currentY; 
            
            // 4. Advance the cursor for the next iteration, accounting for
            // the current child's height plus the requested spacing between items.
            currentY += childRect->size.y + spacing;
        }
    }
}

std::string VBoxContainer::GetName() const {
    return "VBox Container";
}

#ifndef STANDALONE_MODE
void VBoxContainer::OnInspector() {
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

    ImGui::TextDisabled("Auto-aligns child RectTransforms vertically");
    ImGui::Separator();

    ImGui::DragFloat("Spacing", &spacing, 1.0f, 0.0f, 200.0f);
    HandleUndoRedo();
    
    ImGui::DragFloat("Padding Top", &paddingTop, 1.0f, 0.0f, 200.0f);
    HandleUndoRedo();

    ImGui::DragFloat("Padding Bottom", &paddingBottom, 1.0f, 0.0f, 200.0f);
    HandleUndoRedo();

    ImGui::DragFloat("Horizontal Align", &horizontalAlignment, 0.05f, 0.0f, 1.0f);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("0.0 = Left, 0.5 = Center, 1.0 = Right");
    }
    HandleUndoRedo();
}
#endif

nlohmann::json VBoxContainer::Serialize() const {
    return {
        {"type", "VBoxContainer"},
        {"spacing", spacing},
        {"paddingTop", paddingTop},
        {"paddingBottom", paddingBottom},
        {"horizontalAlignment", horizontalAlignment}
    };
}

void VBoxContainer::Deserialize(const nlohmann::json& j) {
    spacing = j.value("spacing", 10.0f);
    paddingTop = j.value("paddingTop", 10.0f);
    paddingBottom = j.value("paddingBottom", 10.0f);
    horizontalAlignment = j.value("horizontalAlignment", 0.5f);
}
