#include "EditorUI.hpp"
#include "editor/commands/CommandHistory.hpp"
#include "editor/commands/ModifyComponentCommand.hpp"

bool EditorUI::DragFloat2(const char* label, float v[2], float v_speed, Component* component, float v_min, float v_max) {
    // Static variable to hold the JSON state before the user starts dragging
    static nlohmann::json initialState;
    bool valueChanged = false;

    // Draw the actual ImGui widget
    if (ImGui::DragFloat2(label, v, v_speed, v_min, v_max)) {
        valueChanged = true;
    }

    // Track the interaction lifecycle for Undo/Redo
    if (ImGui::IsItemActivated()) {
        // The user just clicked the slider. Save the component's state
        initialState = component->Serialize();
    }

    if (ImGui::IsItemDeactivatedAfterEdit()) {
        // The user released the mouse. Push the command to the history
        CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(component, initialState, component->Serialize()));
    }

    return valueChanged;
}

bool EditorUI::DragFloat3(const char* label, float v[3], float v_speed, Component* component, float v_min, float v_max) {
    static nlohmann::json initialState;
    bool valueChanged = false;

    if (ImGui::DragFloat3(label, v, v_speed, v_min, v_max)) {
        valueChanged = true;
    }

    if (ImGui::IsItemActivated()) {
        initialState = component->Serialize();
    }

    if (ImGui::IsItemDeactivatedAfterEdit()) {
        CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(component, initialState, component->Serialize()));
    }

    return valueChanged;
}

bool EditorUI::DragFloat4(const char* label, float v[4], float v_speed, Component* component, float v_min, float v_max) {
    static nlohmann::json initialState;
    bool valueChanged = false;

    if (ImGui::DragFloat4(label, v, v_speed, v_min, v_max)) {
        valueChanged = true;
    }

    if (ImGui::IsItemActivated()) {
        initialState = component->Serialize();
    }
    
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(component, initialState, component->Serialize()));
    }

    return valueChanged;
}

bool EditorUI::DragFloat(const char* label, float* v, float v_speed, Component* component, float v_min, float v_max) {
    static nlohmann::json initialState;
    bool valueChanged = false;

    if (ImGui::DragFloat(label, v, v_speed, v_min, v_max)) {
        valueChanged = true;
    }

    if (ImGui::IsItemActivated()) {
        initialState = component->Serialize();
    }
    
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(component, initialState, component->Serialize()));
    }

    return valueChanged;
}

bool EditorUI::Checkbox(const char* label, bool* v, Component* component) {
    // For a checkbox, we grab the state BEFORE ImGui evaluates the click
    nlohmann::json initialState = component->Serialize();
    
    if (ImGui::Checkbox(label, v)) {
        // If ImGui::Checkbox returns true, the value of 'v' was just inverted!
        // We can immediately push the command to the history.
        CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(component, initialState, component->Serialize()));
        return true;
    }
    return false;
}

bool EditorUI::ColorEdit4(const char* label, Color* color, Component* component) {
    // Convert Raylib color (0-255) to ImGui color (0.0f-1.0f)
    float c[4] = { 
        color->r / 255.0f, 
        color->g / 255.0f, 
        color->b / 255.0f, 
        color->a / 255.0f 
    };

    bool valueChanged = false;
    static nlohmann::json initialState;

    // Draw the ImGui color picker
    if (ImGui::ColorEdit4(label, c)) {
        // If modified, convert back to Raylib format immediately
        color->r = static_cast<unsigned char>(c[0] * 255.0f);
        color->g = static_cast<unsigned char>(c[1] * 255.0f);
        color->b = static_cast<unsigned char>(c[2] * 255.0f);
        color->a = static_cast<unsigned char>(c[3] * 255.0f);
        valueChanged = true;
    }

    // Track Undo/Redo lifecycle
    if (ImGui::IsItemActivated()) {
        initialState = component->Serialize();
    }
    
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(component, initialState, component->Serialize()));
    }

    return valueChanged;
}

bool EditorUI::DragInt(const char* label, int* v, float v_speed, Component* component, int v_min, int v_max) {
    static nlohmann::json initialState;
    bool valueChanged = false;

    if (ImGui::DragInt(label, v, v_speed, v_min, v_max)) {
        valueChanged = true;
    }

    if (ImGui::IsItemActivated()) {
        initialState = component->Serialize();
    }
    
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(component, initialState, component->Serialize()));
    }

    return valueChanged;
}
