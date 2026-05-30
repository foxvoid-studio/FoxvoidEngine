#pragma once

#include "scene/GameObject.hpp"
#include <raylib.h>
#include "physics/colliders/Collision2D.hpp"
#include "physics/colliders/ColliderData.hpp"

class Scene;
class RectCollider;
class Transform2d;

class PhysicsEngine {
    public:
        // Global gravity force.
        static Vector2 GlobalGravity;

        // The main function to call every frame in our game loop
        static void Update(Scene& scene, float deltaTime);

        // Casts a ray against all colliders and tilemaps in the scene.
        // Returns the closest hit, if any.
        static RaycastHit Raycast(Scene& scene, Vector2 origin, Vector2 direction, float maxDistance);

        // Draws the collision boxes for debugging purposes
        static void RenderDebug(Scene& scene);

    private:
        // Helper function to calculate AABB overlap and push objects apart
        static bool ResolveCollision(GameObject* objA, GameObject* objB, Vector2& outNormalA, Vector2& outNormalB);

        // Helper function to resolve collision against static TileMap geometry
        static bool ResolveTileCollision(GameObject* obj, const Rectangle& tileRect, Vector2& outNormal);

        // Helper to check line vs AABB intersection
        static bool CheckLineBoxIntersection(Vector2 p1, Vector2 p2, Rectangle box, Vector2& hitPoint, Vector2& hitNormal);

        // Transforms a RectCollider into 4 rotated vertices in global space
        static std::vector<Vector2> GetColliderVertices(RectCollider* col, Transform2d* t);

        // Generic method that returns the global vertices of ANY collider attached to the object
        static bool GetObjectColliderData(GameObject* obj, ColliderData& outData);

        // Transforms a simple standard Rectangle (like a Tile) into 4 vertices
        static std::vector<Vector2> GetRectangleVertices(const Rectangle& rect);

        // Finds the mathematical center of a polygon
        static Vector2 GetPolygonCenter(const std::vector<Vector2>& vertices);
        
        // Projects a polygon onto a given axis to get the min and max points of the "shadow"
        static void ProjectPolygon(const std::vector<Vector2>& vertices, Vector2 axis, float& min, float& max);
        
        static Vector2 ClosestPointOnLineSegment(Vector2 p, Vector2 a, Vector2 b);
        
        // The pure SAT Algorithm: returns true if there is a collision, and populates outMTV with the push vector
        static bool SATCollision(const std::vector<Vector2>& polyA, const std::vector<Vector2>& polyB, Vector2& outMTV);

        // Handles collisions between two perfect circles
        static bool CircleCircleCollision(const ColliderData& circleA, const ColliderData& circleB, Vector2& outMTV);
        
        // Handles collisions between a polygon and a perfect circle using modified SAT
        static bool SATPolygonCircleCollision(const ColliderData& poly, const ColliderData& circle, Vector2& outMTV);

        static bool CapsuleCircleCollision(const ColliderData& cap, const ColliderData& circ, Vector2& outMTV);
        static bool SATPolygonCapsuleCollision(const ColliderData& poly, const ColliderData& cap, Vector2& outMTV);
        static bool CapsuleCapsuleCollision(const ColliderData& capA, const ColliderData& capB, Vector2& outMTV);
};
