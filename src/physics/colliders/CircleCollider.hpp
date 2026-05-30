#pragma once

#include "scene/Component.hpp"
#include <raylib.h>
#include <string>

#ifndef STANDALONE_MODE
#include "editor/EditorUI.hpp"
#endif

class CircleCollider : public Component {
    public:
        float radius;
        Vector2 offset;
        bool isTrigger;

        // Default radius of 25 to match the 50x50 RectCollider
        CircleCollider(float r = 25.0f) 
            : radius(r), offset{0.0f, 0.0f}, isTrigger(false) {}

        std::string GetName() const override {
            return "Circle Collider";
        }

#ifndef STANDALONE_MODE
        void OnInspector() override {
            EditorUI::DragFloat("Radius", &radius, 0.5f, this, 0.1f, 10000.0f);
            EditorUI::DragFloat2("Offset", &offset.x, 1.0f, this);
            EditorUI::Checkbox("Is Trigger", &isTrigger, this);
        }
#endif

        nlohmann::json Serialize() const override {
            return {
                { "type", "CircleCollider" },
                { "radius", radius },
                { "offsetX", offset.x },
                { "offsetY", offset.y },
                { "isTrigger", isTrigger }
            };
        }

        void Deserialize(const nlohmann::json& j) override {
            radius = j.value("radius", 25.0f);
            offset.x = j.value("offsetX", 0.0f);
            offset.y = j.value("offsetY", 0.0f);
            isTrigger = j.value("isTrigger", false);
        }
};
