#pragma once

#include "scene/Component.hpp"
#include <raylib.h>

#ifndef STANDALONE_MODE
#include "editor/EditorUI.hpp"
#endif

class RigidBody2d : public Component {
    public:
        Vector2 velocity;
        float mass;
        float gravityScale;
        bool isKinematic; // If true, physics logic ignores forces/gravity for this object
        bool isGrounded;

        RigidBody2d()
            : velocity{0.0f, 0.0f}, mass(1.0f), gravityScale(1.0f), isKinematic(false), isGrounded(false) {}

        std::string GetName() const override {
            return "RigidBody 2D";
        }

#ifndef STANDALONE_MODE
        void OnInspector() override {
            EditorUI::Checkbox("Is Kinematic", &isKinematic, this);
        
            // Disable Mass and Gravity inputs if the object is Kinematic
            if (!isKinematic) {
                EditorUI::DragFloat("Mass", &mass, 0.1f, this, 0.01f, 1000.0f);
                EditorUI::DragFloat("Gravity Scale", &gravityScale, 0.1f, this, -10.0f, 10.0f);
            }
            
            EditorUI::DragFloat2("Velocity", &velocity.x, 0.1f, this);
        }
#endif

        nlohmann::json Serialize() const override {
            return {
                { "type", "RigidBody2d" },
                { "velocityX", velocity.x },
                { "velocityY", velocity.y },
                { "mass", mass },
                { "gravityScale", gravityScale },
                { "isKinematic", isKinematic }
            };
        }

        void Deserialize(const nlohmann::json& j) override {
            velocity.x = j.value("velocityX", 0.0f);
            velocity.y = j.value("velocityY", 0.0f);
            mass = j.value("mass", 1.0f);
            gravityScale = j.value("gravityScale", 1.0f);
            isKinematic = j.value("isKinematic", false);
        }
};
