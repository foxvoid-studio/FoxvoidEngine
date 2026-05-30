#pragma once

#include "scene/Component.hpp"
#include <raylib.h>

#ifndef STANDALONE_MODE
#include "editor/EditorUI.hpp"
#endif

class RectCollider : public Component {
    public:
        Vector2 size;
        Vector2 offset;
        bool isTrigger;

        // Default 50x50 box
        RectCollider(float width = 50.0f, float height = 50.0f) 
            : size{width, height}, offset{0.0f, 0.0f}, isTrigger(false) {}

        std::string GetName() const override {
            return "Rect Collider";
        }

#ifndef STANDALONE_MODE
        void OnInspector() override {
            EditorUI::DragFloat2("Size", &size.x, 1.0f, this, 0.0f, 10000.0f);
            EditorUI::DragFloat2("Offset", &offset.x, 1.0f, this);
            EditorUI::Checkbox("Is Trigger", &isTrigger, this);
        }
#endif

        nlohmann::json Serialize() const override {
            return {
                { "type", "RectCollider" },
                { "width", size.x },
                { "height", size.y },
                { "offsetX", offset.x },
                { "offsetY", offset.y },
                { "isTrigger", isTrigger }
            };
        }

        void Deserialize(const nlohmann::json& j) override {
            size.x = j.value("width", 50.0f);
            size.y = j.value("height", 50.0f);
            offset.x = j.value("offsetX", 0.0f);
            offset.y = j.value("offsetY", 0.0f);
            isTrigger = j.value("isTrigger", false);
        }
};
