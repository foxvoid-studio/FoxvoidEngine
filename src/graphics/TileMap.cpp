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
    : tileSize { 32.0f, 32.0f }, tileSpacing(0), gridWidth(10), gridHeight(10)
{
    // By default, create a single base layer
    AddLayer("Background");
}

TileMap::~TileMap() {
    ClearTilesets();
}

void TileMap::ClearTilesets() {
    for (auto& ts : m_tilesets) {
        if (ts.texture.id != 0) {
            UnloadTexture(ts.texture);
        }
    }
    m_tilesets.clear();
}

std::string TileMap::GetName() const {
    return "Tile Map";
}

void TileMap::AddTileset(UUID uuid) {
    if (uuid == 0) return;

    // Prevent adding the exact same tileset twice
    for (const auto& existingTs : m_tilesets) {
        if (existingTs.uuid == uuid) return;
    }

    std::string path = AssetRegistry::GetPathForUUID(uuid).string();
    if (path.empty()) return;

    Texture2D tex = Graphics::LoadTextureFiltered(path);
    if (tex.id == 0) return;

    int stepX = (int)tileSize.x + tileSpacing;
    int stepY = (int)tileSize.y + tileSpacing;
    if (stepX <= 0 || stepY <= 0) return;

    Tileset ts;
    ts.uuid = uuid;
    ts.texture = tex;
    ts.name = std::filesystem::path(path).stem().string();
    
    // Cache the grid dimensions of this specific texture
    ts.columns = (tex.width + tileSpacing) / stepX;
    int rows = (tex.height + tileSpacing) / stepY;
    ts.tileCount = ts.columns * rows;

    // Calculate the Global ID offset for this tileset
    if (m_tilesets.empty()) {
        ts.firstTileID = 0;
    } else {
        const auto& lastTs = m_tilesets.back();
        ts.firstTileID = lastTs.firstTileID + lastTs.tileCount;
    }

    m_tilesets.push_back(ts);
}

void TileMap::AddTileset(const std::string& path) {
    if (path.empty()) return;
    
    // Resolve the path to a UUID and call the main method
    UUID assetId = AssetRegistry::GetUUIDForPath(path);
    AddTileset(assetId);
}

void TileMap::Render() {
    if (m_tilesets.empty() || m_layers.empty() || !owner) return;

    auto transform = owner->GetComponent<Transform2d>();
    if (!transform) return;

    int stepX = (int)tileSize.x + tileSpacing;
    int stepY = (int)tileSize.y + tileSpacing;
    if (stepX <= 0 || stepY <= 0) return;

    // Loop through all layers from bottom to top
    for (const auto& layer : m_layers) {
        if (!layer.isVisible) continue;

        // OPTIMIZATION: Group Draw Calls by Texture!
        // We iterate over the tilesets FIRST. Raylib will auto-batch all DrawTexturePro 
        // calls into a single OpenGL draw call as long as the texture doesn't change.
        for (const auto& tileset : m_tilesets) {
            if (tileset.texture.id == 0 || tileset.columns <= 0) continue;

            for (int y = 0; y < gridHeight; ++y) {
                for (int x = 0; x < gridWidth; ++x) {
                    int index = y * gridWidth + x;
                    if (index >= layer.data.size()) continue;
                    
                    int globalID = layer.data[index];
                    
                    // Skip empty tiles OR tiles that don't belong to the CURRENT tileset
                    if (globalID < tileset.firstTileID || globalID >= tileset.firstTileID + tileset.tileCount) {
                        continue; 
                    }

                    // Convert Global ID back to Local ID for this specific texture
                    int localID = globalID - tileset.firstTileID;

                    float srcX = (float)((localID % tileset.columns) * stepX);
                    float srcY = (float)((localID / tileset.columns) * stepY);

                    Rectangle srcRec = { srcX, srcY, tileSize.x, tileSize.y };

                    auto position = transform->GetGlobalPosition();
                    float dstX = std::round(position.x + (x * tileSize.x * transform->scale.x));
                    float dstY = std::round(position.y + (y * tileSize.y * transform->scale.y));
                    
                    float dstWidth = std::ceil(tileSize.x * transform->scale.x);
                    float dstHeight = std::ceil(tileSize.y * transform->scale.y);
                    
                    Rectangle dstRec = { dstX, dstY, dstWidth, dstHeight };
                    Vector2 origin = { 0.0f, 0.0f };

                    DrawTexturePro(tileset.texture, srcRec, dstRec, origin, transform->rotation, WHITE);
                }
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
        
        ImGui::Spacing();
        ImGui::SeparatorText("Tilesets");

        // Display loaded tilesets
        for (size_t i = 0; i < m_tilesets.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            const auto& ts = m_tilesets[i];
            
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "%s", ts.name.c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("(IDs: %d - %d)", ts.firstTileID, ts.firstTileID + ts.tileCount - 1);
            
            // To properly remove a tileset in the future, you'd need to shift all subsequent IDs in the layers.
            // For now, displaying them is the priority.
            
            ImGui::PopID();
        }

        ImGui::Spacing();

        // Drop zone for new tilesets
        ImGui::Button("Drop new Tileset Image here", ImVec2(-1, 30));
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                std::string droppedPath = (const char*)payload->Data;
                std::filesystem::path fsPath(droppedPath);
                std::string ext = fsPath.extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                
                if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp") {
                    nlohmann::json initialState = Serialize();
                    
                    AddTileset(AssetRegistry::GetUUIDForPath(droppedPath));
                    
                    CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, initialState, Serialize()));
                }
            }
            ImGui::EndDragDropTarget();
        }
    }

    // Layers management
    if (ImGui::CollapsingHeader("Layers", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button("Add Layer")) {
            AddLayer("Layer " + std::to_string(m_layers.size()));
        }

        for (size_t i = 0; i < m_layers.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));

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
    j["tileSize"] = { tileSize.x, tileSize.y };
    j["tileSpacing"] = tileSpacing;
    j["gridWidth"] = gridWidth;
    j["gridHeight"] = gridHeight;
    j["showGrid"] = showGrid;

    // Save all tilesets
    nlohmann::json tilesetsArray = nlohmann::json::array();
    for (const auto& ts : m_tilesets) {
        tilesetsArray.push_back((uint64_t)ts.uuid);
    }
    j["tilesets"] = tilesetsArray;

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
    if (j.contains("tileSize") && j["tileSize"].is_array() && j["tileSize"].size() == 2) {
        tileSize.x = j["tileSize"][0];
        tileSize.y = j["tileSize"][1];
    } else {
        tileSize.x = 32.0f; tileSize.y = 32.0f;
    }

    tileSpacing = j.value("tileSpacing", 0);
    gridWidth = j.value("gridWidth", 10);
    gridHeight = j.value("gridHeight", 10);
    showGrid = j.value("showGrid", true);
    
    ClearTilesets();

    // Backward compatibility for old single-tileset maps
    if (j.contains("tilesetUUID")) {
        AddTileset(UUID(j["tilesetUUID"].get<uint64_t>()));
    } 
    // New multi-tileset loading
    else if (j.contains("tilesets") && j["tilesets"].is_array()) {
        for (const auto& tsUUID : j["tilesets"]) {
            AddTileset(UUID(tsUUID.get<uint64_t>()));
        }
    }

    m_layers.clear();
    if (j.contains("layers")) {
        for (const auto& layerJson : j["layers"]) {
            TileLayer layer(layerJson.value("name", "Layer"), gridWidth, gridHeight);
            layer.isVisible = layerJson.value("isVisible", true);
            layer.isSolid = layerJson.value("isSolid", false);
            
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
