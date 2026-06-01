#include "SpriteRenderer.hpp"
#include "graphics/Graphics.hpp"
#include "scene/GameObject.hpp"
#include "math/Transform2d.hpp"
#include <iostream>
#include "core/assets/AssetManager.hpp"
#include "core/assets/AssetRegistry.hpp"
#include <filesystem>

#ifndef STANDALONE_MODE
#include "editor/commands/CommandHistory.hpp"
#include "editor/commands/ModifyComponentCommand.hpp"
#endif

SpriteRenderer::SpriteRenderer(const std::string& texturePath) {
    // Load the image into GPU memory
    m_transform = nullptr;
    m_texture.id = 0;

    if (!texturePath.empty()) {
        SetTexture(texturePath);
    }
}

SpriteRenderer::~SpriteRenderer() {}

void SpriteRenderer::SetTexture(const std::string& path) {
    if (path.empty()) {
        SetTexture(UUID(0));
        return;
    }
    
    // Interrogate the registry to find the unique ID of this file
    UUID assetId = AssetRegistry::GetUUIDForPath(path);
    SetTexture(assetId);
}

void SpriteRenderer::SetTexture(UUID uuid) {
    m_textureUUID = uuid;

    // Only try to load if the UUID is valid (not 0)
    if (m_textureUUID != 0) {
        // Resolve the UUID back to its CURRENT path on the hard drive
        std::string resolvedPath = AssetRegistry::GetPathForUUID(m_textureUUID).string();
        
        if (!resolvedPath.empty()) {
            m_texture = AssetManager::GetTexture(resolvedPath);
        } else {
            std::cerr << "[SpriteRenderer] Error: Could not resolve UUID " << (uint64_t)m_textureUUID << " to a valid path!" << std::endl;
            m_texture.id = 0; // Invalidate the local texture if path fails
        }
    }
    else {
        // If the UUID is 0 (cleared), just clear the local struct to stop rendering
        m_texture.id = 0;
    }
}

void SpriteRenderer::Start() {
    // Cache the Transform2d so we don't have to search for it every single frame
    m_transform = owner->GetComponent<Transform2d>();

    if (!m_transform) {
        std::cerr << "[SpriteRenderer] Warning: No Transform2d found on GameObject!" << std::endl;
    }
}

void SpriteRenderer::Render() {
    if (m_transform) {
        // Source Rectangle: The entire texture
        Rectangle sourceRec = { 0.0f, 0.0f, (float)m_texture.width, (float)m_texture.height };
    
        // Destination Rectangle: Position and scaled size
        auto position = m_transform->GetGlobalPosition();
        Rectangle destRec = {
            position.x,
            position.y,
            m_texture.width * m_transform->scale.x,
            m_texture.height * m_transform->scale.y
        };

        // Origin: Center of the scaled texture for correct rotation
        Vector2 origin = { destRec.width / 2.0f, destRec.height / 2.0f };

        // 4. Draw with full transform support
        DrawTexturePro(m_texture, sourceRec, destRec, origin, m_transform->rotation, WHITE);
    }
}

std::string SpriteRenderer::GetName() const {
    return "Sprite Renderer";
}

#ifndef STANDALONE_MODE
void SpriteRenderer::OnInspector() {
    // Dynamically fetch the current path from the registry for the UI
    std::string currentPath = "";
    if (m_textureUUID != 0) {
        currentPath = AssetRegistry::GetPathForUUID(m_textureUUID).string();
    }

    char buffer[256];
    strncpy(buffer, currentPath.c_str(), sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';

    if (ImGui::InputText("Texture Path", buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
        std::string newPath(buffer);
        if (newPath != currentPath) {
            nlohmann::json initialState = Serialize();
            SetTexture(newPath); // Will automatically find the new UUID
            CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, initialState, Serialize()));
        }
    }

    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
            
            std::string droppedPath = (const char*)payload->Data;
            std::filesystem::path fsPath(droppedPath);
            
            std::string ext = fsPath.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            
            if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga") {
                nlohmann::json initialState = Serialize();
                SetTexture(droppedPath); // Will automatically find the new UUID
                CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, initialState, Serialize()));
            }
        }
        ImGui::EndDragDropTarget();
    }
    
    ImGui::TextDisabled("Press ENTER to load new texture");

    if (m_texture.id != 0) {
        ImGui::Text("Resolution: %d x %d", m_texture.width, m_texture.height);
        ImGui::TextDisabled("UUID: %llu", (uint64_t)m_textureUUID); // Helpful debug info
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "No texture loaded!");
    }
}
#endif

nlohmann::json SpriteRenderer::Serialize() const {
    return {
        {"type", "SpriteRenderer"},
        {"textureUUID", (uint64_t)m_textureUUID}
    };
}

void SpriteRenderer::Deserialize(const nlohmann::json& j) {
    // Backward compatibility: If the scene is old and uses texturePath, convert it on load!
    if (j.contains("textureUUID")) {
        SetTexture(UUID(j["textureUUID"].get<uint64_t>()));
    } 
    else if (j.contains("texturePath")) {
        SetTexture(j["texturePath"].get<std::string>());
    }
}
