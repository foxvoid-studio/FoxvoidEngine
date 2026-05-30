#pragma once

#include "scene/Component.hpp"
#include <raylib.h>
#include <vector>
#include <string>

#ifndef STANDALONE_MODE
#include "editor/EditorUI.hpp"
#include <imgui.h>
#endif

class PolygonCollider : public Component {
    public:
        // Stores the local coordinates of the polygon's corners
        std::vector<Vector2> localVertices;
        Vector2 offset;
        bool isTrigger;

        PolygonCollider() : offset{0.0f, 0.0f}, isTrigger(false) {
            // Default shape: A simple upward-pointing triangle
            localVertices.push_back({0.0f, -25.0f});   // Top
            localVertices.push_back({25.0f, 25.0f});   // Bottom Right
            localVertices.push_back({-25.0f, 25.0f});  // Bottom Left
        }

        std::string GetName() const override {
            return "Polygon Collider";
        }

#ifndef STANDALONE_MODE
        void OnInspector() override {
            EditorUI::Checkbox("Is Trigger", &isTrigger, this);
            EditorUI::DragFloat2("Offset", &offset.x, 1.0f, this);
            
            ImGui::Separator();
            ImGui::TextDisabled("Vertices (Must be Convex & Clockwise)");
            
            // Allow editing of individual vertices
            for (size_t i = 0; i < localVertices.size(); ++i) {
                ImGui::PushID(i); // Push ID to prevent ImGui label collisions
                std::string label = "Vertex " + std::to_string(i);
                EditorUI::DragFloat2(label.c_str(), &localVertices[i].x, 1.0f, this);
                ImGui::PopID();
            }

            ImGui::Spacing();

            // Add/Remove vertices dynamically
            if (ImGui::Button("Add Vertex")) {
                localVertices.push_back({0.0f, 0.0f});
            }
            ImGui::SameLine();
            // We enforce a minimum of 3 vertices to form a valid 2D polygon
            if (ImGui::Button("Remove Last") && localVertices.size() > 3) {
                localVertices.pop_back(); 
            }
        }
#endif

        nlohmann::json Serialize() const override {
            nlohmann::json j;
            j["type"] = "PolygonCollider";
            j["offsetX"] = offset.x;
            j["offsetY"] = offset.y;
            j["isTrigger"] = isTrigger;
            
            // Serialize the dynamic array of vertices
            nlohmann::json vertsArray = nlohmann::json::array();
            for (const auto& v : localVertices) {
                vertsArray.push_back({{"x", v.x}, {"y", v.y}});
            }
            j["vertices"] = vertsArray;
            
            return j;
        }

        void Deserialize(const nlohmann::json& j) override {
            offset.x = j.value("offsetX", 0.0f);
            offset.y = j.value("offsetY", 0.0f);
            isTrigger = j.value("isTrigger", false);
            
            if (j.contains("vertices") && j["vertices"].is_array()) {
                localVertices.clear();
                for (const auto& vJson : j["vertices"]) {
                    localVertices.push_back({vJson.value("x", 0.0f), vJson.value("y", 0.0f)});
                }
            }
        }
};
