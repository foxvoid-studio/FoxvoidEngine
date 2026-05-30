#pragma once

#include "scene/Component.hpp"
#include <raylib.h>
#include <nlohmann/json.hpp>
#include <string>

class RectTransform: public Component {
    public:
        // Size of the UI element in pixels
        Vector2 size;
        
        // Offset in pixels relative to the anchor point
        Vector2 position;
        
        // Normalized screen anchor (0.0 to 1.0)
        // (0,0) = Top-Left, (1,1) = Bottom-Right, (0.5, 0.5) = Center
        Vector2 anchor;
        
        // Normalized pivot of the UI element itself (0.0 to 1.0)
        // Determines which part of the size rectangle is at the 'position'
        Vector2 pivot;

        RectTransform();
        ~RectTransform() override = default;

        // The magic function: Calculates the final absolute screen rectangle!
        Rectangle GetScreenRect() const;

        std::string GetName() const override;

#ifndef STANDALONE_MODE
        void OnInspector() override;
#endif

        nlohmann::json Serialize() const override;
        void Deserialize(const nlohmann::json& j) override;
};
