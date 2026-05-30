#pragma once

#include "scene/Component.hpp"
#include <raylib.h>
#include <string>
#include <algorithm>

#ifndef STANDALONE_MODE
#include "editor/EditorUI.hpp"
#endif

class CapsuleCollider : public Component {
    public:
        float radius;
        float height; // Total height from top to bottom
        Vector2 offset;
        bool isTrigger;

        // Default dimensions matching the 50x50 RectCollider visually
        CapsuleCollider(float r = 25.0f, float h = 50.0f) 
            : radius(r), height(h), offset{0.0f, 0.0f}, isTrigger(false) {
            
            // A capsule cannot have a height smaller than its two circular ends combined
            if (height < radius * 2.0f) height = radius * 2.0f;
        }

        std::string GetName() const override {
            return "Capsule Collider";
        }

#ifndef STANDALONE_MODE
        void OnInspector() override {
            EditorUI::DragFloat("Radius", &radius, 0.5f, this, 0.1f, 1000.0f);
            EditorUI::DragFloat("Height", &height, 0.5f, this, 0.1f, 1000.0f);
            EditorUI::DragFloat2("Offset", &offset.x, 1.0f, this);
            EditorUI::Checkbox("Is Trigger", &isTrigger, this);
            
            // Constrain height dynamically in the editor
            if (height < radius * 2.0f) height = radius * 2.0f; 
        }
#endif

        nlohmann::json Serialize() const override {
            return {
                { "type", "CapsuleCollider" },
                { "radius", radius },
                { "height", height },
                { "offsetX", offset.x },
                { "offsetY", offset.y },
                { "isTrigger", isTrigger }
            };
        }

        void Deserialize(const nlohmann::json& j) override {
            radius = j.value("radius", 25.0f);
            height = j.value("height", 50.0f);
            offset.x = j.value("offsetX", 0.0f);
            offset.y = j.value("offsetY", 0.0f);
            isTrigger = j.value("isTrigger", false);
        }
};
