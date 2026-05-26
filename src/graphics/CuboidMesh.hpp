#pragma once

#include "world/Component.hpp"
#include "world/GameObject.hpp"
#include <raylib.h>
#include <nlohmann/json.hpp>
#include <string>

class CuboidMesh : public Component {
    public:
        Vector3 size; // Base dimensions of the cube
        Color color;
        bool isVisible;
        bool drawWireframe;

        CuboidMesh();

        // Called by the Scene during the Render pass
        void Render() override;

        std::string GetName() const override;

#ifndef STANDALONE_MODE
        void OnInspector() override;
#endif

        nlohmann::json Serialize() const override;
        void Deserialize(const nlohmann::json& j) override;
};
