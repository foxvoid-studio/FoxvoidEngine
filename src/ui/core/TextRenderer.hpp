#pragma once

#include "scene/Component.hpp"
#include "core/utils/UUID.hpp"
#include <string>
#include <raylib.h>

class TextRenderer : public Component {
    public:
        std::string text;
        float fontSize;
        float spacing;
        Color color;
        bool isHUD; // If true, renders in Screen Space, If false, renders in World Space.

        TextRenderer();

        void Render() override;
        void RenderHUD() override;

        std::string GetName() const override;

#ifndef STANDALONE_MODE
        void OnInspector() override;
#endif

        nlohmann::json Serialize() const override;
        void Deserialize(const nlohmann::json& j) override;

        // Overloaded methods to handle both Paths (UI/Scripts) and UUIDs (Serialization)
        void SetFontPath(const std::string& path);
        void SetFontPath(UUID uuid);

        std::string GetFontPath() const;

    private:
        // We store the UUID instead of the hardcoded path
        UUID m_fontUUID = 0;

        Font m_customFont;
        bool m_isFontLoaded;
};
