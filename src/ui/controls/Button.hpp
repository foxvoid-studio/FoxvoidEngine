#pragma once

#include "scene/Component.hpp"
#include "core/UUID.hpp"
#include <raylib.h>
#include <nlohmann/json.hpp>
#include <string>

enum class ButtonState {
    Normal,
    Hovered,
    Pressed
};

enum class ButtonTransition {
    None,
    ColorTint,
    SpriteSwap
};

class Button : public Component {
    public:
        // Legacy Hitbox (Used if no RectTransform is present)
        float width;
        float height;
        bool isHUD;

        // Transition Settings
        ButtonTransition transition;

        // Visual feedback colors (ColorTint mode)
        Color normalColor;
        Color hoverColor;
        Color pressedColor;

        // Visual feedback sprites (SpriteSwap mode)
        UUID normalSpriteUUID;
        UUID hoverSpriteUUID;
        UUID pressedSpriteUUID;

        Button();
        ~Button() override = default;

        void Update(float deltaTime) override;

        void Render() override;

        std::string GetName() const override;

#ifndef STANDALONE_MODE
        void OnInspector() override;
#endif

        nlohmann::json Serialize() const override;
        void Deserialize(const nlohmann::json& j) override;

        // API for Python to check if the button was clicked this frame
        bool IsClicked() const;
        
        // Expose state if needed
        ButtonState GetState() const { return m_state; }

    private:
        ButtonState m_state;
        ButtonState m_lastState;
        bool m_wasClicked;

        // Helper to apply the visual changes based on the current state and transition mode
        void ApplyTransition();
};
