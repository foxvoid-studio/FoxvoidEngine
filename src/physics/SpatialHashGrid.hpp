#pragma once

#include <raylib.h>
#include <unordered_map>
#include <vector>
#include <utility>
#include <unordered_set>
#include "scene/GameObject.hpp"
#include "physics/PhysicsEngine.hpp"

// Custom Hasher so std::unordered_map can use a std::pair<int, int> as a Key
struct PairHash {
    std::size_t operator()(const std::pair<int, int>& p) const {
        // A simple but effective bitwise hash combining X and Y
        return std::hash<int>()(p.first) ^ (std::hash<int>()(p.second) << 1);
    }
};

class SpatialHashGrid {
    public:
        // By default, cells are 100x100 pixels. You can tweak this later!
        SpatialHashGrid(float cellSize = 100.0f);

        // Clears the entire grid (called at the start of every frame)
        void Clear();

        // Calculates the Bounding Box of an object and inserts it into the overlapping cells
        void Insert(GameObject* obj, const ColliderData& data);

        // Returns all UNIQUE GameObjects in the same cells as the queried object
        std::vector<GameObject*> GetNearby(const ColliderData& data);

    private:
        float m_cellSize;
        
        // The Infinite Grid: Maps a Grid Coordinate (X, Y) to a list of GameObjects
        std::unordered_map<std::pair<int, int>, std::vector<GameObject*>, PairHash> m_cells;

        // Helper: Converts a World Position to Grid Coordinates
        std::pair<int, int> GetCellCoords(Vector2 pos) const;

        // Helper: Calculates the Axis-Aligned Bounding Box (AABB) of any shape
        void GetAABB(const ColliderData& data, Vector2& outMin, Vector2& outMax) const;
};
