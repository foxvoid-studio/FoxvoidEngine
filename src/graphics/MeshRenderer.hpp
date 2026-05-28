#pragma once

#include "world/Component.hpp"
#include "world/GameObject.hpp"
#include <raylib.h>
#include <raymath.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <core/UUID.hpp>

// Represents a sub-part of a 3D model (e.g., an arm, a head, or a pivot point)
struct ModelNode {
    std::string name;
    int gltfNodeIndex = -1; // The absolute unique ID of this node in the GLTF file
    
    // A node can have multiple meshes if it uses multiple materials (primitives)
    std::vector<int> meshIndices; 

    // Save the baked matrix if it exists to perfectly mirror Raylib's initialization
    bool hasMatrix = false;
    Matrix localMatrix = MatrixIdentity();

    // Local transform (This is what the animation will modify!)
    Vector3 translation = { 0.0f, 0.0f, 0.0f };
    Quaternion rotation = { 0.0f, 0.0f, 0.0f, 1.0f };
    Vector3 scale = { 1.0f, 1.0f, 1.0f };

    // Store the initial global matrix and its inverse to "un-bake" Raylib's pre-transformations
    Matrix initialGlobalMatrix = MatrixIdentity();
    Matrix inverseBindMatrix = MatrixIdentity();
    
    // The final calculated matrix for rendering
    Matrix globalMatrix = MatrixIdentity();
    
    // Children nodes (e.g., Hand is a child of Arm)
    std::vector<ModelNode> children;
};

// Represents a 3D model component that can be attached to a GameObject.
class [[gnu::visibility("default")]] MeshRenderer : public Component {
    public:
        UUID m_modelUUID = 0;
        Model model = { 0 };
        bool isLoaded = false;
        Color tint = WHITE;

        ModelNode rootNode;
        bool usesCustomHierarchy = false; // True for Kenney-style models
        nlohmann::json m_gltfJsonCache; // The JSON extracted from the .glb

        std::vector<uint8_t> m_glbBinBuffer; // Stores raw animation and geometry binary chunk data

        // Reads the GLB file to reconstruct the parent-child relationships
        void LoadCustomHierarchy(const std::string& path);

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

    private:
        // Helper to calculate inverse matrices upon loading
        void ComputeInitialTransforms(ModelNode& node, const Matrix& parentMatrix);

        // Recursive rendering function for hierarchical models
        void RenderNodeRecursive(ModelNode& node, const Matrix& parentGlobalMatrix);
};
