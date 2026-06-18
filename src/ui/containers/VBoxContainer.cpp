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
    // At runtime, we rebuild the layout every frame to dynamically adapt to any UI changes
    RebuildLayout();
}

void VBoxContainer::RebuildLayout() {
    if (!owner) return;

    // Retrieve our own RectTransform component to handle automatic resizing of the container
    RectTransform* myRect = owner->GetComponent<RectTransform>();

    // Start positioning children after the top padding margin
    float currentY = paddingTop;
    float maxWidth = 0.0f;

    // Fetch all immediate child GameObjects belonging to this container
    const auto& children = owner->GetChildren(); 
    
    for (GameObject* child : children) {
        // Skip inactive children to prevent leaving empty spaces or holes in the visual layout
        if (!child->IsActiveInHierarchy()) continue;

        if (RectTransform* childRect = child->GetComponent<RectTransform>()) {
            
            // Force the child's anchor and pivot to the top edge alignment
            // The X-axis anchor/pivot is strictly bound to the horizontalAlignment configuration
            childRect->anchor = { horizontalAlignment, 0.0f };
            childRect->pivot = { horizontalAlignment, 0.0f };
            
            // Assign the definitive local coordinates computed by the layout system
            childRect->position.x = 0.0f; 
            childRect->position.y = currentY; 
            
            // Push the cursor downward for the next child element, accounting for size and gap spacing
            currentY += childRect->size.y + spacing;

            // Track the maximum width among all elements to adjust the parent container later
            if (childRect->size.x > maxWidth) {
                maxWidth = childRect->size.x;
            }
        }
    }

    // Strip the trailing spacing added by the last element, then add the bottom padding margin
    if (!children.empty()) {
        currentY -= spacing;
    }
    currentY += paddingBottom;

    // AUTO-SIZING FEATURE: Force the parent container to perfectly bound and wrap its calculated content
    if (myRect) {
        myRect->size.y = currentY;
        if (maxWidth > 0.0f) {
            myRect->size.x = maxWidth;
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
        
        // Rebuild the layout in real-time while dragging sliders in the editor
        if (ImGui::IsItemActive() || ImGui::IsItemDeactivatedAfterEdit()) {
            RebuildLayout();
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

    ImGui::Separator();

    // Manual override button for the editor
    if (ImGui::Button("Force Rebuild Layout", ImVec2(-1, 0))) {
        RebuildLayout();
    }
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
