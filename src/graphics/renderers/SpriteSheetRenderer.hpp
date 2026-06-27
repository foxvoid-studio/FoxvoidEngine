#pragma once

#include "scene/Component.hpp"
#include "core/utils/UUID.hpp"
#include <string>
#include <raylib.h>
#include <nlohmann/json.hpp>

#ifndef STANDALONE_MODE
#include <imgui.h>
#endif

class Transform2d; // Forward declaration

class SpriteSheetRenderer : public Component {
    public:
        bool flipX = false;
        bool flipY = false;
    
        // Takes the texture path, and the grid dimensions (columns and rows)
        SpriteSheetRenderer(const std::string& texturePath = "", int columns = 1, int rows = 1);
        ~SpriteSheetRenderer() override;

        void Start() override;
        void Render() override;

        // Safely unloads the old texture and loads the new one
        void SetTexture(const std::string& path);
        void SetTexture(UUID uuid);

        Texture2D GetTexture() const { return m_texture; }

        // Changes the current frame to display
        void SetFrame(int frameIndex);
        int GetFrame() const { return m_currentFrame; }

        // Returns the total number of frames in the spritesheet
        int GetFrameCount() const { return m_columns * m_rows; }

        int GetColumns() const { return m_columns; }
        int GetRows() const { return m_rows; }

        // Sets the RGBA multiplication color
        void SetTint(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255);

        // Helper to quickly change only the transparency (0.0f to 1.0f)
        void SetOpacity(float alpha);

        // Instantly toggle rendering on or off without destroying the object
        void SetVisible(bool visible) { m_isVisible = visible; }
        bool IsVisible() const { return m_isVisible; }

        std::string GetName() const override;

#ifndef STANDALONE_MODE
        void OnInspector() override;
#endif

        nlohmann::json Serialize() const override;
        void Deserialize(const nlohmann::json& j) override;

        // Calculates the specific source rectangle for the current frame
        Rectangle GetSourceRec() const;

    private:
        UUID m_textureUUID = 0;
    
        Texture2D m_texture;
        Transform2d* m_transform;

        int m_columns;
        int m_rows;
        int m_currentFrame;

        // Internal visual state
        Color m_tint = WHITE; // Defaults to pure white (no color alteration)
        bool m_isVisible = true;
};
