#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include "physics/colliders/Collision2D.hpp"

#ifndef STANDALONE_MODE
#include "imgui.h"
#endif

// Forward declaration to avoid circular dependencies
class GameObject; 

class Component {
    public:
        // Pointer to the GameObject that owns this component
        GameObject* owner = nullptr; 

        Component() = default;

        // Virtual destructor is mandatory for proper cleanup of derived classes
        virtual ~Component() = default;

        Component(const Component&) = delete;
        Component& operator=(const Component&) = delete;

        // Called once when the component is initialized
        virtual void Start() {}

        // Called every frame for logic and physics
        virtual void Update(float deltaTime) {}

        // Called every frame after update, specifically for drawing via Raylib
        virtual void Render() {}

        // Called after the camera has finished drawing the world
        // Used for UI elements like health bars, score text, etc.
        virtual void RenderHUD() {}

        virtual void OnCollision(const Collision2D& collision) {}

        // Called whan an Animator2d triggers a specific frame event.
        // Can be overridden by scripts, audio players, or particle emitters.
        virtual void OnAnimationEvent(const std::string& eventName) {}

        // Called when a GUI button attached to the same GameObject is clicked
        virtual void OnGUIClick(const std::string& buttonName) {}

        // Returns the name of the component for the Inspecteur header
        virtual std::string GetName() const { return "Component"; }

#ifndef STANDALONE_MODE
        // Called by the Editor to draw ImGui controls for this specific component
        virtual void OnInspector() {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No exposed variables.");
        }
#endif

        // Serializes the component's data into a JSON object
        virtual nlohmann::json Serialize() const {
            return nlohmann::json::object();
        }
    
        // Restores the component's data from a JSON object
        virtual void Deserialize(const nlohmann::json& j) {}
};
