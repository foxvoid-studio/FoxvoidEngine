#pragma once

#include "world/Component.hpp"
#include "core/UUID.hpp"
#include <raylib.h>
#include <vector>
#include <string>
#include <nlohmann/json.hpp>

// Represents a single layer of tiles in the TileMap
struct TileLayer {
    std::string name;
    std::vector<int> data; // 1D array representing a 2D grid. -1 means empty tile.
    bool isVisible;
    bool isSolid;

    TileLayer(const std::string& n, int width, int height)
        : name(n), isVisible(true), isSolid(false)
    {
        // Initialize the grid with -1 (empty)
        data.resize(width * height, -1);
    }
};

// Represents a single texture atlas (Tileset) mapped to a specific range of global IDs
struct Tileset {
    std::string name;
    UUID uuid = 0;
    Texture2D texture = {0};

    int firstTileID = 0; // The starting global ID for this specific tileset
    int tileCount = 0;   // Total number of tiles in this texture
    int columns = 0;     // Cached for fast math during rendering
};

class TileMap : public Component {
    public:
        Vector2 tileSize; // Size of a single tile in pixels (e.g., 16x16, 32x32)
        int tileSpacing; // Spacing in pixels between tiles
        int gridWidth; // Number of columns in the map
        int gridHeight; // Number of rows in the map

        bool showGrid = true; // Toggle for the TileMap's specific grid

        TileMap();
        ~TileMap();

        std::string GetName() const override;

#ifndef STANDALONE_MODE
        void OnInspector() override;
#endif

        nlohmann::json Serialize() const override;
        void Deserialize(const nlohmann::json& j) override;

        // The core rendering loop for the game view
        void Render() override;

        // TileMap Operations

        // Appends a new tileset to the map and calculates its ID range
        void AddTileset(UUID uuid);

        // Adds a new tileset using a file path (Used by Python bindings)
        void AddTileset(const std::string& path);

        // Returns a read-only list of all loaded tilesets (Used by the Palette Panel)
        const std::vector<Tileset>& GetTilesets() const { return m_tilesets; }
        
        // Clears all tilesets and unloads textures
        void ClearTilesets();

        // Safely resizes the map without losing existing tile data
        void ResizeMap(int newWidth, int newHeight);

        // Adds a new empty layer on top
        void AddLayer(const std::string& name);

        // Gets a tile ID from a specific layer (-1 if out of bounds)
        int GetTile(int layerIndex, int x, int y) const;

        // Sets a tile ID on a specific layer
        void SetTile(int layerIndex, int x, int y, int tileID);

        // Allows commands to save and restore full layer states
        std::vector<int> GetLayerData(int layerIndex) const;
        void SetLayerData(int layerIndex, const std::vector<int>& data);

        // Generates a list of bounding boxes for all solid tiles in world space
        std::vector<Rectangle> GetCollisionRects() const;

        // Returns a read-only reference to the layers list for the UI
        const std::vector<TileLayer>& GetLayers() const { return m_layers; }

        // Draw the grid in the editor
        void RenderGrid() const;

        // Retrieves a mutable pointer to a layer by its index or name
        TileLayer* GetLayer(int index);
        TileLayer* GetLayer(const std::string& name);

    private:
        std::vector<Tileset> m_tilesets;
        std::vector<TileLayer> m_layers;

        // Helper to safely access 1D vector as 2D
        bool IsInBounds(int x, int y) const;
};
