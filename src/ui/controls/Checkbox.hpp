#pragma once

#include "scene/Component.hpp"
#include "core/utils/UUID.hpp"
#include <raylib.h>
#include <nlohmann/json.hpp>
#include <string>

class Checkbox : public Component {
    public:
        // The current state of the toggle
        bool isOn;

        // Visual mode: Color tint vs Texture swap
        bool useSprite;

        // Colors for ShapeRenderer or ImageRenderer tint
        Color colorOn;
        Color colorOff;

        // Textures for ImageRenderer swap
        UUID spriteOnUUID;
        UUID spriteOffUUID;

        Checkbox();
        ~Checkbox() override = default;

        void Update(float deltaTime) override;

        std::string GetName() const override;

#ifndef STANDALONE_MODE
        void OnInspector() override;
#endif

        nlohmann::json Serialize() const override;
        void Deserialize(const nlohmann::json& j) override;

    private:
        // Tracks state changes to update visuals only when needed
        bool m_lastState; 
        
        // Helper to push visual changes to attached renderers
        void ApplyVisuals();
};
