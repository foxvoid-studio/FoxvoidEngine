#pragma once

#include "world/Component.hpp"
#include "world/GameObject.hpp"
#include <raylib.h>
#include <raymath.h>

#ifndef STANDALONE_MODE
#include <imgui.h>
#include <editor/commands/CommandHistory.hpp>
#include <editor/commands/ModifyComponentCommand.hpp>
#include <editor/EditorUI.hpp>
#endif


class [[gnu::visibility("default")]] Transform3d : public Component {
    public:
        Vector3 position;
        Vector3 rotation;
        Vector3 scale;

        Transform3d(float x = 0.0f, float y = 0.0f, float z = 0.0f)
            : position{x, y, z}, rotation{0.0f, 0.0f, 0.0f}, scale{1.0f, 1.0f, 1.0f} {}

#pragma region Matrix Calculations
        // Generates the transformation matrix for this specific object (Local Space)
        Matrix GetLocalMatrix() const {
            // Scale
            Matrix matScale = MatrixScale(scale.x, scale.y, scale.z);

            // Rotate (Convert degrees to radians)
            Matrix matRot = MatrixRotateXYZ({
                rotation.x * DEG2RAD,
                rotation.y * DEG2RAD,
                rotation.z * DEG2RAD
            });

            // Translate
            Matrix matTrans = MatrixTranslate(position.x, position.y, position.z);

            // Combine: Scale -> Rotate -> Translate
            return MatrixMultiply(MatrixMultiply(matScale, matRot), matTrans);
        }

        // Recursively calculates the absolute transformation matrix in the world
        Matrix GetGlobalMatrix() const {
            if (!owner || !owner->GetParent()) return GetLocalMatrix();

            auto parentTransform = owner->GetParent()->GetComponent<Transform3d>();

            // Multiply local matrix by parent's global matrix
            return MatrixMultiply(GetLocalMatrix(), parentTransform->GetGlobalMatrix());
        }
#pragma endregion

#pragma region Global Space Calculations

        Vector3 GetGlobalPosition() const {
            if (!owner || !owner->GetParent()) return position;

            // Extract the position directly from the Global Matrix
            // It applies all parent scales, rotations, and translations to our local origin (0,0,0)
            return Vector3Transform({0.0f, 0.0f, 0.0f}, GetGlobalMatrix());
        }

        void SetGlobalPosition(Vector3 targetGlobalPos) {
            if (!owner || !owner->GetParent()) {
                position = targetGlobalPos;
                return;
            }
            
            auto parentTransform = owner->GetParent()->GetComponent<Transform3d>();
            if (!parentTransform) {
                position = targetGlobalPos;
                return;
            }

            // Convert the target global position back into our parent's local space
            Matrix parentGlobalInv = MatrixInvert(parentTransform->GetGlobalMatrix());
            position = Vector3Transform(targetGlobalPos, parentGlobalInv);
        }

        // Uses Quaternions to safely accumulate rotations without Gimbal Lock
        Quaternion GetGlobalQuaternion() const {
            Quaternion localQ = QuaternionFromEuler(rotation.x * DEG2RAD, rotation.y * DEG2RAD, rotation.z * DEG2RAD);
            
            if (!owner || !owner->GetParent()) return localQ;
            
            auto parentTransform = owner->GetParent()->GetComponent<Transform3d>();
            if (!parentTransform) return localQ;

            // Combine parent global rotation with our local rotation
            return QuaternionMultiply(parentTransform->GetGlobalQuaternion(), localQ);
        }

        Vector3 GetGlobalRotation() const {
            if (!owner || !owner->GetParent()) return rotation;

            // Convert the safe global quaternion back to human-readable Euler angles
            Vector3 eulerRads = QuaternionToEuler(GetGlobalQuaternion());
            return { eulerRads.x * RAD2DEG, eulerRads.y * RAD2DEG, eulerRads.z * RAD2DEG };
        }

        Vector3 GetGlobalScale() const {
            if (!owner || !owner->GetParent()) return scale;
            
            auto parentTransform = owner->GetParent()->GetComponent<Transform3d>();
            if (!parentTransform) return scale;

            // Scales are multiplied down the hierarchy
            Vector3 parentGlobalScale = parentTransform->GetGlobalScale();
            return { parentGlobalScale.x * scale.x, parentGlobalScale.y * scale.y, parentGlobalScale.z * scale.z };
        }

#pragma endregion

        std::string GetName() const override {
            return "Transform 3D";
        }

#ifndef STANDALONE_MODE
        void OnInspector() override {
            EditorUI::DragFloat3("Position", &position.x, 0.1f, this);
            EditorUI::DragFloat3("Rotation", &rotation.x, 1.0f, this);
            EditorUI::DragFloat3("Scale", &scale.x, 0.1f, this);

            ImGui::Separator();
            Vector3 gPos = GetGlobalPosition();
            ImGui::TextDisabled("Global Pos: (%.1f, %.1f, %.1f)", gPos.x, gPos.y, gPos.z);
        }
#endif

        nlohmann::json Serialize() const override {
            return {
                { "type", "Transform3d" },
                { "x", position.x },
                { "y", position.y },
                { "z", position.z },
                { "rotX", rotation.x },
                { "rotY", rotation.y },
                { "rotZ", rotation.z },
                { "scaleX", scale.x },
                { "scaleY", scale.y },
                { "scaleZ", scale.z }
            };
        }

        void Deserialize(const nlohmann::json& j) override {
            position.x = j.value("x", 0.0f);
            position.y = j.value("y", 0.0f);
            position.z = j.value("z", 0.0f);
            
            rotation.x = j.value("rotX", 0.0f);
            rotation.y = j.value("rotY", 0.0f);
            rotation.z = j.value("rotZ", 0.0f);
            
            scale.x    = j.value("scaleX", 1.0f);
            scale.y    = j.value("scaleY", 1.0f);
            scale.z    = j.value("scaleZ", 1.0f);
        }
};
