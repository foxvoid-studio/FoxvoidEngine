#pragma once

#include "scene/Component.hpp"
#include "scene/GameObject.hpp"
#include "math/Transform2d.hpp"
#include "ui/core/RectTransform.hpp"
#include <raylib.h>
#include <rlgl.h>

#ifndef STANDALONE_MODE
#include "editor/EditorUI.hpp"
#include <imgui.h>
#endif

class ShapeRenderer : public Component {
    public:
        Color color;
        float width;
        float height;
        bool isHUD;

        float roundness;
        int segments;

        ShapeRenderer(float w = 50.0f, float h = 50.0f, Color c = WHITE, bool hud = false) 
            : width(w), height(h), color(c), isHUD(hud), roundness(0.0f), segments(10) {}

        // We only override Render, as this component doesn't need to update logic
        void Render() override {
            // Skip rendering in world space if marked as HUD
            if (isHUD || !owner) return;

            // Fetch the Transform component from the parent GameObject
            Transform2d* transform = owner->GetComponent<Transform2d>();
            
            // Safety check: only draw if the GameObject actually has a Transform
            if (transform != nullptr) {
                
                // Create a Raylib Rectangle definition
                // We multiply width and height by the transform's scale
                auto position = transform->GetGlobalPosition();
                Rectangle rec = { 
                    position.x, 
                    position.y, 
                    width * transform->scale.x, 
                    height * transform->scale.y 
                };
                
                // Define the origin point for rotation (center of the rectangle)
                Vector2 origin = { rec.width / 2.0f, rec.height / 2.0f };
                
                if (roundness > 0.0f) {
                    // Since Raylib lacks DrawRectangleRoundedPro, we use OpenGL matrices
                    rlPushMatrix();
                        // Move to the object's position
                        rlTranslatef(position.x, position.y, 0.0f);
                        // Rotate around the Z axis
                        rlRotatef(transform->rotation, 0.0f, 0.0f, 1.0f);
                        
                        // Draw the rounded rect centered on the new origin (0,0)
                        Rectangle localRec = { -origin.x, -origin.y, rec.width, rec.height };
                        DrawRectangleRounded(localRec, roundness, segments, color);
                    rlPopMatrix();
                }
                else {
                    // Legacy fast-path for perfectly square rectangles
                    DrawRectanglePro(rec, origin, transform->rotation, color);
                }
            }
        }

        void RenderHUD() override {
            if (!isHUD || !owner) return;

            // UI Elements should prefer RectTransform
            if (RectTransform* rectTransform = owner->GetComponent<RectTransform>()) {
                Rectangle rec = rectTransform->GetScreenRect();
                
                if (roundness > 0.0f) {
                    DrawRectangleRounded(rec, roundness, segments, color);
                } else {
                    DrawRectangleRec(rec, color);
                }

                return;
            }

            // Fallback: Legacy Transform2d support for older scenes
            Transform2d* transform = owner->GetComponent<Transform2d>();
            
            if (transform != nullptr) {
                auto position = transform->GetGlobalPosition();
                Rectangle rec = { 
                    position.x, 
                    position.y, 
                    width * transform->scale.x, 
                    height * transform->scale.y 
                };
                
                Vector2 origin = { rec.width / 2.0f, rec.height / 2.0f };

                if (roundness > 0.0f) {
                    rlPushMatrix();
                        rlTranslatef(position.x, position.y, 0.0f);
                        rlRotatef(transform->rotation, 0.0f, 0.0f, 1.0f);
                        Rectangle localRec = { -origin.x, -origin.y, rec.width, rec.height };
                        DrawRectangleRounded(localRec, roundness, segments, color);
                    rlPopMatrix();
                } 
                else {
                    DrawRectanglePro(rec, origin, transform->rotation, color);
                }
            }
        }

        std::string GetName() const override {
            return "Shape Renderer";
        }

#ifndef STANDALONE_MODE
        void OnInspector() override {
            EditorUI::DragFloat("Width", &width, 1.0f, this, 0.0f, 10000.0f);
            EditorUI::DragFloat("Height", &height, 1.0f, this, 0.0f, 10000.0f);

            EditorUI::ColorEdit4("Color", &color, this);

            ImGui::Separator();

            ImGui::Text("Appearance");
            EditorUI::DragFloat("Roundness", &roundness, 0.01f, this, 0.0f, 1.0f);

            // Only show segment configuration if the shape is actually rounded
            if (roundness > 0.0f) {
                ImGui::SliderInt("Smoothness", &segments, 0, 36);
            }

            ImGui::Separator();
            EditorUI::Checkbox("Is HUB (Screen Space)", &isHUD, this);
        }
#endif

        nlohmann::json Serialize() const override {
            return {
                { "type", "ShapeRenderer" },
                { "width", width },
                { "height", height },
                { "color", { color.r, color.g, color.b, color.a }},
                { "isHUD", isHUD },
                { "roundness", roundness },
                { "segments", segments }
            };
        }

        void Deserialize(const nlohmann::json& j) override {
            width = j.value("width", 50.0f);
            height = j.value("height", 50.0f);
            isHUD = j.value("isHUD", false);

            roundness = j.value("roundness", 0.0f);
            segments = j.value("segments", 10);
            
            // Safely extract the color array
            if (j.contains("color") && j["color"].is_array() && j["color"].size() == 4) {
                color.r = j["color"][0];
                color.g = j["color"][1];
                color.b = j["color"][2];
                color.a = j["color"][3];
            }
        }
};
