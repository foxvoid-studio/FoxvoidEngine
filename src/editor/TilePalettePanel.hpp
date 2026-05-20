#pragma once

#include <raylib.h>
#include <string>

class TilePalettePanel {
    public:
        TilePalettePanel();

        // Draws the Tile Palette window
        void Draw(int& selectedTileID, int& selectedLayer, class TileMap* activeTileMap);

        bool isOpen = true;

    private:
        float m_zoom = 1.0f;

        // Tracks which tileset is currently visible in the palette
        int m_selectedTilesetIndex = 0;
};
