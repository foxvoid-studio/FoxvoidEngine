#pragma once

#include "world/Component.hpp"
#include "world/GameObject.hpp"
#include <raylib.h>
#include <nlohmann/json.hpp>
#include <string>
#include <core/UUID.hpp>

// Represents a 3D model component that can be attached to a GameObject.
// Handles loading, unloading, and storing the data required for 3D rendering.
class [[gnu::visibility("default")]] MeshRenderer : public Component {
    public:
        UUID m_modelUUID = 0;
        Model model = { 0 };
        bool isLoaded = false;
        Color tint = WHITE;

        // Constructor & Destructor
        MeshRenderer();
        ~MeshRenderer();

        // Returns the component's UI name
        std::string GetName() const override;

        // Resolves the path to a UUID and loads the model
        void LoadModelFromPath(const std::string& path);
        
        // Loads a 3D model from its unique identifier
        void LoadModelFromUUID(UUID uuid);

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
