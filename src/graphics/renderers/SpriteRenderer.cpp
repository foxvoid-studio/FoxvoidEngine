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

void SpriteRenderer::SetTint(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    m_tint = { r, g, b, a };
}

void SpriteRenderer::SetOpacity(float alpha) {
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    m_tint.a = static_cast<unsigned char>(alpha * 255.0f);
}

void SpriteRenderer::Start() {
    // Cache the Transform2d so we don't have to search for it every single frame
    m_transform = owner->GetComponent<Transform2d>();

    if (!m_transform) {
        std::cerr << "[SpriteRenderer] Warning: No Transform2d found on GameObject!" << std::endl;
    }
}

void SpriteRenderer::Render() {
    // Early exit if the component is disabled or the sprite is invisible
    if (!m_isVisible || !m_transform) return;

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

    // 4. Draw with full transform support and the tint color
    DrawTexturePro(m_texture, sourceRec, destRec, origin, m_transform->rotation, m_tint);
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

    ImGui::Separator();
    
    // Visibility Checkbox
    bool visible = m_isVisible;
    if (ImGui::Checkbox("Visible", &visible)) {
        nlohmann::json initialState = Serialize();
        m_isVisible = visible;
        CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, initialState, Serialize()));
    }

    // Color Picker
    float color[4] = { m_tint.r / 255.0f, m_tint.g / 255.0f, m_tint.b / 255.0f, m_tint.a / 255.0f };
    if (ImGui::ColorEdit4("Tint Color", color)) {
        nlohmann::json initialState = Serialize();
        m_tint.r = static_cast<unsigned char>(color[0] * 255.0f);
        m_tint.g = static_cast<unsigned char>(color[1] * 255.0f);
        m_tint.b = static_cast<unsigned char>(color[2] * 255.0f);
        m_tint.a = static_cast<unsigned char>(color[3] * 255.0f);
        
        if (ImGui::IsItemDeactivatedAfterEdit()) {
             CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, initialState, Serialize()));
        }
    }
}
#endif

nlohmann::json SpriteRenderer::Serialize() const {
    return {
        {"type", "SpriteRenderer"},
        {"textureUUID", (uint64_t)m_textureUUID},
        {"isVisible", m_isVisible},
        {"tint", {m_tint.r, m_tint.g, m_tint.b, m_tint.a}}
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

    m_isVisible = j.value("isVisible", true);
    
    if (j.contains("tint") && j["tint"].is_array() && j["tint"].size() == 4) {
        m_tint.r = j["tint"][0].get<unsigned char>();
        m_tint.g = j["tint"][1].get<unsigned char>();
        m_tint.b = j["tint"][2].get<unsigned char>();
        m_tint.a = j["tint"][3].get<unsigned char>();
    }
}
