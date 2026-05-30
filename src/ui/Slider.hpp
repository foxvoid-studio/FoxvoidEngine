#pragma once

#include "scene/Component.hpp"
#include <raylib.h>
#include <nlohmann/json.hpp>
#include <string>

class Slider : public Component {
    public:
        // The current value of the slider
        float value;
        
        // The constraints for the value
        float minValue;
        float maxValue;

        // Visual properties
        Color trackColor;
        Color handleColor;
        Color activeHandleColor; // Color when the user is actively dragging the handle

        Slider();
        ~Slider() override = default;

        // Handles the dragging logic
        void Update(float deltaTime) override;
        
        // Directly draws the track and the handle
        void RenderHUD() override;

        std::string GetName() const override;

#ifndef STANDALONE_MODE
        void OnInspector() override;
#endif

        nlohmann::json Serialize() const override;
        void Deserialize(const nlohmann::json& j) override;

    private:
        // Tracks if the user is currently holding the mouse down on this slider
        bool m_isDragging;
};
