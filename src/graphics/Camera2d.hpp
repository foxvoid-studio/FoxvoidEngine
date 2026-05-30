#pragma once

#include "scene/Component.hpp"
#include <raylib.h>
#include <nlohmann/json.hpp>

// Define the possible anchor points for the camera
enum class Camera2dAnchor {
    TopLeft,
    Center
};

class Camera2d : public Component {
    public:
        float zoom;
        Vector2 offset; // Custom shift relative to the anchor
        Camera2dAnchor anchor; // The base screen position
        bool isMain; // Determines if this is the primary camera used to render the Game View

        // Background clear color
        Color backgroundColor;

        // Camera smoothing and Constraints
        float lerpFactor; // 0 means instant snap, > 0 means smooth follow (e.g., 5.0)
        bool useWorldBounds;
        Rectangle worldBounds; // Limits of the camera: x, y, width, height

        Camera2d();

        // Called every frame to calculate smooth movement
        void Update(float deltaTime) override;

        std::string GetName() const override;

#ifndef STANDALONE_MODE
        void OnInspector() override;
#endif

        nlohmann::json Serialize() const override;
        void Deserialize(const nlohmann::json& j) override;

        // Generates the Raylib Camera2D struct dynamically based on the GameObject's Transform2d
        Camera2D GetCamera(float screenWidth, float screenHeight) const;

        // Triggers a screen shake effect
        // intensity: max pixel offset, duration: how long it lasts in seconds
        void Shake(float intensity, float duration);

    private:
        Vector2 m_currentTarget; // Internal state for Lerp calculation
        bool m_isFirstFrame;     // Ensures the camera snaps instantly on start instead of sliding from 0,0

        // Screen shake internal state
        float m_shakeIntensity;
        float m_shakeDuration;
        float m_shakeTimer;
};
