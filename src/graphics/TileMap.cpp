#include "TileMap.hpp"
#include "physics/Transform2d.hpp"
#include "world/GameObject.hpp"
#include "graphics/Graphics.hpp"
#include <iostream>

#ifndef STANDALONE_MODE
#include "editor/EditorUI.hpp"
#include <imgui.h>
#endif
#include <core/AssetRegistry.hpp>

TileMap::TileMap()
    : tileSize { 32.0f, 32.0f }, tileSpacing(0), gridWidth(10), gridHeight(10), m_tilesetTexture{0}
{
    // By default, create a single base layer
    AddLayer("Background");
}

TileMap::~TileMap() {
    if (m_tilesetTexture.id != 0) {
        UnloadTexture(m_tilesetTexture);
    }
}

std::string TileMap::GetName() const {
    return "Tile Map";
}

void TileMap::LoadTileset(const std::string& path) {
    if (path.empty()) {
        LoadTileset(UUID(0));
        return;
    }
    
    // Resolve UI path to UUID
    UUID assetId = AssetRegistry::GetUUIDForPath(path);
    LoadTileset(assetId);
}

void TileMap::LoadTileset(UUID uuid) {
    m_tilesetUUID = uuid;

    if (m_tilesetUUID != 0) {
        std::string resolvedPath = AssetRegistry::GetPathForUUID(m_tilesetUUID).string();
        
        if (!resolvedPath.empty()) {
            // We use Graphics::LoadTextureFiltered to ensure the tilemap renders correctly without blur
            m_tilesetTexture = Graphics::LoadTextureFiltered(resolvedPath);
        } else {
            std::cerr << "[TileMap] Error: Could not resolve UUID " << (uint64_t)m_tilesetUUID << " to a valid path!" << std::endl;
            m_tilesetTexture.id = 0;
        }
    }
    else {
        // If the UUID is 0 (cleared), just clear the local struct to stop rendering
        m_tilesetTexture.id = 0;
    }
}

void TileMap::Render() {
    // Safety checks
    if (m_tilesetTexture.id == 0 || m_layers.empty() || !owner) return;

    auto transform = owner->GetComponent<Transform2d>();
    if (!transform) return;

    int stepX = (int)tileSize.x + tileSpacing;
    int stepY = (int)tileSize.y + tileSpacing;

    // Safety check: Prevent division by zero
    if (stepX <= 0 || stepY <= 0) return;

    // Calculate how many columns the tileset texture has
    // +tileSpacing handles the edge case where the right/bottom edge of the image doesn't have a final padding
    int tilesetCols = (m_tilesetTexture.width + tileSpacing) / stepX;
    if (tilesetCols == 0) return; // Prevent division by zero if texture is too small

    // Loop through all layers from bottom to top
    for (const auto& layer : m_layers) {
        if (!layer.isVisible) continue;

        // Loop through the grid
        for (int y = 0; y < gridHeight; ++y) {
            for (int x = 0; x < gridWidth; ++x) {
                int index = y * gridWidth + x;

                if (index >= layer.data.size()) continue;
                
                int tileID = layer.data[index];
                if (tileID < 0) continue; // -1 represents an empty (transparent) tile

                // Calculate where to read the tile from the texture (Source Rectangle) using the step sizes
                float srcX = (float)((tileID % tilesetCols) * stepX);
                float srcY = (float)((tileID / tilesetCols) * stepY);

                Rectangle srcRec = { srcX, srcY, tileSize.x, tileSize.y };

                // Calculate where to draw the tile in the world (Destination Rectangle)
                // We round the coordinates to avoid sub-pixel positioning
                auto position = transform->GetGlobalPosition();
                float dstX = std::round(position.x + (x * tileSize.x * transform->scale.x));
                float dstY = std::round(position.y + (y * tileSize.y * transform->scale.y));
                
                // We use ceil to slightly force the width/height to the upper pixel
                // This completely eliminates floating-point hairline cracks between tiles
                float dstWidth = std::ceil(tileSize.x * transform->scale.x);
                float dstHeight = std::ceil(tileSize.y * transform->scale.y);
                
                Rectangle dstRec = { dstX, dstY, dstWidth, dstHeight };

                // We don't want each individual tile to rotate around its own center, 
                // so origin is 0,0. (For full map rotation, logic would be more complex)
                Vector2 origin = { 0.0f, 0.0f };

                DrawTexturePro(m_tilesetTexture, srcRec, dstRec, origin, transform->rotation, WHITE);
            }
        }
    }
}

#ifndef STANDALONE_MODE
void TileMap::OnInspector() {
    // Grid setup
    if (ImGui::CollapsingHeader("Map Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Show Tile Grid", &showGrid);
        
        EditorUI::DragFloat2("Tile Size", &tileSize.x, 1.0f, this, 1.0f, 256.0f);
        EditorUI::DragInt("Tile Spacing", &tileSpacing, 1, this, 0, 64);

        // Grid width
        // Static variable to hold the temporary value while the user is dragging the mouse.
        // This prevents the map from resizing (and destroying data) 60 times a second.
        static int tempWidth = gridWidth;
        
        // Draw the interactive UI element using the temporary variable
        ImGui::DragInt("Grid Width", &tempWidth, 1, 1, 1000);
        
        // Trigger the heavy map resize ONLY when the mouse click is released (or Enter is pressed)
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            if (tempWidth != gridWidth) {
                // Save the current state before modifying it for the Undo/Redo history
                nlohmann::json initialState = Serialize();
                
                ResizeMap(tempWidth, gridHeight);
                
                // Commit the action to the command history
                CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, initialState, Serialize()));
            }
        }
        
        // Resynchronize the displayed UI value with the actual map value
        // ONLY if the user is not currently clicking/dragging the widget.
        // This ensures external resizes (like pressing Ctrl+Z) update the slider correctly.
        if (!ImGui::IsItemActive()) {
            tempWidth = gridWidth;
        }

        // Grid height
        static int tempHeight = gridHeight;
        ImGui::DragInt("Grid Height", &tempHeight, 1, 1, 1000);
        
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            if (tempHeight != gridHeight) {
                nlohmann::json initialState = Serialize();
                
                ResizeMap(gridWidth, tempHeight);
                
                CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, initialState, Serialize()));
            }
        }
        
        if (!ImGui::IsItemActive()) {
            tempHeight = gridHeight;
        }
        
        // Dynamically fetch the current path from the registry for the UI
        std::string currentPath = "";
        if (m_tilesetUUID != 0) {
            currentPath = AssetRegistry::GetPathForUUID(m_tilesetUUID).string();
        }
        
        static char pathBuffer[256];
        strncpy(pathBuffer, currentPath.c_str(), sizeof(pathBuffer));
        pathBuffer[sizeof(pathBuffer) - 1] = '\0';
        
        if (ImGui::InputText("Tileset Path", pathBuffer, sizeof(pathBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
            std::string newPath(pathBuffer);
            if (newPath != currentPath) {
                nlohmann::json initialState = Serialize();
                LoadTileset(newPath); // Will translate path to UUID
                CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, initialState, Serialize()));
            }
        }

        // Drag and Drop support for the tileset path
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                std::string droppedPath = (const char*)payload->Data;
                std::filesystem::path fsPath(droppedPath);
                std::string ext = fsPath.extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                
                if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp") {
                    nlohmann::json initialState = Serialize();
                    LoadTileset(droppedPath);
                    CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, initialState, Serialize()));
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (m_tilesetUUID != 0) {
            ImGui::TextDisabled("UUID: %llu", (uint64_t)m_tilesetUUID);
        }
    }

    // Layers management
    if (ImGui::CollapsingHeader("Layers", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button("Add Layer")) {
            AddLayer("Layer " + std::to_string(m_layers.size()));
        }

        for (size_t i = 0; i < m_layers.size(); ++i) {
            ImGui::PushID(i);

            // UI layout for Layer controls
            ImGui::Checkbox("Vis", &m_layers[i].isVisible);
            ImGui::SameLine();

            ImGui::Checkbox("Solid", &m_layers[i].isSolid);
            ImGui::SameLine();
            
            // Allow renaming the layer
            char nameBuffer[64];
            strncpy(nameBuffer, m_layers[i].name.c_str(), sizeof(nameBuffer));
            if (ImGui::InputText("##name", nameBuffer, sizeof(nameBuffer))) {
                m_layers[i].name = nameBuffer;
            }
            ImGui::PopID();
        }
    }
}
#endif

void TileMap::AddLayer(const std::string& name) {
    m_layers.emplace_back(name, gridWidth, gridHeight);
}

bool TileMap::IsInBounds(int x, int y) const {
    return (x >= 0 && x < gridWidth && y >= 0 && y < gridHeight);
}

int TileMap::GetTile(int layerIndex, int x, int y) const {
    if (layerIndex < 0 || layerIndex >= m_layers.size() || !IsInBounds(x, y)) return -1;
    
    return m_layers[layerIndex].data[y * gridWidth + x];
}

void TileMap::SetTile(int layerIndex, int x, int y, int tileID) {
    if (layerIndex < 0 || layerIndex >= m_layers.size() || !IsInBounds(x, y)) return;
    
    m_layers[layerIndex].data[y * gridWidth + x] = tileID;
}

void TileMap::ResizeMap(int newWidth, int newHeight) {
    if (newWidth <= 0 || newHeight <= 0) return;

    // For every layer, we need to create a new buffer and copy the old data over
    // while respecting the 2D grid layout (we can't just standard resize the 1D vector)
    for (auto& layer : m_layers) {
        std::vector<int> newData(newWidth * newHeight, -1);

        for (int y = 0; y < std::min(gridHeight, newHeight); ++y) {
            for (int x = 0; x < std::min(gridWidth, newWidth); ++x) {
                newData[y * newWidth + x] = layer.data[y * gridWidth + x];
            }
        }
        layer.data = std::move(newData);
    }

    gridWidth = newWidth;
    gridHeight = newHeight;
}

nlohmann::json TileMap::Serialize() const {
    nlohmann::json j;
    j["type"] = "TileMap";
    j["tilesetUUID"] = (uint64_t)m_tilesetUUID;
    j["tileSize"] = { tileSize.x, tileSize.y };
    j["tileSpacing"] = tileSpacing;
    j["gridWidth"] = gridWidth;
    j["gridHeight"] = gridHeight;
    j["showGrid"] = showGrid;

    nlohmann::json layersArray = nlohmann::json::array();
    for (const auto& layer : m_layers) {
        layersArray.push_back({
            {"name", layer.name},
            {"isVisible", layer.isVisible},
            {"isSolid", layer.isSolid},
            {"data", layer.data}
        });
    }
    j["layers"] = layersArray;

    return j;
}

void TileMap::Deserialize(const nlohmann::json& j) {
    // Safely extract the tileSize array
    if (j.contains("tileSize") && j["tileSize"].is_array() && j["tileSize"].size() == 2) {
        tileSize.x = j["tileSize"][0];
        tileSize.y = j["tileSize"][1];
    } else {
        // Fallback defaults
        tileSize.x = 32.0f;
        tileSize.y = 32.0f;
    }

    tileSpacing = j.value("tileSpacing", 0);

    // Extract standard values using correct default fallbacks
    gridWidth = j.value("gridWidth", 10);
    gridHeight = j.value("gridHeight", 10);

    showGrid = j.value("showGrid", true);
    
    // Backward compatibility for loading the texture
    if (j.contains("tilesetUUID")) {
        LoadTileset(UUID(j["tilesetUUID"].get<uint64_t>()));
    } 
    else if (j.contains("tilesetPath")) {
        LoadTileset(j["tilesetPath"].get<std::string>());
    }

    m_layers.clear();
    if (j.contains("layers")) {
        for (const auto& layerJson : j["layers"]) {
            TileLayer layer(layerJson.value("name", "Layer"), gridWidth, gridHeight);
            layer.isVisible = layerJson.value("isVisible", true);
            layer.isSolid = layerJson.value("isSolid", false);
            
            // Safely copy the JSON array into the vector
            if (layerJson.contains("data") && layerJson["data"].is_array()) {
                layer.data = layerJson["data"].get<std::vector<int>>();
            }
            m_layers.push_back(layer);
        }
    }
}

std::vector<int> TileMap::GetLayerData(int layerIndex) const {
    if (layerIndex >= 0 && layerIndex < m_layers.size()) {
        return m_layers[layerIndex].data;
    }
    return {};
}

void TileMap::SetLayerData(int layerIndex, const std::vector<int>& data) {
    if (layerIndex >= 0 && layerIndex < m_layers.size()) {
        m_layers[layerIndex].data = data;
    }
}

std::vector<Rectangle> TileMap::GetCollisionRects() const {
    std::vector<Rectangle> rects;

    if (!owner) return rects;
    auto transform = owner->GetComponent<Transform2d>();
    if (!transform) return rects;

    for (const auto& layer : m_layers) {
        // Skip layers that don't have physics enabled
        if (!layer.isSolid) continue;

        for (int y = 0; y < gridHeight; ++y) {
            for (int x = 0; x < gridWidth; ++x) {
                int index = y * gridWidth + x;

                // Safety check: Prevent Buffer Overrun
                if (index >= layer.data.size()) continue;

                int tileID = layer.data[index];
                if (tileID < 0) continue; // Skip empty tiles

                // Calculate the world-space bounding box of this specific tile
                auto position = transform->GetGlobalPosition();
                float dstX = position.x + (x * tileSize.x * transform->scale.x);
                float dstY = position.y + (y * tileSize.y * transform->scale.y);
                float dstWidth = tileSize.x * transform->scale.x;
                float dstHeight = tileSize.y * transform->scale.y;

                rects.push_back({ dstX, dstY, dstWidth, dstHeight });
            }
        }
    }

    return rects;
}

void TileMap::RenderGrid() const {
    if (!showGrid || !owner) return;

    auto transform = owner->GetComponent<Transform2d>();
    if (!transform) return;

    // Calculate dimensions taking scale into account
    float scaledWidth = tileSize.x * transform->scale.x;
    float scaledHeight = tileSize.y * transform->scale.y;
    float totalWidth = gridWidth * scaledWidth;
    float totalHeight = gridHeight * scaledHeight;

    // A nice semi-transparent white for the internal grid
    Color gridColor = { 255, 255, 255, 90 };

    // Draw vertical lines
    auto position = transform->GetGlobalPosition();
    for (int x = 0; x <= gridWidth; ++x) {
        float posX = position.x + (x * scaledWidth);
        DrawLineV(
            { posX, position.y },
            { posX, position.y + totalHeight },
            gridColor
        );
    }

    // Draw horizontal lines
    for (int y = 0; y <= gridHeight; ++y) {
        float posY = position.y + (y * scaledHeight);
        DrawLineV(
            { position.x, posY },
            { position.x + totalWidth, posY },
            gridColor
        );
    }

    // Draw a thicker white border to clearly show the bounds of the TileMap
    DrawRectangleLinesEx(
        { position.x, position.y, totalWidth, totalHeight },
        2.0f, 
        WHITE
    );
}

TileLayer* TileMap::GetLayer(int index) {
    if (index >= 0 && index < m_layers.size()) {
        return &m_layers[index];
    }
    return nullptr;
}

TileLayer* TileMap::GetLayer(const std::string& name) {
    for (auto& layer : m_layers) {
        if (layer.name == name) {
            return &layer;
        }
    }
    return nullptr;
}
