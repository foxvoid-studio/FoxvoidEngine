#include "Camera3d.hpp"
#include "physics/Transform3d.hpp" // Included here to access GetGlobalMatrix() and GetGlobalQuaternion()
#include <raymath.h>

#ifndef STANDALONE_MODE
#include <imgui.h>
#include "editor/commands/CommandHistory.hpp"
#include "editor/commands/ModifyComponentCommand.hpp"
#include "editor/EditorUI.hpp"
#endif

Camera3D Camera3d::GetRaylibCamera() const {
    Camera3D cam = { 0 };
    cam.fovy = fov;
    cam.projection = projection;

    if (owner) {
        auto transform = owner->GetComponent<Transform3d>();
        if (transform) {
            cam.position = transform->GetGlobalPosition();

            // 1. Calculate Target (Where we are looking)
            // We take a point exactly 1 unit in front of the camera (Local -Z)
            // and multiply it by the Global Matrix to get the absolute world coordinate.
            cam.target = Vector3Transform({ 0.0f, 0.0f, -1.0f }, transform->GetGlobalMatrix());

            // 2. Calculate Up Vector (Camera roll)
            // We rotate the default Y-Up vector by the camera's global quaternion
            // to prevent Gimbal Lock and allow smooth 3D rotations.
            cam.up = Vector3RotateByQuaternion({ 0.0f, 1.0f, 0.0f }, transform->GetGlobalQuaternion());
            
            return cam;
        }
    }

    // Fallback defaults if no Transform3d is attached to the owner
    cam.position = { 0.0f, 0.0f, 0.0f };
    cam.target = { 0.0f, 0.0f, -1.0f };
    cam.up = { 0.0f, 1.0f, 0.0f };
    return cam;
}

std::string Camera3d::GetName() const {
    return "Camera 3D";
}

#ifndef STANDALONE_MODE
void Camera3d::OnInspector() {
    bool tempIsMain = isMain;
    if (ImGui::Checkbox("Main Camera", &tempIsMain)) {
        nlohmann::json initialState = Serialize();
        isMain = tempIsMain;
        CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, initialState, Serialize()));
    }

    float tempFov = fov;
    // Assuming EditorUI::DragFloat handles the UI logic and returns true on edit
    if (EditorUI::DragFloat("FOV", &tempFov, 1.0f, this, 1.0f, 179.0f)) {
        fov = tempFov;
    }

    ImGui::Separator();

    // Dropdown for the Raylib projection type
    const char* projNames[] = { "Perspective", "Orthographic" };
    if (ImGui::BeginCombo("Projection", projNames[projection])) {
        for (int i = 0; i < 2; i++) {
            bool isSelected = (projection == i);
            if (ImGui::Selectable(projNames[i], isSelected)) {
                nlohmann::json initialState = Serialize();
                projection = i;
                CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, initialState, Serialize()));
            }
            // Ensure the active item is focused when opening the combo box
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
}
#endif

nlohmann::json Camera3d::Serialize() const {
    return {
        { "type", "Camera3d" },
        { "isMain", isMain },
        { "fov", fov },
        { "projection", projection }
    };
}

void Camera3d::Deserialize(const nlohmann::json& j) {
    isMain = j.value("isMain", true);
    fov = j.value("fov", 60.0f);
    projection = j.value("projection", CAMERA_PERSPECTIVE);
}
