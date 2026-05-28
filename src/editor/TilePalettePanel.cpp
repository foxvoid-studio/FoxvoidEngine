#include "TilePalettePanel.hpp"
#include <algorithm>
#include "graphics/TileMap.hpp"

#ifndef STANDALONE_MODE
#include <imgui.h>
#include <rlImGui.h>
#include "extras/IconsFontAwesome6.h"
#endif

TilePalettePanel::TilePalettePanel() : m_zoom(2.0f), m_selectedTilesetIndex(0) {}

void TilePalettePanel::Draw(int& selectedTileID, int& selectedLayer, TileTool& activeTool, TileMap* activeTileMap) {
    if (!isOpen) return;

    if (ImGui::Begin("Tile Palette", &isOpen)) {
        // If no TileMap is selected, or it has no tilesets, stop drawing the palette
        if (!activeTileMap || activeTileMap->GetTilesets().empty()) {
            ImGui::TextDisabled("No tilesets loaded.");
            ImGui::End();
            return;
        }

        const auto& layers = activeTileMap->GetLayers();
        const auto& tilesets = activeTileMap->GetTilesets();

        // Safety clamps just in case a layer or tileset was deleted
        if (selectedLayer >= layers.size()) selectedLayer = 0;
        if (m_selectedTilesetIndex >= tilesets.size()) m_selectedTilesetIndex = 0;

        // --- Layer Selection ---
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::BeginCombo("##LayerCombo", layers.empty() ? "No Layers" : layers[selectedLayer].name.c_str())) {
            for (int i = 0; i < layers.size(); i++) {
                bool isSelected = (selectedLayer == i);
                if (ImGui::Selectable(layers[i].name.c_str(), isSelected)) {
                    selectedLayer = i;
                }
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::Separator();

        // --- Tileset Selection (New Dropdown) ---
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::BeginCombo("##TilesetCombo", tilesets[m_selectedTilesetIndex].name.c_str())) {
            for (int i = 0; i < tilesets.size(); i++) {
                bool isSelected = (m_selectedTilesetIndex == i);
                if (ImGui::Selectable(tilesets[i].name.c_str(), isSelected)) {
                    m_selectedTilesetIndex = i; 
                }
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::Separator();

        // Extract properties from the currently viewed tileset
        const auto& activeTileset = tilesets[m_selectedTilesetIndex];
        Texture2D currentTileset = activeTileset.texture;
        Vector2 tileSize = activeTileMap->tileSize;
        int tileSpacing = activeTileMap->tileSpacing;

        // Controls
        // Helper lambda to draw selectable tool buttons
        auto drawToolButton = [](const char* icon, const char* tooltip, TileTool toolType, TileTool& currentTool) {
            bool isSelected = (currentTool == toolType);
            if (isSelected) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.90f, 0.45f, 0.10f, 1.0f)); 
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.00f, 0.55f, 0.20f, 1.0f));
            }
            
            if (ImGui::Button(icon, ImVec2(32, 32))) currentTool = toolType;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tooltip);
            
            if (isSelected) ImGui::PopStyleColor(2);
        };

        // Draw tool buttons (Brush, Eraser, Bucket)
        drawToolButton(ICON_FA_PAINTBRUSH, "Brush (Paint single tiles)", TileTool::Brush, activeTool);
        ImGui::SameLine();
        drawToolButton(ICON_FA_ERASER, "Eraser (Remove tiles)", TileTool::Eraser, activeTool);
        ImGui::SameLine();
        drawToolButton(ICON_FA_FILL_DRIP, "Bucket (Flood fill area)", TileTool::Bucket, activeTool);

        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10); // Spacing
        
        // Reduce the slider width to make room for the tool buttons on the same line
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::SliderFloat("##Zoom", &m_zoom, 1.0f, 5.0f, "Zoom: %.1fx");
        ImGui::PopItemWidth();

        ImGui::Separator();

        // Display the currently selected Tile ID (Global ID)
        if (activeTool == TileTool::Eraser) {
            ImGui::TextDisabled("Tool: Eraser Active");
        } else {
            ImGui::Text("Tile (Global ID): %d", selectedTileID);
        }

        ImGui::Separator();

        // Palette display
        // We use a child window to allow scrolling if the tileset is large
        ImGui::BeginChild("PaletteRegion", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);

        // Safety check in case the texture failed to load but the tileset entry exists
        if (currentTileset.id != 0) {
            float displayWidth = currentTileset.width * m_zoom;
            float displayHeight = currentTileset.height * m_zoom;

            // Save the start position of the image in screen coordinates
            ImVec2 screenPos = ImGui::GetCursorScreenPos();

            // Draw the tileset texture
            rlImGuiImageSize(&currentTileset, (int)displayWidth, (int)displayHeight);

            // Interaction logic
            bool isHovered = ImGui::IsItemHovered();
            
            int stepPxX = (int)tileSize.x + tileSpacing;
            int stepPxY = (int)tileSize.y + tileSpacing;
            
            // Prevent division by zero
            if (stepPxX > 0 && stepPxY > 0) {
                int cols = (currentTileset.width + tileSpacing) / stepPxX;
                int rows = (currentTileset.height + tileSpacing) / stepPxY;

                ImDrawList* drawList = ImGui::GetWindowDrawList();
                ImU32 gridColor = IM_COL32(200, 200, 200, 60);

                // Draw individual tile boxes (skipping the spacing gaps)
                for (int y = 0; y < rows; y++) {
                    for (int x = 0; x < cols; x++) {
                        ImVec2 p_min = ImVec2(screenPos.x + (x * stepPxX * m_zoom), screenPos.y + (y * stepPxY * m_zoom));
                        ImVec2 p_max = ImVec2(p_min.x + (tileSize.x * m_zoom), p_min.y + (tileSize.y * m_zoom));
                        drawList->AddRect(p_min, p_max, gridColor);
                    }
                }

                // Selection Logic
                if (isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    ImVec2 mousePos = ImGui::GetMousePos();
                    
                    // Calculate local position in standard unzoomed pixels
                    int localX = (int)((mousePos.x - screenPos.x) / m_zoom);
                    int localY = (int)((mousePos.y - screenPos.y) / m_zoom);

                    int gridX = localX / stepPxX;
                    int gridY = localY / stepPxY;

                    // Modulo math to check if the click fell inside the spacing gap (dead zone)
                    int modX = localX % stepPxX;
                    int modY = localY % stepPxY;

                    // Only select the tile if we clicked inside the actual tile area
                    if (modX < (int)tileSize.x && modY < (int)tileSize.y && gridX < cols && gridY < rows) {
                        int localTileID = gridY * cols + gridX;
                        
                        // Crucial: Add the tileset offset to get the Global ID
                        selectedTileID = activeTileset.firstTileID + localTileID;
                        
                        // Automatically switch back to Brush if a tile is picked while the Eraser is active
                        if (activeTool == TileTool::Eraser) {
                            activeTool = TileTool::Brush;
                        }
                    }
                }

                // Visual feedback: Draw a thick white rectangle around the selected tile
                // ONLY if the selected Global ID belongs to the currently viewed tileset
                if (selectedTileID >= activeTileset.firstTileID && selectedTileID < activeTileset.firstTileID + activeTileset.tileCount) {
                    
                    // Convert Global ID back to Local ID to draw the highlight correctly
                    int localSelID = selectedTileID - activeTileset.firstTileID;
                    int selX = localSelID % cols;
                    int selY = localSelID / cols;

                    ImVec2 rectMin = {
                        screenPos.x + (selX * stepPxX * m_zoom),
                        screenPos.y + (selY * stepPxY * m_zoom)
                    };
                    ImVec2 rectMax = {
                        rectMin.x + (tileSize.x * m_zoom),
                        rectMin.y + (tileSize.y * m_zoom)
                    };

                    drawList->AddRect(rectMin, rectMax, IM_COL32(255, 255, 255, 255), 0.0f, 0, 2.5f);
                }
            }
        } else {
            ImGui::TextDisabled("Invalid Texture");
        }

        ImGui::EndChild();
    }

    ImGui::End();
}
