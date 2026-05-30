#pragma once

#include <vector>
#include <raylib.h>

enum class ColliderShape { Polygon, Circle, Capsule };

// Universal structure to hold extracted collision geometry
struct ColliderData {
    ColliderShape shapeType;
    std::vector<Vector2> vertices; // Used if shapeType == Polygon
    Vector2 center;                // Used if shapeType == Circle (Global Space)
    Vector2 p1, p2;                // Used if shapeType == Capsule (The internal segment)
    float radius;                  // Used if shapeType == Circle (Global Scaled)
    bool isTrigger;
};
