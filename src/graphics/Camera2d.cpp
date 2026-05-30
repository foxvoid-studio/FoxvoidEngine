#include "Camera2d.hpp"
#include "math/Transform2d.hpp"
#include "scene/GameObject.hpp"
#include <raymath.h>
#include "core/Engine.hpp"
#include "scene/Scene.hpp"
#include "graphics/TileMap.hpp"

#ifndef STANDALONE_MODE
#include "editor/EditorUI.hpp"
#endif

// Default constructor: 1.0x zoom, no offset, and marked as the main camera by default
Camera2d::Camera2d() 
    : zoom(1.0f), 
      offset{0.0f, 0.0f}, 
      anchor(Camera2dAnchor::Center), 
      isMain(true),
      backgroundColor{40, 40, 40, 255}, // Dark grey by default
      lerpFactor(0.0f), // Default is instant follow
      useWorldBounds(false),
      worldBounds{0.0f, 0.0f, 2000.0f, 2000.0f},
      m_currentTarget{0.0f, 0.0f},
      m_isFirstFrame(true),
      m_shakeIntensity(0.0f),
      m_shakeDuration(0.0f),
      m_shakeTimer(0.0f)
{}

void Camera2d::Shake(float intensity, float duration) {
    m_shakeIntensity = intensity;
    m_shakeDuration = duration;
    m_shakeTimer = duration;
}

void Camera2d::Update(float deltaTime) {
    if (!owner) return;

    // Update the shake timer
    if (m_shakeTimer > 0.0f) {
        m_shakeTimer -= deltaTime;
        if (m_shakeTimer < 0.0f) {
            m_shakeTimer = 0.0f;
        }
    }

    auto transform = owner->GetComponent<Transform2d>();
    if (!transform) return;

    // Prevent the camera from sliding from the origin on the very first frame of the game
    if (m_isFirstFrame) {
        m_currentTarget = transform->GetGlobalPosition();
        m_isFirstFrame = false;
    } else {
        // Smooth interpolation towards the Transform's position
        if (lerpFactor > 0.0f) {
            m_currentTarget = Vector2Lerp(m_currentTarget, transform->GetGlobalPosition(), lerpFactor * deltaTime);
        } else {
            m_currentTarget = transform->GetGlobalPosition(); // Instant snap
        }
    }
}

std::string Camera2d::GetName() const {
    return "Camera 2D";
}

#ifndef STANDALONE_MODE
void Camera2d::OnInspector() {
    EditorUI::DragFloat("Zoom", &zoom, 0.01f, this, 0.1f, 10.0f);
    
    const char* anchorNames[] = { "Top Left", "Center" };
    int currentAnchor = static_cast<int>(anchor);
    
    if (ImGui::Combo("Anchor", &currentAnchor, anchorNames, 2)) {
        // Track Undo/Redo for the dropdown
        nlohmann::json initialState = Serialize();
        anchor = static_cast<Camera2dAnchor>(currentAnchor);
        CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, initialState, Serialize()));
    }

    // Offset is useful if you want the character to be on the left of the screen 
    // rather than perfectly in the center (e.g., in a runner game).
    EditorUI::DragFloat2("Screen Offset", &offset.x, 1.0f, this);
    
    EditorUI::Checkbox("Is Main Camera", &isMain, this);

    EditorUI::ColorEdit4("Background Color", &backgroundColor, this);

    ImGui::Separator();
    
    // Lerp settings
    EditorUI::DragFloat("Lerp Factor", &lerpFactor, 0.1f, this, 0.0f, 50.0f);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("0 = Instant Follow. 5 = Smooth. 20 = Snappy.");

    // World Bounds settings
    EditorUI::Checkbox("Use World Bounds", &useWorldBounds, this);
    if (useWorldBounds) {
        float bounds[4] = { worldBounds.x, worldBounds.y, worldBounds.width, worldBounds.height };
        
        if (EditorUI::DragFloat4("Bounds (X,Y,W,H)", bounds, 1.0f, this)) {
            worldBounds = { bounds[0], bounds[1], bounds[2], bounds[3] };
        }

        // Auto-calculate button
        if (ImGui::Button("Auto-Calculate from Scene", ImVec2(-1, 0))) {
            if (Engine::Get()) {
                Scene& scene = Engine::Get()->GetActiveScene();
                
                // Initialize with extreme values to find the actual min/max
                float minX = 999999.0f, minY = 999999.0f;
                float maxX = -999999.0f, maxY = -999999.0f;
                bool foundAny = false;

                for (const auto& go : scene.GetGameObjects()) {
                    auto transform = go->GetComponent<Transform2d>();
                    if (!transform) continue;

                    auto position = transform->GetGlobalPosition();

                    float objMinX = position.x;
                    float objMinY = position.y;
                    float objMaxX = position.x;
                    float objMaxY = position.y;

                    // If it's a TileMap, add the total grid size to the maximum limits
                    if (auto tm = go->GetComponent<TileMap>()) {
                        objMaxX += tm->gridWidth * tm->tileSize.x * transform->scale.x;
                        objMaxY += tm->gridHeight * tm->tileSize.y * transform->scale.y;
                    }

                    if (objMinX < minX) minX = objMinX;
                    if (objMinY < minY) minY = objMinY;
                    if (objMaxX > maxX) maxX = objMaxX;
                    if (objMaxY > maxY) maxY = objMaxY;

                    foundAny = true;
                }

                // If any objects were found, apply the new limits
                if (foundAny) {
                    nlohmann::json initialState = Serialize();
                    
                    // Define the global rectangle (X, Y, Width, Height)
                    worldBounds = { minX, minY, maxX - minX, maxY - minY };
                    
                    CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, initialState, Serialize()));
                }
            }
        }
    }
}
#endif

nlohmann::json Camera2d::Serialize() const {
    return {
        { "type", "Camera2d" },
        { "zoom", zoom },
        { "offsetX", offset.x },
        { "offsetY", offset.y },
        { "isMain", isMain },
        { "bgColor", { backgroundColor.r, backgroundColor.g, backgroundColor.b, backgroundColor.a } },
        { "lerpFactor", lerpFactor },
        { "useWorldBounds", useWorldBounds },
        { "boundsX", worldBounds.x },
        { "boundsY", worldBounds.y },
        { "boundsW", worldBounds.width },
        { "boundsH", worldBounds.height }
    };
}

void Camera2d::Deserialize(const nlohmann::json& j) {
    zoom = j.value("zoom", 1.0f);
    offset.x = j.value("offsetX", 0.0f);
    offset.y = j.value("offsetY", 0.0f);
    isMain = j.value("isMain", true);

    if (j.contains("bgColor") && j["bgColor"].is_array() && j["bgColor"].size() == 4) {
        backgroundColor.r = j["bgColor"][0];
        backgroundColor.g = j["bgColor"][1];
        backgroundColor.b = j["bgColor"][2];
        backgroundColor.a = j["bgColor"][3];
    }

    lerpFactor = j.value("lerpFactor", 0.0f);
    useWorldBounds = j.value("useWorldBounds", false);
    
    worldBounds.x = j.value("boundsX", 0.0f);
    worldBounds.y = j.value("boundsY", 0.0f);
    worldBounds.width = j.value("boundsW", 2000.0f);
    worldBounds.height = j.value("boundsH", 2000.0f);

    // Reset first frame flag on load
    m_isFirstFrame = true;
}

Camera2D Camera2d::GetCamera(float screenWidth, float screenHeight) const {
    Camera2D cam = { 0 };
    cam.zoom = zoom;

    // Calculate base anchor offset
    Vector2 baseOffset = { 0.0f, 0.0f };
    if (anchor == Camera2dAnchor::Center) {
        baseOffset.x = screenWidth / 2.0f;
        baseOffset.y = screenHeight / 2.0f;
    }

    cam.offset.x = baseOffset.x + offset.x;
    cam.offset.y = baseOffset.y + offset.y;

    // Target calculation
    // If the engine is currently in Edit mode (not playing), the Update() loop isn't running.
    // In this case, we bypass the lerped target and snap directly to the Transform's current position 
    // so the Game View remains accurate while editing.
    bool isPlaying = true;
    if (Engine::Get()) {
        isPlaying = Engine::Get()->IsPlaying();
    }

    if (isPlaying && !m_isFirstFrame) {
        cam.target = m_currentTarget;
    } else {
        if (owner && owner->GetComponent<Transform2d>()) {
            cam.target = owner->GetComponent<Transform2d>()->GetGlobalPosition();
        } else {
            cam.target = m_currentTarget; // Fallback to (0,0)
        }
    }

    // Apply rotation
    if (owner) {
        auto transform = owner->GetComponent<Transform2d>();
        if (transform) cam.rotation = transform->rotation;
    }

    // Clamping logic
    if (useWorldBounds) {
        // Find the absolute edges of the camera view in World Space
        float halfVisibleWidth = (baseOffset.x) / zoom;
        float halfVisibleHeight = (baseOffset.y) / zoom;
        float remainVisibleWidth = (screenWidth - baseOffset.x) / zoom;
        float remainVisibleHeight = (screenHeight - baseOffset.y) / zoom;

        // Calculate limits so the screen edges never exit the bounding box
        float minX = worldBounds.x + halfVisibleWidth;
        float maxX = worldBounds.x + worldBounds.width - remainVisibleWidth;
        
        float minY = worldBounds.y + halfVisibleHeight;
        float maxY = worldBounds.y + worldBounds.height - remainVisibleHeight;

        // Edge case: If the screen is actually bigger than the defined bounds, 
        // we prevent max from being smaller than min (which causes visual glitches)
        if (minX > maxX) maxX = minX;
        if (minY > maxY) maxY = minY;

        cam.target.x = Clamp(cam.target.x, minX, maxX);
        cam.target.y = Clamp(cam.target.y, minY, maxY);
    }

    // Screen shake logic
    // We apply it at the very end to the visual offset, so it doesn't mess with bounds or lerping
    if (m_shakeTimer > 0.0f && m_shakeDuration > 0.0f) {
        // Calculate the dampening factor (1.0 at start, moving to 0.0 at the end)
        float dampening = m_shakeTimer / m_shakeDuration;
        float currentIntensity = m_shakeIntensity * dampening;

        // Apply random shift using Raylib's GetRandomValue
        // GetRandomValue takes integers, so we cast to int and back to float
        int maxOffset = (int)currentIntensity;
        if (maxOffset > 0) {
            cam.offset.x += (float)GetRandomValue(-maxOffset, maxOffset);
            cam.offset.y += (float)GetRandomValue(-maxOffset, maxOffset);
        }
    }

    return cam;
}
