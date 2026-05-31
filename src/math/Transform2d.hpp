#pragma once

#include "scene/Component.hpp"
#include "scene/GameObject.hpp"
#include <raylib.h>
#include <cmath>

#ifndef STANDALONE_MODE
#include <imgui.h>
#include <editor/commands/CommandHistory.hpp>
#include <editor/commands/ModifyComponentCommand.hpp>
#include <editor/EditorUI.hpp>
#endif

class Transform2d : public Component {
    public:
        Vector2 position;
        float rotation;  // In degrees, as Raylib expects degrees for drawing
        Vector2 scale;

        // Controls the render order (Higher = rendered on top)
        // Z-Index can also accumulate so children naturally render above their parent
        int zIndex;

        // Constructor with default values
        Transform2d(float x = 0.0f, float y = 0.0f) 
            : position{x, y}, rotation(0.0f), scale{1.0f, 1.0f}, zIndex(0) {}

#pragma region Global Space Calculations

        Vector2 GetGlobalPosition() const;

        void SetGlobalPosition(Vector2 targetGlobalPos);

        float GetGlobalRotation() const;

        void SetGlobalRotation(float targetGlobalRot);

        Vector2 GetGlobalScale() const;

        void SetGlobalScale(Vector2 targetGlobalScale);

        int GetGlobalZIndex() const;

#pragma endregion

        std::string GetName() const override {
            return "Transform 2D";
        }

#ifndef STANDALONE_MODE
        void OnInspector() override;
#endif

        nlohmann::json Serialize() const override;

        void Deserialize(const nlohmann::json& j) override;
};
