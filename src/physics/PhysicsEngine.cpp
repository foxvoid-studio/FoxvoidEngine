#include "PhysicsEngine.hpp"
#include "math/Transform2d.hpp"
#include "physics/RigidBody2d.hpp"
#include "physics/colliders/RectCollider.hpp"
#include "graphics/renderers/TileMap.hpp"
#include "scene/Scene.hpp"
#include <raymath.h>
#include <cmath>
#include <limits>
#include "physics/colliders/PolygonCollider.hpp"
#include "physics/colliders/CircleCollider.hpp"
#include "physics/colliders/CapsuleCollider.hpp"
#include "physics/SpatialHashGrid.hpp"

// Initialize global gravity (981 pixels/sec² pointing down)
// TODO: Allow game developers to modify this gravity value from editor, and add it to the Project Settings (or new other file for global values)
Vector2 PhysicsEngine::GlobalGravity = { 0.0f, 981.0f };

void PhysicsEngine::Update(Scene& scene, float deltaTime) {
    const auto& gameObjects = scene.GetGameObjects();

    // Step 1: Integration (Apply Gravity and Velocity)
    for (const auto& go : gameObjects) {
        auto rb = go->GetComponent<RigidBody2d>();
        auto transform = go->GetComponent<Transform2d>();

        if (rb && transform && !rb->isKinematic) {
            rb->isGrounded = false;
            rb->velocity.x += GlobalGravity.x * rb->gravityScale * deltaTime;
            rb->velocity.y += GlobalGravity.y * rb->gravityScale * deltaTime;

            Vector2 globalPos = transform->GetGlobalPosition();
            globalPos.x += rb->velocity.x * deltaTime;
            globalPos.y += rb->velocity.y * deltaTime;
            transform->SetGlobalPosition(globalPos);
        }
    }

    // Step 2: Build the Spatial Hash Grid for this frame
    SpatialHashGrid grid(100.0f); // 100 pixels per cell
    
    // We store the data we extract so we don't have to extract it twice
    std::vector<std::pair<GameObject*, ColliderData>> physicsObjects;

    for (const auto& go : gameObjects) {
        ColliderData data;
        if (GetObjectColliderData(go.get(), data)) {
            grid.Insert(go.get(), data);
            physicsObjects.push_back({go.get(), data});
        }

        // Also add solid tiles to the grid!
        if (auto tileMap = go->GetComponent<TileMap>()) {
            for (const Rectangle& rect : tileMap->GetCollisionRects()) {
                ColliderData tileData;
                tileData.shapeType = ColliderShape::Polygon;
                tileData.vertices = GetRectangleVertices(rect);
                tileData.isTrigger = false;
                grid.Insert(go.get(), tileData);
                // We don't push tiles to physicsObjects because tiles don't initiate collisions, they only receive them.
            }
        }
    }

    std::vector<std::pair<GameObject*, Collision2D>> collisionEvents;

    // Step 3: Collision resolution using the Broadphase
    // Notice how we ONLY loop through physicsObjects now!
    for (size_t i = 0; i < physicsObjects.size(); ++i) {
        GameObject* objA = physicsObjects[i].first;
        ColliderData& dataA = physicsObjects[i].second;

        // BROADPHASE MAGIC: Get only objects in the same or adjacent cells!
        std::vector<GameObject*> nearbyObjects = grid.GetNearby(dataA);

        for (GameObject* objB : nearbyObjects) {
            // Prevent self-collision and duplicate checks
            if (objA == objB) continue;
            
            // To prevent testing A vs B, then B vs A later, we enforce a strict memory address order
            // Note: TileMaps are a special case because they are added to the grid but not physicsObjects
            bool isTileMapB = objB->GetComponent<TileMap>() != nullptr;
            if (!isTileMapB && objA >= objB) continue; 

            Vector2 normalA = { 0.0f, 0.0f };
            Vector2 normalB = { 0.0f, 0.0f };
            
            bool hit = false;
            if (isTileMapB) {
                // If B is a TileMap, we need a special check because the Grid returns the TileMap object,
                // but we need to check against its specific tiles.
                // (Optimization note: A more advanced grid would store the specific Tile index, 
                // but looping over nearby tiles is fast enough for now).
                auto tileMap = objB->GetComponent<TileMap>();
                for (const Rectangle& rect : tileMap->GetCollisionRects()) {
                    if (ResolveTileCollision(objA, rect, normalA)) {
                        collisionEvents.push_back({ objA, Collision2D{ nullptr, normalA } });
                    }
                }
            } 
            else {
                if (ResolveCollision(objA, objB, normalA, normalB)) {
                    collisionEvents.push_back({ objA, Collision2D{ objB, normalA } });
                    collisionEvents.push_back({ objB, Collision2D{ objA, normalB } });
                }
            }
        }
    }

    // Step 4: Dispatch events to scripts
    for (const auto& event : collisionEvents) {
        event.first->OnCollision(event.second);
    }
}

bool PhysicsEngine::ResolveCollision(GameObject* objA, GameObject* objB, Vector2& outNormalA, Vector2& outNormalB) {
    ColliderData dataA, dataB;

    if (!GetObjectColliderData(objA, dataA)) return false;
    if (!GetObjectColliderData(objB, dataB)) return false;

    Vector2 mtv;
    bool hasCollision = false;

    // Collision Dispatcher
    if (dataA.shapeType == ColliderShape::Polygon && dataB.shapeType == ColliderShape::Polygon) {
        hasCollision = SATCollision(dataA.vertices, dataB.vertices, mtv);
    } 
    else if (dataA.shapeType == ColliderShape::Circle && dataB.shapeType == ColliderShape::Circle) {
        hasCollision = CircleCircleCollision(dataA, dataB, mtv);
    } 
    else if (dataA.shapeType == ColliderShape::Capsule && dataB.shapeType == ColliderShape::Capsule) {
        hasCollision = CapsuleCapsuleCollision(dataA, dataB, mtv);
    }
    else if (dataA.shapeType == ColliderShape::Polygon && dataB.shapeType == ColliderShape::Circle) {
        hasCollision = SATPolygonCircleCollision(dataA, dataB, mtv);
    } 
    else if (dataA.shapeType == ColliderShape::Circle && dataB.shapeType == ColliderShape::Polygon) {
        hasCollision = SATPolygonCircleCollision(dataB, dataA, mtv);
        if (hasCollision) mtv = {-mtv.x, -mtv.y}; 
    }
    else if (dataA.shapeType == ColliderShape::Polygon && dataB.shapeType == ColliderShape::Capsule) {
        hasCollision = SATPolygonCapsuleCollision(dataA, dataB, mtv);
    } 
    else if (dataA.shapeType == ColliderShape::Capsule && dataB.shapeType == ColliderShape::Polygon) {
        hasCollision = SATPolygonCapsuleCollision(dataB, dataA, mtv);
        if (hasCollision) mtv = {-mtv.x, -mtv.y}; 
    }
    else if (dataA.shapeType == ColliderShape::Capsule && dataB.shapeType == ColliderShape::Circle) {
        hasCollision = CapsuleCircleCollision(dataA, dataB, mtv);
    }
    else if (dataA.shapeType == ColliderShape::Circle && dataB.shapeType == ColliderShape::Capsule) {
        hasCollision = CapsuleCircleCollision(dataB, dataA, mtv);
        if (hasCollision) mtv = {-mtv.x, -mtv.y}; 
    }
    
    if (hasCollision) {
        if (dataA.isTrigger || dataB.isTrigger) {
            outNormalA = { 0.0f, 0.0f };
            outNormalB = { 0.0f, 0.0f };
            return true;
        }

        auto tA = objA->GetComponent<Transform2d>();
        auto tB = objB->GetComponent<Transform2d>();
        auto rbA = objA->GetComponent<RigidBody2d>();
        auto rbB = objB->GetComponent<RigidBody2d>();

        bool isKinematicA = !rbA || rbA->isKinematic;
        bool isKinematicB = !rbB || rbB->isKinematic;

        if (isKinematicA && isKinematicB) return true;

        // The collision normal is simply the direction of the MTV
        Vector2 normal = Vector2Normalize(mtv);
        outNormalA = {-normal.x, -normal.y};
        outNormalB = normal;

        // Check the push direction (outNormal) instead of the raw MTV direction
        // In Raylib, negative Y is upwards. If outNormal pushes the object UP, it's grounded.
        if (outNormalA.y < -0.5f && rbA) rbA->isGrounded = true;
        if (outNormalB.y < -0.5f && rbB) rbB->isGrounded = true;

        Vector2 posA = tA->GetGlobalPosition();
        Vector2 posB = tB->GetGlobalPosition();

        if (!isKinematicA && isKinematicB) {
            tA->SetGlobalPosition(Vector2Subtract(posA, mtv));
            
            // Axis-dependent velocity cancellation to avoid sticking to walls
            if (rbA) { 
                if (std::abs(outNormalA.x) > 0.5f) rbA->velocity.x = 0;
                if (std::abs(outNormalA.y) > 0.5f) rbA->velocity.y = 0;
            }
        } 
        else if (isKinematicA && !isKinematicB) {
            tB->SetGlobalPosition(Vector2Add(posB, mtv));
            
            if (rbB) { 
                if (std::abs(outNormalB.x) > 0.5f) rbB->velocity.x = 0;
                if (std::abs(outNormalB.y) > 0.5f) rbB->velocity.y = 0;
            }
        } 
        else {
            // Split mass: push both objects away from each other equally
            tA->SetGlobalPosition(Vector2Subtract(posA, {mtv.x * 0.5f, mtv.y * 0.5f}));
            tB->SetGlobalPosition(Vector2Add(posB, {mtv.x * 0.5f, mtv.y * 0.5f}));
            
            if (rbA) { 
                if (std::abs(outNormalA.x) > 0.5f) rbA->velocity.x = 0;
                if (std::abs(outNormalA.y) > 0.5f) rbA->velocity.y = 0;
            }
            if (rbB) { 
                if (std::abs(outNormalB.x) > 0.5f) rbB->velocity.x = 0;
                if (std::abs(outNormalB.y) > 0.5f) rbB->velocity.y = 0;
            }
        }

        return true;
    }
    return false;
}

bool PhysicsEngine::ResolveTileCollision(GameObject* obj, const Rectangle& tileRect, Vector2& outNormal) {
    ColliderData dataA;
    if (!GetObjectColliderData(obj, dataA)) return false;

    // A tile is always a Polygon
    ColliderData dataTile;
    dataTile.shapeType = ColliderShape::Polygon;
    dataTile.vertices = GetRectangleVertices(tileRect);

    Vector2 mtv;
    bool hasCollision = false;

    if (dataA.shapeType == ColliderShape::Polygon) {
        hasCollision = SATCollision(dataA.vertices, dataTile.vertices, mtv);
    } 
    else if (dataA.shapeType == ColliderShape::Circle) {
        // Here, A is the Circle, and B is the Tile (Polygon)
        // We calculate Poly vs Circle, and INVERT the MTV because 'obj' is A.
        hasCollision = SATPolygonCircleCollision(dataTile, dataA, mtv);
        if (hasCollision) mtv = {-mtv.x, -mtv.y};
    } 
    else if (dataA.shapeType == ColliderShape::Capsule) {
        hasCollision = SATPolygonCapsuleCollision(dataTile, dataA, mtv);
        if (hasCollision) mtv = {-mtv.x, -mtv.y};
    }

    if (hasCollision) {
        if (dataA.isTrigger) {
            outNormal = {0.0f, 0.0f};
            return true;
        }

        auto t = obj->GetComponent<Transform2d>();
        auto rb = obj->GetComponent<RigidBody2d>();

        if (!rb || rb->isKinematic) return false;

        Vector2 normal = Vector2Normalize(mtv);
        outNormal = {-normal.x, -normal.y};

        // Check outNormal instead of normal
        if (outNormal.y < -0.5f) rb->isGrounded = true;

        Vector2 pos = t->GetGlobalPosition();
        t->SetGlobalPosition(Vector2Subtract(pos, mtv));
        
        // Basic velocity cancellation depending on the hit axis
        if (std::abs(outNormal.x) > 0.5f) rb->velocity.x = 0;
        if (std::abs(outNormal.y) > 0.5f) rb->velocity.y = 0;

        return true;
    }
    return false;
}

void PhysicsEngine::RenderDebug(Scene& scene) {
    const auto& gameObjects = scene.GetGameObjects();

    for (const auto& go : gameObjects) {
        ColliderData data;
        if (GetObjectColliderData(go.get(), data)) {
            Color debugColor = data.isTrigger ? YELLOW : GREEN;
            
            if (data.shapeType == ColliderShape::Polygon) {
                for (size_t i = 0; i < data.vertices.size(); i++) {
                    DrawLineEx(data.vertices[i], data.vertices[(i + 1) % data.vertices.size()], 2.0f, debugColor);
                }
            } 
            else if (data.shapeType == ColliderShape::Circle) {
                // Draw a perfect circle outline
                DrawCircleLines(data.center.x, data.center.y, data.radius, debugColor);
                
                // Draw a line indicating the rotation of the circle
                if (auto t = go->GetComponent<Transform2d>()) {
                    float rot = t->GetGlobalRotation() * DEG2RAD;
                    Vector2 rotLineEnd = {
                        data.center.x + std::cos(rot) * data.radius,
                        data.center.y + std::sin(rot) * data.radius
                    };
                    DrawLineEx(data.center, rotLineEnd, 2.0f, debugColor);
                }
            }
            else if (data.shapeType == ColliderShape::Capsule) {
                // Draw the two end circles
                DrawCircleLines(data.p1.x, data.p1.y, data.radius, debugColor);
                DrawCircleLines(data.p2.x, data.p2.y, data.radius, debugColor);
                
                // Draw the two side lines connecting the circles
                Vector2 edge = Vector2Subtract(data.p2, data.p1);
                Vector2 normal = Vector2Normalize({-edge.y, edge.x});
                Vector2 offsetLeft = Vector2Scale(normal, data.radius);
                Vector2 offsetRight = Vector2Scale(normal, -data.radius);
                
                DrawLineEx(Vector2Add(data.p1, offsetLeft), Vector2Add(data.p2, offsetLeft), 2.0f, debugColor);
                DrawLineEx(Vector2Add(data.p1, offsetRight), Vector2Add(data.p2, offsetRight), 2.0f, debugColor);
            }

            if (auto t = go->GetComponent<Transform2d>()) {
                Vector2 pos = t->GetGlobalPosition();
                DrawLine(pos.x - 5, pos.y, pos.x + 5, pos.y, RED);
                DrawLine(pos.x, pos.y - 5, pos.x, pos.y + 5, RED);
            }
        }

        // Draw TileMap Solid Layers Geometry
        if (auto tileMap = go->GetComponent<TileMap>()) {
            std::vector<Rectangle> solidTiles = tileMap->GetCollisionRects();
            for (const auto& tileRect : solidTiles) {
                DrawRectangleLinesEx(tileRect, 1.5f, SKYBLUE);
            }
        }
    }
}

// A simple implementation of Line-vs-AABB using Raylib maths
bool PhysicsEngine::CheckLineBoxIntersection(Vector2 p1, Vector2 p2, Rectangle box, Vector2& hitPoint, Vector2& hitNormal) {
    // We use Raylib's internal CheckCollisionLines to test the ray against the 4 edges of the box.
    // We keep the intersection that is closest to p1.
    
    Vector2 boxCorners[4] = {
        { box.x, box.y },                           // Top-Left
        { box.x + box.width, box.y },               // Top-Right
        { box.x + box.width, box.y + box.height },  // Bottom-Right
        { box.x, box.y + box.height }               // Bottom-Left
    };
    
    Vector2 boxNormals[4] = {
        { 0, -1 }, // Top edge normal
        { 1, 0 },  // Right edge normal
        { 0, 1 },  // Bottom edge normal
        { -1, 0 }  // Left edge normal
    };

    bool hitSomething = false;
    float closestDist = std::numeric_limits<float>::infinity();

    for (int i = 0; i < 4; i++) {
        Vector2 edgeStart = boxCorners[i];
        Vector2 edgeEnd = boxCorners[(i + 1) % 4];
        Vector2 currentHitPoint;

        // CheckCollisionLines returns true if the two line segments cross
        if (CheckCollisionLines(p1, p2, edgeStart, edgeEnd, &currentHitPoint)) {
            float dist = Vector2Distance(p1, currentHitPoint);
            if (dist < closestDist) {
                closestDist = dist;
                hitPoint = currentHitPoint;
                hitNormal = boxNormals[i];
                hitSomething = true;
            }
        }
    }
    return hitSomething;
}

RaycastHit PhysicsEngine::Raycast(Scene& scene, Vector2 origin, Vector2 direction, float maxDistance) {
    RaycastHit result = { false, nullptr, {0,0}, {0,0}, maxDistance };
    
    // Normalize direction to be safe
    direction = Vector2Normalize(direction);
    Vector2 destination = { origin.x + direction.x * maxDistance, origin.y + direction.y * maxDistance };

    float closestHitDist = maxDistance;

    // Check GameObjects (RectColliders)
    for (const auto& go : scene.GetGameObjects()) {
        auto col = go->GetComponent<RectCollider>();
        auto t = go->GetComponent<Transform2d>();
        
        // Skip triggers if you want, or add a parameter to the function to include them!
        if (!col || !t || col->isTrigger) continue;

        Vector2 pos = t->GetGlobalPosition();
        Vector2 scale = t->GetGlobalScale();

        float scaledWidth = col->size.x * std::abs(scale.x);
        float scaledHeight = col->size.y * std::abs(scale.y);
        Rectangle rec = {
            (pos.x + col->offset.x) - (scaledWidth / 2.0f),
            (pos.y + col->offset.y) - (scaledHeight / 2.0f),
            scaledWidth,
            scaledHeight
        };

        Vector2 hitPoint, hitNormal;
        if (CheckLineBoxIntersection(origin, destination, rec, hitPoint, hitNormal)) {
            float dist = Vector2Distance(origin, hitPoint);
            if (dist < closestHitDist) {
                closestHitDist = dist;
                result.hit = true;
                result.collider = go.get();
                result.point = hitPoint;
                result.normal = hitNormal;
                result.distance = dist;
            }
        }
    }

    // Check TileMaps
    for (const auto& go : scene.GetGameObjects()) {
        if (auto tileMap = go->GetComponent<TileMap>()) {
            
            // Get all solid blocks from this specific TileMap
            std::vector<Rectangle> solidTiles = tileMap->GetCollisionRects();
            
            for (const Rectangle& tileRect : solidTiles) {
                Vector2 hitPoint, hitNormal;
                
                // Test the ray against this specific tile
                if (CheckLineBoxIntersection(origin, destination, tileRect, hitPoint, hitNormal)) {
                    float dist = Vector2Distance(origin, hitPoint);
                    
                    // Same logic: keep it if it's the closest thing hit so far
                    if (dist < closestHitDist) {
                        closestHitDist = dist;
                        result.hit = true;
                        
                        // We associate the hit with the GameObject that owns the TileMap
                        result.collider = go.get(); 
                        result.point = hitPoint;
                        result.normal = hitNormal;
                        result.distance = dist;
                    }
                }
            }
        }
    }

    return result;
}

std::vector<Vector2> PhysicsEngine::GetColliderVertices(RectCollider* col, Transform2d* t) {
    std::vector<Vector2> vertices(4);
    
    Vector2 pos = t->GetGlobalPosition();
    Vector2 scale = t->GetGlobalScale();
    float rot = t->GetGlobalRotation() * DEG2RAD; // Raylib DEG2RAD macro converts degrees to radians
    
    float cosR = std::cos(rot);
    float sinR = std::sin(rot);
    
    float sw = col->size.x * std::abs(scale.x);
    float sh = col->size.y * std::abs(scale.y);
    float ox = col->offset.x * std::abs(scale.x);
    float oy = col->offset.y * std::abs(scale.y);

    // The 4 local corners centered around the origin (0,0)
    Vector2 localPoints[4] = {
        {-sw / 2.0f + ox, -sh / 2.0f + oy}, // Top-Left
        { sw / 2.0f + ox, -sh / 2.0f + oy}, // Top-Right
        { sw / 2.0f + ox,  sh / 2.0f + oy}, // Bottom-Right
        {-sw / 2.0f + ox,  sh / 2.0f + oy}  // Bottom-Left
    };

    // Apply the rotation matrix and translate to the global position
    for(int i = 0; i < 4; i++) {
        vertices[i].x = pos.x + (localPoints[i].x * cosR - localPoints[i].y * sinR);
        vertices[i].y = pos.y + (localPoints[i].x * sinR + localPoints[i].y * cosR);
    }
    return vertices;
}

bool PhysicsEngine::GetObjectColliderData(GameObject* obj, ColliderData& outData) {
    auto t = obj->GetComponent<Transform2d>();
    if (!t) return false;

    // 1. Standard RectCollider
    if (auto rectCol = obj->GetComponent<RectCollider>()) {
        outData.shapeType = ColliderShape::Polygon;
        outData.vertices = GetColliderVertices(rectCol, t);
        outData.isTrigger = rectCol->isTrigger;
        return true;
    }

    // 2. Custom PolygonCollider
    if (auto polyCol = obj->GetComponent<PolygonCollider>()) {
        outData.shapeType = ColliderShape::Polygon;
        outData.vertices.clear();
        outData.vertices.reserve(polyCol->localVertices.size());
        
        Vector2 pos = t->GetGlobalPosition();
        Vector2 scale = t->GetGlobalScale();
        float rot = t->GetGlobalRotation() * DEG2RAD;
        float cosR = std::cos(rot);
        float sinR = std::sin(rot);
        
        for (const auto& localV : polyCol->localVertices) {
            float sx = (localV.x + polyCol->offset.x) * std::abs(scale.x);
            float sy = (localV.y + polyCol->offset.y) * std::abs(scale.y);
            float gx = pos.x + (sx * cosR - sy * sinR);
            float gy = pos.y + (sx * sinR + sy * cosR);
            outData.vertices.push_back({gx, gy});
        }
        
        outData.isTrigger = polyCol->isTrigger;
        return true;
    }

    // 3. Perfect CircleCollider
    if (auto circleCol = obj->GetComponent<CircleCollider>()) {
        outData.shapeType = ColliderShape::Circle;
        outData.isTrigger = circleCol->isTrigger;
        
        Vector2 pos = t->GetGlobalPosition();
        Vector2 scale = t->GetGlobalScale();
        
        // The circle's global center must account for scale, rotation, and offset
        float rot = t->GetGlobalRotation() * DEG2RAD;
        float scaledOffsetX = circleCol->offset.x * std::abs(scale.x);
        float scaledOffsetY = circleCol->offset.y * std::abs(scale.y);
        
        outData.center.x = pos.x + (scaledOffsetX * std::cos(rot) - scaledOffsetY * std::sin(rot));
        outData.center.y = pos.y + (scaledOffsetX * std::sin(rot) + scaledOffsetY * std::cos(rot));
        
        // Circles can't have non-uniform scale easily. We take the largest scale axis.
        float maxScale = std::max(std::abs(scale.x), std::abs(scale.y));
        outData.radius = circleCol->radius * maxScale;
        
        return true;
    }

    // 4. CapsuleCollider
    if (auto capCol = obj->GetComponent<CapsuleCollider>()) {
        outData.shapeType = ColliderShape::Capsule;
        outData.isTrigger = capCol->isTrigger;
        
        Vector2 pos = t->GetGlobalPosition();
        Vector2 scale = t->GetGlobalScale();
        float rot = t->GetGlobalRotation() * DEG2RAD;
        
        float maxScale = std::max(std::abs(scale.x), std::abs(scale.y));
        outData.radius = capCol->radius * maxScale;
        
        // The core line segment length is the total height minus the two circles at the ends
        float segmentLen = std::max(0.0f, capCol->height - 2.0f * capCol->radius) * std::abs(scale.y);
        float halfLen = segmentLen / 2.0f;
        
        float ox = capCol->offset.x * std::abs(scale.x);
        float oy = capCol->offset.y * std::abs(scale.y);
        float cx = pos.x + (ox * std::cos(rot) - oy * std::sin(rot));
        float cy = pos.y + (ox * std::sin(rot) + oy * std::cos(rot));
        
        // Apply rotation to the top and bottom points of the inner segment
        outData.p1 = { cx + halfLen * std::sin(rot), cy - halfLen * std::cos(rot) }; // Top point
        outData.p2 = { cx - halfLen * std::sin(rot), cy + halfLen * std::cos(rot) }; // Bottom point
        
        return true;
    }

    return false;
}

std::vector<Vector2> PhysicsEngine::GetRectangleVertices(const Rectangle& rect) {
    return {
        { rect.x, rect.y },
        { rect.x + rect.width, rect.y },
        { rect.x + rect.width, rect.y + rect.height },
        { rect.x, rect.y + rect.height }
    };
}

Vector2 PhysicsEngine::GetPolygonCenter(const std::vector<Vector2>& vertices) {
    Vector2 center = {0, 0};
    for (const auto& v : vertices) {
        center.x += v.x;
        center.y += v.y;
    }
    center.x /= vertices.size();
    center.y /= vertices.size();
    return center;
}

void PhysicsEngine::ProjectPolygon(const std::vector<Vector2>& vertices, Vector2 axis, float& min, float& max) {
    min = Vector2DotProduct(vertices[0], axis);
    max = min;
    for (size_t i = 1; i < vertices.size(); i++) {
        float projection = Vector2DotProduct(vertices[i], axis);
        if (projection < min) min = projection;
        if (projection > max) max = projection;
    }
}

bool PhysicsEngine::SATCollision(const std::vector<Vector2>& polyA, const std::vector<Vector2>& polyB, Vector2& outMTV) {
    float minOverlap = std::numeric_limits<float>::infinity();
    Vector2 smallestAxis = {0, 0};

    std::vector<std::vector<Vector2>> polygons = {polyA, polyB};

    // For each polygon...
    for (int p = 0; p < 2; p++) {
        const auto& poly = polygons[p];
        
        // ... test the normal of each edge
        for (size_t i = 0; i < poly.size(); i++) {
            Vector2 p1 = poly[i];
            Vector2 p2 = poly[(i + 1) % poly.size()];
            
            // Edge vector
            Vector2 edge = Vector2Subtract(p2, p1);
            // The normal is the perpendicular vector to the edge (flipped x and y)
            Vector2 normal = Vector2Normalize({-edge.y, edge.x});

            float minA, maxA, minB, maxB;
            ProjectPolygon(polyA, normal, minA, maxA);
            ProjectPolygon(polyB, normal, minB, maxB);

            // If there is a gap on this axis, THEN THERE IS NO COLLISION AT ALL!
            if (minA >= maxB || minB >= maxA) {
                return false;
            }

            // Calculate the actual overlap depth
            float overlap = std::min(maxA, maxB) - std::max(minA, minB);
            
            // Keep track of the axis with the smallest overlap (Minimum Translation Vector)
            if (overlap < minOverlap) {
                minOverlap = overlap;
                smallestAxis = normal;
            }
        }
    }

    // Ensure the push vector always points from A to B
    Vector2 centerA = GetPolygonCenter(polyA);
    Vector2 centerB = GetPolygonCenter(polyB);
    Vector2 dir = Vector2Subtract(centerB, centerA);
    
    if (Vector2DotProduct(smallestAxis, dir) < 0) {
        smallestAxis = {-smallestAxis.x, -smallestAxis.y};
    }

    outMTV = {smallestAxis.x * minOverlap, smallestAxis.y * minOverlap};
    return true;
}

// Very cheap and perfect math for two circles
bool PhysicsEngine::CircleCircleCollision(const ColliderData& circleA, const ColliderData& circleB, Vector2& outMTV) {
    Vector2 dir = Vector2Subtract(circleB.center, circleA.center);
    float distSq = Vector2LengthSqr(dir);
    float radiusSum = circleA.radius + circleB.radius;

    if (distSq < radiusSum * radiusSum) {
        float dist = std::sqrt(distSq);
        float overlap = radiusSum - dist;
        
        // Prevent division by zero if they are in the exact same spot
        if (dist == 0.0f) {
            outMTV = {0.0f, -overlap};
        } else {
            Vector2 normal = {dir.x / dist, dir.y / dist};
            outMTV = {normal.x * overlap, normal.y * overlap};
        }
        return true;
    }
    return false;
}

// Mixed SAT Algorithm for Polygon vs Circle
bool PhysicsEngine::SATPolygonCircleCollision(const ColliderData& poly, const ColliderData& circle, Vector2& outMTV) {
    float minOverlap = std::numeric_limits<float>::infinity();
    Vector2 smallestAxis = {0, 0};

    // 1. Test all standard edge normals of the polygon
    for (size_t i = 0; i < poly.vertices.size(); i++) {
        Vector2 p1 = poly.vertices[i];
        Vector2 p2 = poly.vertices[(i + 1) % poly.vertices.size()];
        Vector2 edge = Vector2Subtract(p2, p1);
        Vector2 normal = Vector2Normalize({-edge.y, edge.x});

        float minA, maxA;
        ProjectPolygon(poly.vertices, normal, minA, maxA);
        
        // Projecting a circle is just the center projected onto the axis +/- the radius
        float centerProj = Vector2DotProduct(circle.center, normal);
        float minB = centerProj - circle.radius;
        float maxB = centerProj + circle.radius;

        // Gap found
        if (minA >= maxB || minB >= maxA) return false;

        float overlap = std::min(maxA, maxB) - std::max(minA, minB);
        if (overlap < minOverlap) {
            minOverlap = overlap;
            smallestAxis = normal;
        }
    }

    // 2. We MUST test one extra axis: The vector from the circle's center to the closest polygon vertex
    Vector2 closestVertex = poly.vertices[0];
    float minDistSq = Vector2DistanceSqr(circle.center, closestVertex);
    for (size_t i = 1; i < poly.vertices.size(); i++) {
        float distSq = Vector2DistanceSqr(circle.center, poly.vertices[i]);
        if (distSq < minDistSq) {
            minDistSq = distSq;
            closestVertex = poly.vertices[i];
        }
    }

    Vector2 specialAxis = Vector2Normalize(Vector2Subtract(circle.center, closestVertex));
    
    // Safety check if the center is exactly ON the vertex
    if (std::isnan(specialAxis.x) || std::isnan(specialAxis.y)) specialAxis = {1, 0};

    float minA, maxA;
    ProjectPolygon(poly.vertices, specialAxis, minA, maxA);
    float centerProj = Vector2DotProduct(circle.center, specialAxis);
    float minB = centerProj - circle.radius;
    float maxB = centerProj + circle.radius;

    if (minA >= maxB || minB >= maxA) return false;

    float overlap = std::min(maxA, maxB) - std::max(minA, minB);
    if (overlap < minOverlap) {
        minOverlap = overlap;
        smallestAxis = specialAxis;
    }

    // Ensure the MTV pushes from Polygon (A) to Circle (B)
    Vector2 centerA = GetPolygonCenter(poly.vertices);
    Vector2 dir = Vector2Subtract(circle.center, centerA);
    if (Vector2DotProduct(smallestAxis, dir) < 0) {
        smallestAxis = {-smallestAxis.x, -smallestAxis.y};
    }

    outMTV = {smallestAxis.x * minOverlap, smallestAxis.y * minOverlap};
    return true;
}

// Finds the closest point on a line segment (a to b) to a specific point (p)
Vector2 PhysicsEngine::ClosestPointOnLineSegment(Vector2 p, Vector2 a, Vector2 b) {
    Vector2 ab = Vector2Subtract(b, a);
    float t = Vector2DotProduct(Vector2Subtract(p, a), ab) / (Vector2LengthSqr(ab) + 0.0001f);
    t = std::clamp(t, 0.0f, 1.0f);
    return Vector2Add(a, Vector2Scale(ab, t));
}

bool PhysicsEngine::CapsuleCircleCollision(const ColliderData& cap, const ColliderData& circ, Vector2& outMTV) {
    Vector2 closest = ClosestPointOnLineSegment(circ.center, cap.p1, cap.p2);
    Vector2 dir = Vector2Subtract(circ.center, closest);
    float distSq = Vector2LengthSqr(dir);
    float radiusSum = cap.radius + circ.radius;
    
    if (distSq < radiusSum * radiusSum) {
        float dist = std::sqrt(distSq);
        float overlap = radiusSum - dist;
        if (dist == 0.0f) outMTV = {0.0f, -overlap};
        else outMTV = { (dir.x/dist)*overlap, (dir.y/dist)*overlap };
        return true;
    }
    return false;
}

bool PhysicsEngine::SATPolygonCapsuleCollision(const ColliderData& poly, const ColliderData& cap, Vector2& outMTV) {
    float minOverlap = std::numeric_limits<float>::infinity();
    Vector2 smallestAxis = {0, 0};
    std::vector<Vector2> axes;
    
    // 1. Polygon edge normals
    for (size_t i = 0; i < poly.vertices.size(); i++) {
        Vector2 p1 = poly.vertices[i];
        Vector2 p2 = poly.vertices[(i + 1) % poly.vertices.size()];
        Vector2 edge = Vector2Subtract(p2, p1);
        axes.push_back(Vector2Normalize({-edge.y, edge.x}));
    }

    // 2. Capsule segment normal
    Vector2 capEdge = Vector2Subtract(cap.p2, cap.p1);
    if (Vector2LengthSqr(capEdge) > 0.0001f) {
        axes.push_back(Vector2Normalize({-capEdge.y, capEdge.x}));
    }

    // 3. Vectors from Capsule endpoints to the closest polygon vertices
    for (Vector2 capPoint : {cap.p1, cap.p2}) {
        Vector2 closestVertex = poly.vertices[0];
        float minDistSq = Vector2DistanceSqr(capPoint, closestVertex);
        for (size_t i = 1; i < poly.vertices.size(); i++) {
            float distSq = Vector2DistanceSqr(capPoint, poly.vertices[i]);
            if (distSq < minDistSq) {
                minDistSq = distSq;
                closestVertex = poly.vertices[i];
            }
        }
        Vector2 axis = Vector2Normalize(Vector2Subtract(capPoint, closestVertex));
        if (!std::isnan(axis.x)) axes.push_back(axis);
    }

    // Test all collected axes
    for (Vector2 axis : axes) {
        float minA, maxA;
        ProjectPolygon(poly.vertices, axis, minA, maxA);
        
        float proj1 = Vector2DotProduct(cap.p1, axis);
        float proj2 = Vector2DotProduct(cap.p2, axis);
        float minB = std::min(proj1, proj2) - cap.radius;
        float maxB = std::max(proj1, proj2) + cap.radius;

        if (minA >= maxB || minB >= maxA) return false;

        float overlap = std::min(maxA, maxB) - std::max(minA, minB);
        if (overlap < minOverlap) {
            minOverlap = overlap;
            smallestAxis = axis;
        }
    }

    Vector2 centerA = GetPolygonCenter(poly.vertices);
    Vector2 centerB = { (cap.p1.x + cap.p2.x)/2.0f, (cap.p1.y + cap.p2.y)/2.0f };
    Vector2 dir = Vector2Subtract(centerB, centerA);
    if (Vector2DotProduct(smallestAxis, dir) < 0) smallestAxis = {-smallestAxis.x, -smallestAxis.y};

    outMTV = {smallestAxis.x * minOverlap, smallestAxis.y * minOverlap};
    return true;
}

// A beautiful optimization: We can resolve Capsule vs Capsule purely through SAT
// without complex segment-to-segment distance algorithms.
bool PhysicsEngine::CapsuleCapsuleCollision(const ColliderData& capA, const ColliderData& capB, Vector2& outMTV) {
    float minOverlap = std::numeric_limits<float>::infinity();
    Vector2 smallestAxis = {0, 0};
    std::vector<Vector2> axes;
    
    // 1. Both segment normals
    Vector2 edgeA = Vector2Subtract(capA.p2, capA.p1);
    if (Vector2LengthSqr(edgeA) > 0.0001f) axes.push_back(Vector2Normalize({-edgeA.y, edgeA.x}));
    
    Vector2 edgeB = Vector2Subtract(capB.p2, capB.p1);
    if (Vector2LengthSqr(edgeB) > 0.0001f) axes.push_back(Vector2Normalize({-edgeB.y, edgeB.x}));

    // 2. Point-to-Point vectors between all 4 endpoints
    Vector2 pointsA[] = {capA.p1, capA.p2};
    Vector2 pointsB[] = {capB.p1, capB.p2};
    for (auto pa : pointsA) {
        for (auto pb : pointsB) {
            Vector2 axis = Vector2Normalize(Vector2Subtract(pa, pb));
            if (!std::isnan(axis.x)) axes.push_back(axis);
        }
    }

    // Test axes
    for (Vector2 axis : axes) {
        float projA1 = Vector2DotProduct(capA.p1, axis);
        float projA2 = Vector2DotProduct(capA.p2, axis);
        float minA = std::min(projA1, projA2) - capA.radius;
        float maxA = std::max(projA1, projA2) + capA.radius;

        float projB1 = Vector2DotProduct(capB.p1, axis);
        float projB2 = Vector2DotProduct(capB.p2, axis);
        float minB = std::min(projB1, projB2) - capB.radius;
        float maxB = std::max(projB1, projB2) + capB.radius;

        if (minA >= maxB || minB >= maxA) return false;

        float overlap = std::min(maxA, maxB) - std::max(minA, minB);
        if (overlap < minOverlap) {
            minOverlap = overlap;
            smallestAxis = axis;
        }
    }

    Vector2 centerA = { (capA.p1.x + capA.p2.x)/2.0f, (capA.p1.y + capA.p2.y)/2.0f };
    Vector2 centerB = { (capB.p1.x + capB.p2.x)/2.0f, (capB.p1.y + capB.p2.y)/2.0f };
    Vector2 dir = Vector2Subtract(centerB, centerA);
    if (Vector2DotProduct(smallestAxis, dir) < 0) smallestAxis = {-smallestAxis.x, -smallestAxis.y};

    outMTV = {smallestAxis.x * minOverlap, smallestAxis.y * minOverlap};
    return true;
}
