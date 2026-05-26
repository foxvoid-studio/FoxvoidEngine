#pragma once

#include "world/Component.hpp"
#include "world/GameObject.hpp"
#include <raylib.h>
#include <nlohmann/json.hpp>
#include <string>

class Camera3d : public Component {
    public:
        bool isMain = true;
        float fov = 60.0f; // Standard field of view in degrees
        int projection = CAMERA_PERSPECTIVE; // 0 = Perspective, 1 = Orthographic

        Camera3d() = default;

        // Generates the native Raylib Camera3D struct needed for rendering
        Camera3D GetRaylibCamera() const;

        std::string GetName() const override;

#ifndef STANDALONE_MODE
        void OnInspector() override;
#endif

        nlohmann::json Serialize() const override;
        void Deserialize(const nlohmann::json& j) override;
};
