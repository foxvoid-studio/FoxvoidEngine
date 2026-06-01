#include "ImageRenderer.hpp"
#include "ui/core/RectTransform.hpp"
#include "math/Transform2d.hpp"
#include "core/assets/AssetManager.hpp"
#include "core/assets/AssetRegistry.hpp"
#include <iostream>
#include <filesystem>

#ifndef STANDALONE_MODE
#include "editor/EditorUI.hpp"
#include "editor/commands/CommandHistory.hpp"
#include "editor/commands/ModifyComponentCommand.hpp"
#endif

ImageRenderer::ImageRenderer(const std::string& texturePath) {
    m_texture.id = 0;
    if (!texturePath.empty()) {
        SetTexture(texturePath);
    }
}

ImageRenderer::~ImageRenderer() {
    // Note: We do NOT call UnloadTexture here.
    // The AssetManager handles the lifecycle of textures to prevent 
    // unloading a texture that is still being used by another component.
}

void ImageRenderer::SetTexture(const std::string& path) {
    if (path.empty()) {
        SetTexture(UUID(0));
        return;
    }
    // Convert the string path to a safe UUID using the registry
    SetTexture(AssetRegistry::GetUUIDForPath(path));
}

void ImageRenderer::SetTexture(UUID uuid) {
    m_textureUUID = uuid;
    
    if (m_textureUUID != 0) {
        // Resolve the UUID back to its current physical path
        std::string resolvedPath = AssetRegistry::GetPathForUUID(m_textureUUID).string();
        
        if (!resolvedPath.empty()) {
            // Ask the AssetManager for the texture (it will load it if not cached)
            m_texture = AssetManager::GetTexture(resolvedPath);
        } else {
            std::cerr << "[ImageRenderer] Error: Could not resolve UUID " << (uint64_t)m_textureUUID << std::endl;
        }
    } else {
        // Clear the texture if UUID is 0
        m_texture.id = 0;
    }
}

void ImageRenderer::Render() {
    // Skip rendering if marked as HUD, if there's no owner, or no valid texture
    if (isHUD || !owner || m_texture.id == 0) return;

    // Draw in world space using Legacy Transform2d
    if (Transform2d* transform = owner->GetComponent<Transform2d>()) {
        Rectangle sourceRec = { 0.0f, 0.0f, (float)m_texture.width, (float)m_texture.height };
        auto position = transform->GetGlobalPosition();
        
        Rectangle destRec = {
            position.x, 
            position.y,
            m_texture.width * transform->scale.x,
            m_texture.height * transform->scale.y
        };
        
        Vector2 origin = { destRec.width / 2.0f, destRec.height / 2.0f };
        DrawTexturePro(m_texture, sourceRec, destRec, origin, transform->rotation, color);
    }
}

void ImageRenderer::RenderHUD() {
    if (!isHUD || !owner || m_texture.id == 0) return;

    // --- MODERN UI RENDERING (Preferred) ---
    if (RectTransform* rectTransform = owner->GetComponent<RectTransform>()) {
        // Get the absolute screen rectangle calculated by the anchor/pivot system
        Rectangle destRec = rectTransform->GetScreenRect();
        Rectangle sourceRec = { 0.0f, 0.0f, (float)m_texture.width, (float)m_texture.height };
        
        // For UI, the drawing origin is always top-left (0,0) because the RectTransform 
        // has already handled the complex math for Anchors and Pivots!
        DrawTexturePro(m_texture, sourceRec, destRec, {0.0f, 0.0f}, 0.0f, color);
        return;
    }

    // --- FALLBACK RENDERING (Legacy scenes) ---
    if (Transform2d* transform = owner->GetComponent<Transform2d>()) {
        Rectangle sourceRec = { 0.0f, 0.0f, (float)m_texture.width, (float)m_texture.height };
        auto position = transform->GetGlobalPosition();
        
        Rectangle destRec = {
            position.x, 
            position.y,
            m_texture.width * transform->scale.x,
            m_texture.height * transform->scale.y
        };
        
        Vector2 origin = { destRec.width / 2.0f, destRec.height / 2.0f };
        DrawTexturePro(m_texture, sourceRec, destRec, origin, transform->rotation, color);
    }
}

std::string ImageRenderer::GetName() const { 
    return "Image Renderer"; 
}

#ifndef STANDALONE_MODE
void ImageRenderer::OnInspector() {
    std::string currentPath = "";
    if (m_textureUUID != 0) {
        currentPath = AssetRegistry::GetPathForUUID(m_textureUUID).string();
    }

    char buffer[256];
    strncpy(buffer, currentPath.c_str(), sizeof(buffer));
    
    // Manual path input field
    if (ImGui::InputText("Texture Path", buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
        if (std::string(buffer) != currentPath) {
            nlohmann::json stateBefore = Serialize();
            SetTexture(std::string(buffer));
            CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, stateBefore, Serialize()));
        }
    }

    // Drag and Drop target for image files from the Content Browser
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
            std::string droppedPath = (const char*)payload->Data;
            std::filesystem::path fsPath(droppedPath);
            
            // Convert extension to lowercase for safe comparison
            std::string ext = fsPath.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            
            if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga") {
                nlohmann::json stateBefore = Serialize();
                SetTexture(droppedPath);
                CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, stateBefore, Serialize()));
            }
        }
        ImGui::EndDragDropTarget();
    }

    // Visual feedback and preview
    if (m_texture.id != 0) {
        ImGui::TextDisabled("Resolution: %d x %d", m_texture.width, m_texture.height);
        
        // Calculate a responsive preview height while maintaining aspect ratio
        float availWidth = ImGui::GetContentRegionAvail().x;
        float aspect = (float)m_texture.height / (float)m_texture.width;
        float previewHeight = availWidth * aspect;
        
        // Cap the height so tall images don't break the inspector layout
        if (previewHeight > 150.0f) {
            previewHeight = 150.0f;
            availWidth = previewHeight / aspect;
        }
        
        ImGui::Image((ImTextureID)(uintptr_t)m_texture.id, ImVec2(availWidth, previewHeight));
        
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "No texture loaded!");
    }

    ImGui::Separator();
    
    // UI specific settings
    EditorUI::ColorEdit4("Tint Color", &color, this);
    EditorUI::Checkbox("Is HUD (Screen Space)", &isHUD, this);
}
#endif

nlohmann::json ImageRenderer::Serialize() const {
    return {
        {"type", "ImageRenderer"},
        {"textureUUID", (uint64_t)m_textureUUID},
        {"isHUD", isHUD},
        {"color", {color.r, color.g, color.b, color.a}}
    };
}

void ImageRenderer::Deserialize(const nlohmann::json& j) {
    // Backward compatibility for texture paths
    if (j.contains("textureUUID")) {
        SetTexture(UUID(j["textureUUID"].get<uint64_t>()));
    } else if (j.contains("texturePath")) {
        SetTexture(j["texturePath"].get<std::string>());
    }
    
    isHUD = j.value("isHUD", true);
    
    // Safely extract the color array
    if (j.contains("color") && j["color"].is_array() && j["color"].size() == 4) {
        color.r = j["color"][0]; 
        color.g = j["color"][1]; 
        color.b = j["color"][2]; 
        color.a = j["color"][3];
    }
}
