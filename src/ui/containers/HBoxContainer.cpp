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
    // At runtime, we rebuild the layout every frame to dynamically adapt to any UI changes
    RebuildLayout();
}

void HBoxContainer::RebuildLayout() {
    if (!owner) return;

    // Retrieve our own RectTransform component to handle automatic resizing of the container
    RectTransform* myRect = owner->GetComponent<RectTransform>();

    // Start positioning children after the left padding margin
    float currentX = paddingLeft;
    float maxHeight = 0.0f;

    // Fetch all immediate child GameObjects belonging to this container
    const auto& children = owner->GetChildren(); 
    
    for (GameObject* child : children) {
        // Skip inactive children to prevent leaving empty spaces or holes in the visual layout
        if (!child->IsActiveInHierarchy()) continue;

        if (RectTransform* childRect = child->GetComponent<RectTransform>()) {
            // Force the child's anchor and pivot to the center-left or specified edge alignment
            // The Y-axis anchor/pivot is strictly bound to the verticalAlignment configuration
            childRect->anchor = { 0.0f, verticalAlignment };
            childRect->pivot = { 0.0f, verticalAlignment };
            
            // Assign the definitive local coordinates computed by the layout system
            childRect->position.x = currentX; 
            childRect->position.y = 0.0f; 
            
            // Push the cursor forward for the next child element, accounting for size and gap spacing
            currentX += childRect->size.x + spacing;

            // Track the maximum height among all elements to adjust the parent container later
            if (childRect->size.y > maxHeight) {
                maxHeight = childRect->size.y;
            }
        }
    }

    // Strip the trailing spacing added by the last element, then add the right padding margin
    if (!children.empty()) {
        currentX -= spacing;
    }
    currentX += paddingRight;

    // AUTO-SIZING FEATURE: Force the parent container to perfectly bound and wrap its calculated content
    if (myRect) {
        myRect->size.x = currentX;
        if (maxHeight > 0.0f) {
            myRect->size.y = maxHeight;
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

        if (ImGui::IsItemActive() || ImGui::IsItemDeactivatedAfterEdit()) {
            RebuildLayout();
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

    ImGui::Separator();
    if (ImGui::Button("Force Rebuild Layout", ImVec2(-1, 0))) {
        RebuildLayout();
    }
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