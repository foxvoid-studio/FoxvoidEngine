#pragma once

#include "scene/Component.hpp"
#include "core/utils/UUID.hpp"
#include <string>
#include <raylib.h>
#include <nlohmann/json.hpp>

class Transform2d; // Forward declaration to avoid circular includes

class SpriteRenderer : public Component {
    public:
        // Loads the texture from the given file path
        SpriteRenderer(const std::string& texturePath = "");

        // Virtual destructor to ensure the texture is unloaded safely
        ~SpriteRenderer() override;

        void Start() override;
        void Render() override;

        // A helper method to safely change the texture during runtime or editor mode
        void SetTexture(const std::string& path);
        void SetTexture(UUID uuid);

        Texture2D GetTexture() const { return m_texture; }

        float GetWidth() const { return m_texture.width; }
        float GetHeight() const { return m_texture.height; }

        // Visual properties
        void SetTint(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255);
        void SetOpacity(float alpha);
        void SetVisible(bool visible) { m_isVisible = visible; }
        bool IsVisible() const { return m_isVisible; }

        // Editor UI and Serialization
        std::string GetName() const override;

#ifndef STANDALONE_MODE
        void OnInspector() override;
#endif

        nlohmann::json Serialize() const override;
        void Deserialize(const nlohmann::json& j) override;

    private:
        UUID m_textureUUID = 0;

        // Raylib's native texture structure
        Texture2D m_texture;

        // Cached pointer to the transform for fast position reading every frame
        Transform2d* m_transform;

        // Internal Visual State
        Color m_tint = WHITE;
        bool m_isVisible = true;
};
