#include "Transform2d.hpp"

Vector2 Transform2d::GetGlobalPosition() const {
    // If there is no owner or no parent, the local position is the global position
    if (!owner || !owner->GetParent()) return position;

    auto parentTransform = owner->GetParent()->GetComponent<Transform2d>();
    if (!parentTransform) return position;

    // Get the parent's global state (Recursive call)
    Vector2 parentGlobalPos = parentTransform->GetGlobalPosition();
    float parentGlobalRot = parentTransform->GetGlobalRotation();
    Vector2 parentGlobalScale = parentTransform->GetGlobalScale();
        
    // Apply Scale
    Vector2 scaledLocal = { position.x * parentGlobalScale.x, position.y * parentGlobalScale.y };

    // Apply Rotation (Classic trigonometry)
    // Raylib uses degrees, but standard C++ math functions expect radians
    float rad = parentGlobalRot * (PI / 180.0f);
    float cosR = std::cos(rad);
    float sinR = std::sin(rad);

    Vector2 rotatedLocal = {
        scaledLocal.x * cosR - scaledLocal.y * sinR,
        scaledLocal.x * sinR + scaledLocal.y * cosR
    };

    // Apply Translation (position)
    return { parentGlobalPos.x + rotatedLocal.x, parentGlobalPos.y + rotatedLocal.y };
}

void Transform2d::SetGlobalPosition(Vector2 targetGlobalPos) {
    // If no parent, local = global
    if (!owner || !owner->GetParent()) {
        position = targetGlobalPos;
        return;
    }
            
    auto parentTransform = owner->GetParent()->GetComponent<Transform2d>();
    if (!parentTransform) {
        position = targetGlobalPos;
        return;
    }

    Vector2 parentGlobalPos = parentTransform->GetGlobalPosition();
    float parentGlobalRot = parentTransform->GetGlobalRotation();
    Vector2 parentGlobalScale = parentTransform->GetGlobalScale();

    // Reverse Translation (Subtract parent's position)
    Vector2 translatedBack = { targetGlobalPos.x - parentGlobalPos.x, targetGlobalPos.y - parentGlobalPos.y };

    // Reverse Rotation (Rotate by the negative parent rotation)
    float rad = -parentGlobalRot * (PI / 180.0f);
    float cosR = std::cos(rad);
    float sinR = std::sin(rad);

    Vector2 rotatedBack = {
        translatedBack.x * cosR - translatedBack.y * sinR,
        translatedBack.x * sinR + translatedBack.y * cosR
    };

    // Reverse Scale (Divide by parent scale, protecting against division by zero)
    float safeScaleX = (parentGlobalScale.x != 0.0f) ? parentGlobalScale.x : 1.0f;
    float safeScaleY = (parentGlobalScale.y != 0.0f) ? parentGlobalScale.y : 1.0f;

    position = { rotatedBack.x / safeScaleX, rotatedBack.y / safeScaleY };
}

float Transform2d::GetGlobalRotation() const {
    if (!owner || !owner->GetParent()) return rotation;
            
    auto parentTransform = owner->GetParent()->GetComponent<Transform2d>();
    if (!parentTransform) return rotation;

    // Rotations are simply additive in 2D
    return parentTransform->GetGlobalRotation() + rotation;
}

void Transform2d::SetGlobalRotation(float targetGlobalRot) {
    if (!owner || !owner->GetParent()) {
        rotation = targetGlobalRot;
        return;
    }
    auto parentTransform = owner->GetParent()->GetComponent<Transform2d>();
    if (!parentTransform) {
        rotation = targetGlobalRot;
        return;
    }

    // Reverse additive rotation
    rotation = targetGlobalRot - parentTransform->GetGlobalRotation();
}

Vector2 Transform2d::GetGlobalScale() const {
    if (!owner || !owner->GetParent()) return scale;
            
    auto parentTransform = owner->GetParent()->GetComponent<Transform2d>();
    if (!parentTransform) return scale;

    // Scales are multiplied
    Vector2 parentGlobalScale = parentTransform->GetGlobalScale();
    return { parentGlobalScale.x * scale.x, parentGlobalScale.y * scale.y };
}

void Transform2d::SetGlobalScale(Vector2 targetGlobalScale) {
    if (!owner || !owner->GetParent()) {
        scale = targetGlobalScale;
        return;
    }
    auto parentTransform = owner->GetParent()->GetComponent<Transform2d>();
    if (!parentTransform) {
        scale = targetGlobalScale;
        return;
    }

    // Reverse multiplicative scale
    Vector2 parentGlobalScale = parentTransform->GetGlobalScale();
    float safeScaleX = (parentGlobalScale.x != 0.0f) ? parentGlobalScale.x : 1.0f;
    float safeScaleY = (parentGlobalScale.y != 0.0f) ? parentGlobalScale.y : 1.0f;

    scale = { targetGlobalScale.x / safeScaleX, targetGlobalScale.y / safeScaleY };
}

int Transform2d::GetGlobalZIndex() const {
    if (!owner || !owner->GetParent()) return zIndex;
            
    auto parentTransform = owner->GetParent()->GetComponent<Transform2d>();
    if (!parentTransform) return zIndex;

    return parentTransform->GetGlobalZIndex() + zIndex;
}

#ifndef STANDALONE_MODE
void Transform2d::OnInspector() {
    // The Editor always modifies LOCAL values
    EditorUI::DragFloat2("Position", &position.x, 0.1f, this);
    EditorUI::DragFloat2("Scale", &scale.x, 0.1f, this);
    EditorUI::DragFloat("Rotation", &rotation, 1.0f, this);

    ImGui::Separator();

    int tempZ = zIndex;
    if (ImGui::InputInt("Z-Index", &tempZ)) {
        nlohmann::json initialState = Serialize();
        zIndex = tempZ;
        CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, initialState, Serialize()));
    }

    // Debug information to display the actual global position in the editor
    ImGui::Separator();
    Vector2 gPos = GetGlobalPosition();
    ImGui::TextDisabled("Global Pos: (%.1f, %.1f)", gPos.x, gPos.y);
}
#endif

nlohmann::json Transform2d::Serialize() const {
    return {
        { "type", "Transform2d" },
        { "x", position.x },
        { "y", position.y },
        { "scaleX", scale.x },
        { "scaleY", scale.y },
        { "rotation", rotation },
        { "zIndex", zIndex }
    };
}

void Transform2d::Deserialize(const nlohmann::json& j) {
    position.x = j.value("x", 0.0f);
    position.y = j.value("y", 0.0f);
    scale.x    = j.value("scaleX", 1.0f);
    scale.y    = j.value("scaleY", 1.0f);
    rotation   = j.value("rotation", 0.0f);
    zIndex = j.value("zIndex", 0);
}
