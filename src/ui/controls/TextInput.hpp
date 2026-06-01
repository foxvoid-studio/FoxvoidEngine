#pragma once

#include "scene/Component.hpp"
#include "core/utils/UUID.hpp"
#include <raylib.h>
#include <nlohmann/json.hpp>
#include <string>

class TextInput : public Component {
    public:
        // The current text inside the field
        std::string text;

        // Maximum number of characters allowed
        int maxLength;

        // Visual properties

        // Custom font support
        int fontSize;
        float spacing;
        Color textColor;

        Color bgColor;
        Color focusedBgColor; // Color of the background when the user is typing

        TextInput();
        ~TextInput() override = default;

        // Handles focus and keyboard input
        void Update(float deltaTime) override;
        
        // Draws the background, text, and blinking cursor
        void RenderHUD() override;

        // Font setters
        void SetFont(const std::string& path);
        void SetFont(UUID uuid);

        std::string GetName() const override;

#ifndef STANDALONE_MODE
        void OnInspector() override;
#endif

        nlohmann::json Serialize() const override;
        void Deserialize(const nlohmann::json& j) override;

        // Allows Python to know if the user is currently typing in this field
        bool IsFocused() const { return m_isFocused; }

    private:
        bool m_isFocused;
        
        // Cursor blinking variables
        float m_cursorTimer;
        bool m_showCursor;

        // Font internal data
        UUID m_fontUUID;
        Font m_font; // Cached font for fast rendering
};
