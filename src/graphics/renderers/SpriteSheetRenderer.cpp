#include "SpriteSheetRenderer.hpp"
#include "scene/GameObject.hpp"
#include "math/Transform2d.hpp"
#include "graphics/Graphics.hpp"
#include <iostream>
#include <cstring>
#include "core/AssetManager.hpp"
#include <filesystem>
#include "core/AssetRegistry.hpp"

SpriteSheetRenderer::SpriteSheetRenderer(const std::string& texturePath, int columns, int rows)
    : m_columns(columns), m_rows(rows), m_currentFrame(0), m_transform(nullptr) 
{
    m_texture.id = 0;

    if (!texturePath.empty()) {
        SetTexture(texturePath);
    }
}

SpriteSheetRenderer::~SpriteSheetRenderer() {}

void SpriteSheetRenderer::SetTexture(const std::string& path) {
    if (path.empty()) {
        SetTexture(UUID(0));
        return;
    }
    
    // Interrogate the registry to find the unique ID of this file
    UUID assetId = AssetRegistry::GetUUIDForPath(path);
    SetTexture(assetId);
}

void SpriteSheetRenderer::SetTexture(UUID uuid) {
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

void SpriteSheetRenderer::Start() {
    m_transform = owner->GetComponent<Transform2d>();
    if (!m_transform) {
        std::cerr << "[SpriteSheetRenderer] Warning: No Transform2d found!" << std::endl;
    }
}

void SpriteSheetRenderer::SetFrame(int frameIndex) {
    // Safety check to prevent drawing out of bounds
    int maxFrames = m_columns * m_rows;
    if (frameIndex >= 0 && frameIndex < maxFrames) {
        m_currentFrame = frameIndex;
    }
}

Rectangle SpriteSheetRenderer::GetSourceRec() const {
    // Prevent division by zero just in case
    int safeCols = (m_columns > 0) ? m_columns : 1;
    int safeRows = (m_rows > 0) ? m_rows : 1;

    // Calculate the physical pixel size of a single frame
    float frameWidth = static_cast<float>(m_texture.width) / safeCols;
    float frameHeight = static_cast<float>(m_texture.height) / safeRows;

    // Math magic: convert a 1D index into 2D grid coordinates
    int gridX = m_currentFrame % safeCols;
    int gridY = m_currentFrame / safeCols;

    // Return the subset rectangle of the texture to draw
    return Rectangle{
        gridX * frameWidth,
        gridY * frameHeight,
        frameWidth,
        frameHeight
    };
}

void SpriteSheetRenderer::Render() {
    if (m_transform) {
        // Source Rectangle: The specific frame from the spritesheet
        Rectangle sourceRec = GetSourceRec();

        // Destination Rectangle: Position and scaled size of that single frame
        auto position = m_transform->GetGlobalPosition();
        Rectangle destRec = {
            position.x,
            position.y,
            sourceRec.width * m_transform->scale.x,
            sourceRec.height * m_transform->scale.y
        };

        // Origin: Center of the scaled frame
        Vector2 origin = { destRec.width / 2.0f, destRec.height / 2.0f };

        // Make a temporary copy of the source rectangle for this specific frame
        Rectangle drawRec = sourceRec;

        // Force the absolute value, then apply the flip sign
        drawRec.width = std::abs(drawRec.width) * (flipX ? -1.0f : 1.0f);
        drawRec.height = std::abs(drawRec.height) * (flipY ? -1.0f : 1.0f);

        // Draw with full transform support
        DrawTexturePro(m_texture, drawRec, destRec, origin, m_transform->rotation, WHITE);
    }
}

std::string SpriteSheetRenderer::GetName() const {
    return "SpriteSheet Renderer";
}

#ifndef STANDALONE_MODE
void SpriteSheetRenderer::OnInspector() {
    // Dynamically fetch the current path from the registry for the UI
    std::string currentPath = "";
    if (m_textureUUID != 0) {
        currentPath = AssetRegistry::GetPathForUUID(m_textureUUID).string();
    }

    // Texture Path Input
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

    // Drag and drop target
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

    // Grid Dimensions (Columns and Rows)
    if (EditorUI::DragInt("Columns", &m_columns, 0.1f, this, 1, 64)) {
        m_currentFrame = 0; 
    }
    if (EditorUI::DragInt("Rows", &m_rows, 0.1f, this, 1, 64)) {
        m_currentFrame = 0;
    }

    // 3. Debug frame viewer (Read Only in Editor)
    ImGui::Text("Current Frame: %d / %d", m_currentFrame, (m_columns * m_rows) - 1);
    
    // Show useful debug info
    if (m_texture.id != 0) {
        ImGui::TextDisabled("UUID: %llu", (uint64_t)m_textureUUID);
    }
}
#endif

nlohmann::json SpriteSheetRenderer::Serialize() const {
    return {
        {"type", "SpriteSheetRenderer"},
        {"textureUUID", (uint64_t)m_textureUUID},
        {"columns", m_columns},
        {"rows", m_rows}
    };
}

void SpriteSheetRenderer::Deserialize(const nlohmann::json& j) {
    m_columns = j.value("columns", 1);
    m_rows = j.value("rows", 1);
    m_currentFrame = 0; // Always reset frame on load

    // Backward compatibility: If the scene is old and uses texturePath, convert it on load!
    if (j.contains("textureUUID")) {
        SetTexture(UUID(j["textureUUID"].get<uint64_t>()));
    } 
    else if (j.contains("texturePath")) {
        SetTexture(j["texturePath"].get<std::string>());
    }
}
