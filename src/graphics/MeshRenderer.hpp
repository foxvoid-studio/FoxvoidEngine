#pragma once

#include "world/Component.hpp"
#include "world/GameObject.hpp"
#include <raylib.h>
#include <nlohmann/json.hpp>
#include <string>

// Represents a 3D model component that can be attached to a GameObject.
// Handles loading, unloading, and storing the data required for 3D rendering.
class MeshRenderer : public Component {
    public:
        std::string modelPath = "";
        Model model = { 0 };
        bool isLoaded = false;
        Color tint = WHITE;

        // Constructor & Destructor
        MeshRenderer();
        ~MeshRenderer();

        // Returns the component's UI name
        std::string GetName() const override;

        // Loads a 3D model from the disk and applies the lighting shader to it
        void LoadModelFromPath(const std::string& path);

        // Safely unloads the model from the GPU memory
        void UnloadCurrentModel();

        // Called every frame by the Scene to draw the model
        void Render() override;

// Editor-only methods
#ifndef STANDALONE_MODE
        void OnInspector() override;
#endif

        // Serialization methods for saving/loading the scene
        nlohmann::json Serialize() const override;
        void Deserialize(const nlohmann::json& j) override;
};
