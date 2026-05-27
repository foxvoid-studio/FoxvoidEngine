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
#include <core/AssetRegistry.hpp>

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
    if (path.empty()) {
        LoadModelFromUUID(UUID(0));
        return;
    }
    
    // Interrogate the registry to find the unique ID of this file
    UUID assetId = AssetRegistry::GetUUIDForPath(path);
    LoadModelFromUUID(assetId);
}

void MeshRenderer::LoadModelFromUUID(UUID uuid) {
    m_modelUUID = uuid;
    
    // Ensure any previously loaded model is freed to prevent memory leaks
    UnloadCurrentModel();

    if (m_modelUUID != 0) {
        // Resolve the UUID back to its CURRENT path on the hard drive
        std::string resolvedPath = AssetRegistry::GetPathForUUID(m_modelUUID).string();
        
        if (!resolvedPath.empty()) {
            // Load the model using Raylib's native loader
            model = LoadModel(resolvedPath.c_str());
            
            // Check if the model has valid mesh data
            if (model.meshCount > 0) {
                isLoaded = true;

                // Fetch the active lighting shader from our custom LightingSystem
                Shader lightShader = LightingSystem::GetShader();
                
                // Apply the custom lighting shader to all materials of this newly loaded model
                if (lightShader.id != 0) {
                    for (int i = 0; i < model.materialCount; i++) {
                        model.materials[i].shader = lightShader;
                    }
                }
            } else {
                std::cerr << "[MeshRenderer] Failed to load model: " << resolvedPath << std::endl;
            }
        } else {
            std::cerr << "[MeshRenderer] Error: Could not resolve UUID " << (uint64_t)m_modelUUID << " to a valid path!" << std::endl;
        }
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
    // Dynamically fetch the current path from the registry for the UI
    std::string currentPath = "";
    if (m_modelUUID != 0) {
        currentPath = AssetRegistry::GetPathForUUID(m_modelUUID).string();
    }

    char buffer[256];
    strncpy(buffer, currentPath.c_str(), sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';

    if (ImGui::InputText("Model Path", buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
        std::string newPath(buffer);
        if (newPath != currentPath) {
            nlohmann::json initialState = Serialize();
            LoadModelFromPath(newPath); // Will automatically find the new UUID
            CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, initialState, Serialize()));
        }
    }

    // Drag and Drop support
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
            
            std::string droppedPath = (const char*)payload->Data;
            std::filesystem::path fsPath(droppedPath);
            
            std::string ext = fsPath.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            
            // Allow common 3D formats supported by Raylib
            if (ext == ".glb" || ext == ".gltf" || ext == ".obj" || ext == ".iqm") {
                nlohmann::json initialState = Serialize();
                LoadModelFromPath(droppedPath);
                CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, initialState, Serialize()));
            }
        }
        ImGui::EndDragDropTarget();
    }
    
    ImGui::TextDisabled("Press ENTER to load new model");

    if (isLoaded) {
        ImGui::Text("Meshes: %d | Materials: %d", model.meshCount, model.materialCount);
        ImGui::TextDisabled("UUID: %llu", (uint64_t)m_modelUUID);
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "No model loaded!");
    }

    // Tint color picker (Allows tinting the model while preserving lighting)
    float colorFloat[4] = { tint.r / 255.0f, tint.g / 255.0f, tint.b / 255.0f, tint.a / 255.0f };
    
    if (ImGui::ColorEdit4("Tint", colorFloat)) {
        tint.r = static_cast<unsigned char>(colorFloat[0] * 255.0f);
        tint.g = static_cast<unsigned char>(colorFloat[1] * 255.0f);
        tint.b = static_cast<unsigned char>(colorFloat[2] * 255.0f);
        tint.a = static_cast<unsigned char>(colorFloat[3] * 255.0f);
    }
}
#endif

nlohmann::json MeshRenderer::Serialize() const {
    return {
        { "type", "MeshRenderer" },
        { "modelUUID", (uint64_t)m_modelUUID },
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
    
    // Backward compatibility: Convert old string paths to UUIDs seamlessly
    if (j.contains("modelUUID")) {
        LoadModelFromUUID(UUID(j["modelUUID"].get<uint64_t>()));
    } 
    else if (j.contains("modelPath")) {
        LoadModelFromPath(j["modelPath"].get<std::string>());
    }
}
