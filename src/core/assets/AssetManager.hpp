#pragma once

#include <string>
#include <unordered_map>
#include <raylib.h>

class AssetManager {
    public:
        // Retrieves a texture. If it's not loaded yet, loads it from disk.
        static Texture2D GetTexture(const std::string& filepath);

        // Retrieves a font. If it's not loaded yet, loads it from disk.
        static Font GetFont(const std::string& filepath);

        // Unloads all textures from VRAM. Call this on engine shutdown.
        static void Clear();

    private:
        // Private constructor to prevent instantiation
        AssetManager() = default;

        // Cache storing filepaths and their corresponding Raylib textures
        static std::unordered_map<std::string, Texture2D> s_textures;
        static std::unordered_map<std::string, Font> s_fonts;
};
