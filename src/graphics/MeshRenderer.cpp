#include "MeshRenderer.hpp"
#include "graphics/light/LightingSystem.hpp"
#include <iostream>
#include <cstring>
#include <physics/Transform3d.hpp>

#ifndef STANDALONE_MODE
#include <imgui.h>
#include "editor/commands/CommandHistory.hpp"
#include "editor/commands/ModifyComponentCommand.hpp"
#endif

// Default constructor
MeshRenderer::MeshRenderer() = default;

// Destructor ensures GPU memory is freed when the component is destroyed
MeshRenderer::~MeshRenderer() {
    UnloadCurrentModel();
}

std::string MeshRenderer::GetName() const {
    return "Mesh Renderer";
}

void MeshRenderer::LoadModelFromPath(const std::string& path) {
    // Prevent loading empty paths
    if (path.empty()) return;

    // Ensure any previously loaded model is freed to prevent memory leaks
    UnloadCurrentModel();

    // Load the model using Raylib's native loader (.glb, .gltf, .obj, etc.)
    model = LoadModel(path.c_str());
    
    // Check if the model has valid mesh data
    if (model.meshCount > 0) {
        isLoaded = true;
        modelPath = path;

        // Fetch the active lighting shader from our custom LightingSystem
        Shader lightShader = LightingSystem::GetShader();
        
        // Apply the custom lighting shader to all materials of this newly loaded model
        if (lightShader.id != 0) {
            for (int i = 0; i < model.materialCount; i++) {
                model.materials[i].shader = lightShader;
            }
        }
    } else {
        std::cerr << "[MeshRenderer] Failed to load model: " << path << std::endl;
    }
}

void MeshRenderer::UnloadCurrentModel() {
    // Only unload if a model is currently in memory
    if (isLoaded) {
        UnloadModel(model);
        isLoaded = false;
    }
}

void MeshRenderer::Render() {
    if (!isLoaded || !owner) return;

    auto transform = owner->GetComponent<Transform3d>();
    if (transform) {
        // Raylib's DrawModel uses the transform matrix embedded inside the Model struct.
        // We update it directly with our Entity's global matrix before drawing!
        model.transform = transform->GetGlobalMatrix();
        
        // Note: We use Vector3Zero() because the position is already baked into the transform matrix above
        DrawModel(model, Vector3Zero(), 1.0f, tint);
    }
}

#ifndef STANDALONE_MODE
void MeshRenderer::OnInspector() {
    // Static buffer to hold the text input for the model path in ImGui
    static char pathBuffer[256] = "";
    
    // Update the UI buffer only if the user is not actively typing in it
    // This prevents overwriting the text cursor while typing
    if (std::string(pathBuffer) != modelPath && !ImGui::IsItemActive()) {
        strncpy(pathBuffer, modelPath.c_str(), sizeof(pathBuffer));
        // Ensure null termination
        pathBuffer[sizeof(pathBuffer) - 1] = '\0'; 
    }

    ImGui::Text("Model Path (.glb, .obj):");
    
    // Input field for the path. Triggers an update only when ENTER is pressed.
    if (ImGui::InputText("##modelpath", pathBuffer, sizeof(pathBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
        std::string newPath = pathBuffer;
        
        // If the path actually changed, record the action and load the new model
        if (newPath != modelPath) {
            nlohmann::json stateBefore = Serialize();
            LoadModelFromPath(newPath);
            CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, stateBefore, Serialize()));
        }
    }
    
    // Tooltip helper
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Press ENTER to load the model");
    }

    // Tint color picker (Allows tinting the model while preserving lighting)
    // Convert Raylib's 0-255 format to ImGui's 0.0-1.0 float format
    float colorFloat[4] = { tint.r / 255.0f, tint.g / 255.0f, tint.b / 255.0f, tint.a / 255.0f };
    
    if (ImGui::ColorEdit4("Tint", colorFloat)) {
        tint.r = static_cast<unsigned char>(colorFloat[0] * 255.0f);
        tint.g = static_cast<unsigned char>(colorFloat[1] * 255.0f);
        tint.b = static_cast<unsigned char>(colorFloat[2] * 255.0f);
        tint.a = static_cast<unsigned char>(colorFloat[3] * 255.0f);
        
        // Note: For a robust Undo/Redo on the color, you might want to track 
        // the ImGui::IsItemDeactivatedAfterEdit() state similar to the light components.
    }
}
#endif

nlohmann::json MeshRenderer::Serialize() const {
    return {
        { "type", "MeshRenderer" },
        { "modelPath", modelPath },
        { "tint", { tint.r, tint.g, tint.b, tint.a } }
    };
}

void MeshRenderer::Deserialize(const nlohmann::json& j) {
    // Safely extract the tint color array
    if (j.contains("tint") && j["tint"].is_array() && j["tint"].size() == 4) {
        tint.r = j["tint"][0];
        tint.g = j["tint"][1];
        tint.b = j["tint"][2];
        tint.a = j["tint"][3];
    }
    
    // Load the model if a path is present in the save file
    std::string path = j.value("modelPath", "");
    if (!path.empty()) {
        LoadModelFromPath(path);
    }
}
