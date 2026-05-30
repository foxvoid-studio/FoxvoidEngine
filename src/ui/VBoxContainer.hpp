#pragma once

#include "scene/Component.hpp"
#include <nlohmann/json.hpp>
#include <string>

class VBoxContainer : public Component {
    public:
        // The vertical space (in pixels) added between each child element
        float spacing;

        // Inner padding at the top and bottom of the container
        float paddingTop;
        float paddingBottom;

        // Forces the horizontal alignement of children.
        // 0.0 = Left, 0.5 = Center, 1.0 = Right
        float horizontalAlignment;

        VBoxContainer();
        ~VBoxContainer() override = default;

        // Called every frame to calculate and enforce the layout of children
        void Update(float deltaTime) override;

        std::string GetName() const override;

#ifndef STANDALONE_MODE
        void OnInspector() override;
#endif

        nlohmann::json Serialize() const override;
        void Deserialize(const nlohmann::json& j) override;
};
