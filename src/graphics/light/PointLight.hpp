#pragma once

#include "world/Component.hpp"
#include "world/GameObject.hpp"
#include <raylib.h>
#include <nlohmann/json.hpp>

#ifndef STANDALONE_MODE
#include <imgui.h>
#include "editor/commands/CommandHistory.hpp"
#include "editor/commands/ModifyComponentCommand.hpp"
#include "editor/EditorUI.hpp"
#endif

// Represents a light bulb that emits light in all directions.
// Its position (from Transform3d) and radius dictate its area of effect.
class PointLight : public Component {
    public:
        Color color = WHITE;
        float intensity = 1.0f;
        float radius = 10.0f; // How far the light reaches before fading to complete darkness

        std::string GetName() const override {
            return "Point Light";
        }

#ifndef STANDALONE_MODE
        void OnInspector() override {
            // Convert Raylib's 0-255 color format to ImGui's normalized 0.0-1.0 format
            float colorFloat[3] = { color.r / 255.0f, color.g / 255.0f, color.b / 255.0f };
            
            // Track state for Undo/Redo operations
            static nlohmann::json initialState;
            static bool isEditing = false;
            bool valueChanged = false;

            if (!isEditing) {
                initialState = Serialize();
            }

            // Draw UI elements
            if (ImGui::ColorEdit3("Light Color", colorFloat)) {
                color.r = static_cast<unsigned char>(colorFloat[0] * 255.0f);
                color.g = static_cast<unsigned char>(colorFloat[1] * 255.0f);
                color.b = static_cast<unsigned char>(colorFloat[2] * 255.0f);
                valueChanged = true;
                isEditing = true;
            }

            if (ImGui::DragFloat("Intensity", &intensity, 0.05f, 0.0f, 100.0f)) {
                valueChanged = true;
                isEditing = true;
            }

            if (ImGui::DragFloat("Radius", &radius, 0.5f, 0.1f, 1000.0f)) {
                valueChanged = true;
                isEditing = true;
            }

            // Register the undoable command ONLY when the user releases the mouse click
            if (isEditing && ImGui::IsItemDeactivatedAfterEdit()) {
                CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, initialState, Serialize()));
                isEditing = false;
            }
        }
#endif

        nlohmann::json Serialize() const override {
            return {
                { "type", "PointLight" },
                { "r", color.r },
                { "g", color.g },
                { "b", color.b },
                { "a", color.a },
                { "intensity", intensity },
                { "radius", radius }
            };
        }

        void Deserialize(const nlohmann::json& j) override {
            color.r = j.value("r", 255);
            color.g = j.value("g", 255);
            color.b = j.value("b", 255);
            color.a = j.value("a", 255);
            intensity = j.value("intensity", 1.0f);
            radius = j.value("radius", 10.0f);
        }
};