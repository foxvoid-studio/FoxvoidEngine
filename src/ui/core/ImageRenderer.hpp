#pragma once

#include "scene/Component.hpp"
#include "core/utils/UUID.hpp"
#include <string>
#include <raylib.h>

class ImageRenderer : public Component {
    public:
        // Color tint applied to the image (WHITE means no tint)
        Color color = WHITE; 
        
        // Determines if the image is drawn in screen space (UI) or world space
        bool isHUD = true;

        // Constructor can take an optional path to immediately load a texture
        ImageRenderer(const std::string& texturePath = "");
        
        // Virtual destructor
        ~ImageRenderer() override;

        // Called during the world rendering pass
        void Render() override;
        
        // Called during the UI rendering pass (drawn on top of the world)
        void RenderHUD() override;

        // Safely updates the texture using a file path (resolves to UUID)
        void SetTexture(const std::string& path);
        
        // Safely updates the texture using its unique registry ID
        void SetTexture(UUID uuid);
        
        // Returns the underlying Raylib texture object
        Texture2D GetTexture() const { return m_texture; }

        std::string GetName() const override;

#ifndef STANDALONE_MODE
        void OnInspector() override;
#endif

        nlohmann::json Serialize() const override;
        void Deserialize(const nlohmann::json& j) override;

    private:
        // The unique identifier linking this component to the texture asset on disk
        UUID m_textureUUID = 0;
        
        // Cached Raylib texture struct for fast rendering
        Texture2D m_texture;
};
