#include "SpriteSheetRenderer.hpp"
#include "scene/GameObject.hpp"
#include "math/Transform2d.hpp"
#include "graphics/Graphics.hpp"
#include <iostream>
#include <cstring>
#include "core/assets/AssetManager.hpp"
#include "core/assets/AssetRegistry.hpp"
#include <filesystem>

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

// Adds the requested tint color
void SpriteSheetRenderer::SetTint(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    m_tint = { r, g, b, a };
}

// Quickly modifies only the alpha channel, keeping the RGB values intact
void SpriteSheetRenderer::SetOpacity(float alpha) {
    // Clamp between 0.0 and 1.0, then convert to 0-255 byte
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    m_tint.a = static_cast<unsigned char>(alpha * 255.0f);
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
    // Early exit if the component is disabled or the sprite is set to invisible
    if (!m_isVisible || !m_transform) return;

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
    DrawTexturePro(m_texture, drawRec, destRec, origin, m_transform->rotation, m_tint);
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

    ImGui::Separator();

    // Visibility Checkbox
    bool visible = m_isVisible;
    if (ImGui::Checkbox("Visible", &visible)) {
        nlohmann::json initialState = Serialize();
        m_isVisible = visible;
        CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, initialState, Serialize()));
    }

    // Color Picker (Converts Raylib 0-255 to ImGui 0.0-1.0 floats)
    float color[4] = { m_tint.r / 255.0f, m_tint.g / 255.0f, m_tint.b / 255.0f, m_tint.a / 255.0f };
    if (ImGui::ColorEdit4("Tint Color", color)) {
        nlohmann::json initialState = Serialize();
        m_tint.r = static_cast<unsigned char>(color[0] * 255.0f);
        m_tint.g = static_cast<unsigned char>(color[1] * 255.0f);
        m_tint.b = static_cast<unsigned char>(color[2] * 255.0f);
        m_tint.a = static_cast<unsigned char>(color[3] * 255.0f);
        
        // Note: ColorEdit4 updates every frame while dragging, which floods the Undo history. 
        // If ImGui::IsItemDeactivatedAfterEdit() is true, that's usually the best time to save the command!
        if (ImGui::IsItemDeactivatedAfterEdit()) {
             CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, initialState, Serialize()));
        }
    }
}
#endif

nlohmann::json SpriteSheetRenderer::Serialize() const {
    return {
        {"type", "SpriteSheetRenderer"},
        {"textureUUID", (uint64_t)m_textureUUID},
        {"columns", m_columns},
        {"rows", m_rows},
        {"isVisible", m_isVisible},
        {"tint", {m_tint.r, m_tint.g, m_tint.b, m_tint.a}}
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

    m_isVisible = j.value("isVisible", true); // Default to true if not found
    
    if (j.contains("tint") && j["tint"].is_array() && j["tint"].size() == 4) {
        m_tint.r = j["tint"][0].get<unsigned char>();
        m_tint.g = j["tint"][1].get<unsigned char>();
        m_tint.b = j["tint"][2].get<unsigned char>();
        m_tint.a = j["tint"][3].get<unsigned char>();
    }
}
