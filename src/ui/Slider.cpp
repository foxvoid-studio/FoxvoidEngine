#include "Slider.hpp"
#include "ui/RectTransform.hpp"
#include "scene/GameObject.hpp"
#include "input/Mouse.hpp"

#ifndef STANDALONE_MODE
#include "editor/EditorUI.hpp"
#include "editor/commands/CommandHistory.hpp"
#include "editor/commands/ModifyComponentCommand.hpp"
#include <imgui.h>
#endif

Slider::Slider() 
    : value(0.5f), minValue(0.0f), maxValue(1.0f), 
      trackColor(DARKGRAY), handleColor(LIGHTGRAY), activeHandleColor(WHITE),
      m_isDragging(false) {}

void Slider::Update(float deltaTime) {
    if (!owner) return;
    
    RectTransform* rectTransform = owner->GetComponent<RectTransform>();
    if (!rectTransform) return; // Cannot interact without UI bounds

    Rectangle bounds = rectTransform->GetScreenRect();
    Vector2 mousePos = Mouse::GetPosition();

    // 1. Check if the user just clicked inside the slider bounds
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mousePos, bounds)) {
        m_isDragging = true;
    }

    // 2. If the user is currently dragging the slider
    if (m_isDragging) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            // Calculate the percentage of the mouse X position relative to the track width
            float percentage = (mousePos.x - bounds.x) / bounds.width;
            
            // Clamp strictly between 0.0 and 1.0 to prevent the handle from leaving the track
            if (percentage < 0.0f) percentage = 0.0f;
            if (percentage > 1.0f) percentage = 1.0f;

            // Calculate the new actual value
            float newValue = minValue + percentage * (maxValue - minValue);

            // If the value actually changed, we update and broadcast the event!
            if (value != newValue) {
                value = newValue;
                // Broadcast to Python scripts! (Continuous update while dragging)
                owner->OnGUIClick(owner->name);
            }
        } else {
            // The user released the mouse button
            m_isDragging = false;
        }
    }
}  

void Slider::RenderHUD() {
    if (!owner) return;

    RectTransform* rectTransform = owner->GetComponent<RectTransform>();
    if (!rectTransform) return;

    Rectangle bounds = rectTransform->GetScreenRect();

    // 1. Draw the Background Track (Thinner than the actual bounds, centered vertically)
    float trackHeight = bounds.height * 0.3f; // Track is 30% of the total height
    Rectangle trackRec = { 
        bounds.x, 
        bounds.y + (bounds.height / 2.0f) - (trackHeight / 2.0f), 
        bounds.width, 
        trackHeight 
    };
    DrawRectangleRec(trackRec, trackColor);

    // 2. Calculate Handle Position
    float percentage = (value - minValue) / (maxValue - minValue);
    
    // Make the handle square-ish (as wide as the bounding box is tall)
    float handleWidth = bounds.height * 0.6f; 
    
    float handleX = bounds.x + (bounds.width * percentage) - (handleWidth / 2.0f);

    // Keep the handle visually inside the track limits
    if (handleX < bounds.x) handleX = bounds.x;
    if (handleX > bounds.x + bounds.width - handleWidth) handleX = bounds.x + bounds.width - handleWidth;

    Rectangle handleRec = { 
        handleX, 
        bounds.y + (bounds.height / 2.0f) - (bounds.height * 0.8f / 2.0f), 
        handleWidth, 
        bounds.height * 0.8f 
    };

    // 3. Draw the Handle
    Color currentHandleColor = m_isDragging ? activeHandleColor : handleColor;
    DrawRectangleRec(handleRec, currentHandleColor);
}

std::string Slider::GetName() const {
    return "Slider";
}

#ifndef STANDALONE_MODE
void Slider::OnInspector() {
    static nlohmann::json initialState;
    auto HandleUndoRedo = [&]() {
        if (ImGui::IsItemActivated()) initialState = Serialize();
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, initialState, Serialize()));
        }
    };

    ImGui::TextDisabled("Hold and drag to change value.");
    ImGui::Separator();

    ImGui::DragFloat("Value", &value, 0.01f, minValue, maxValue);
    HandleUndoRedo();
    
    ImGui::DragFloat("Min Value", &minValue, 0.1f);
    HandleUndoRedo();
    
    ImGui::DragFloat("Max Value", &maxValue, 0.1f);
    HandleUndoRedo();

    ImGui::Separator();

    EditorUI::ColorEdit4("Track Color", &trackColor, this);
    EditorUI::ColorEdit4("Handle Color", &handleColor, this);
    EditorUI::ColorEdit4("Active Handle Color", &activeHandleColor, this);
}
#endif

nlohmann::json Slider::Serialize() const {
    return {
        {"type", "Slider"},
        {"value", value},
        {"minValue", minValue},
        {"maxValue", maxValue},
        {"tr_r", trackColor.r}, {"tr_g", trackColor.g}, {"tr_b", trackColor.b}, {"tr_a", trackColor.a},
        {"hd_r", handleColor.r}, {"hd_g", handleColor.g}, {"hd_b", handleColor.b}, {"hd_a", handleColor.a},
        {"ah_r", activeHandleColor.r}, {"ah_g", activeHandleColor.g}, {"ah_b", activeHandleColor.b}, {"ah_a", activeHandleColor.a}
    };
}

void Slider::Deserialize(const nlohmann::json& j) {
    value = j.value("value", 0.5f);
    minValue = j.value("minValue", 0.0f);
    maxValue = j.value("maxValue", 1.0f);

    trackColor = { static_cast<unsigned char>(j.value("tr_r", 80)), static_cast<unsigned char>(j.value("tr_g", 80)), static_cast<unsigned char>(j.value("tr_b", 80)), static_cast<unsigned char>(j.value("tr_a", 255)) };
    handleColor = { static_cast<unsigned char>(j.value("hd_r", 200)), static_cast<unsigned char>(j.value("hd_g", 200)), static_cast<unsigned char>(j.value("hd_b", 200)), static_cast<unsigned char>(j.value("hd_a", 255)) };
    activeHandleColor = { static_cast<unsigned char>(j.value("ah_r", 255)), static_cast<unsigned char>(j.value("ah_g", 255)), static_cast<unsigned char>(j.value("ah_b", 255)), static_cast<unsigned char>(j.value("ah_a", 255)) };
}
