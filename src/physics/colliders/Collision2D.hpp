#pragma once

#include <raylib.h>

class GameObject;

// Data structure to send to our python scripts
struct Collision2D {
    GameObject* other; // The object we hit (can be nullptr if we hit the TileMap)
    Vector2 normal; // The direction of the surface we hit
};

// Data structure for the result of a Raycast

struct RaycastHit {
    bool hit;               // Did we hit something?
    GameObject* collider;   // What did we hit? (Can be null if we hit a TileMap)
    Vector2 point;          // The exact world coordinate of the intersection
    Vector2 normal;         // The surface normal at the hit point
    float distance;         // Distance from origin to the hit point
};
