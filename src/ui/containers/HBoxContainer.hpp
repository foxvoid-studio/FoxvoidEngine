#pragma once

#include "scene/Component.hpp"
#include <nlohmann/json.hpp>
#include <string>

class HBoxContainer : public Component {
    public:
        // The horizontal space (in pixels) added between each child element
        float spacing;
        
        // Inner padding at the left and right of the container
        float paddingLeft;
        float paddingRight;

        // Forces the vertical alignment of children.
        // 0.0 = Top, 0.5 = Center, 1.0 = Bottom
        float verticalAlignment;

        HBoxContainer();
        ~HBoxContainer() override = default;

        // Called every frame to calculate and enforce the horizontal layout
        void Update(float deltaTime) override;

        // Forces the container to recalculate its children's positions immediately
        void RebuildLayout();

        std::string GetName() const override;

#ifndef STANDALONE_MODE
        void OnInspector() override;
#endif

        nlohmann::json Serialize() const override;
        void Deserialize(const nlohmann::json& j) override;
};
