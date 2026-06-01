#include "AssetManager.hpp"

#include "graphics/Graphics.hpp"
#include "core/assets/AssetRegistry.hpp"

#include <filesystem>
#include <vector>
#include <iostream>

std::unordered_map<std::string, Texture2D> AssetManager::s_textures;
std::unordered_map<std::string, Font> AssetManager::s_fonts;

Texture2D AssetManager::GetTexture(const std::string& filepath) {
    auto it = s_textures.find(filepath);

    if (it != s_textures.end()) {
        // Texture found, return the cached version
        return it->second;
    }

    // Texture not found, load it from the disk
    Texture2D texture = Graphics::LoadTextureFiltered(filepath);

    // Validate that the texture loaded correctly (Raylib sets id to 0 on failure)
    if (texture.id == 0) {
        std::cerr << "[AssetManager] Failed to load texture: " << filepath << std::endl;
    } else {
        std::cout << "[AssetManager] Loaded new texture into VRAM: " << filepath << std::endl;
        // Store it in the cache for future requests
        s_textures[filepath] = texture;
    }

    return texture;
}

Font AssetManager::GetFont(const std::string& filepath) {
    // 1. Check if the font is already loaded in our cache
    auto it = s_fonts.find(filepath);
    if (it != s_fonts.end()) {
        return it->second;
    }

    Font newFont = {0};

    // 2. Check if we are running in Standalone (VFS mode) or Editor mode
    if (AssetRegistry::IsPacked()) {
        // Fetch the raw bytes from the data.pak archive
        std::vector<unsigned char> fileData = AssetRegistry::GetFileData(filepath);
        
        if (!fileData.empty()) {
            // Raylib needs the extension to know how to decode the font format (.ttf or .otf)
            std::string ext = std::filesystem::path(filepath).extension().string();
            
            // Load from RAM. We match the LoadFontEx parameters here:
            // 128 = base font size (high res for crisp scaling)
            // nullptr = default character map
            // 250 = glyph count
            newFont = LoadFontFromMemory(ext.c_str(), fileData.data(), fileData.size(), 128, nullptr, 250);
        } else {
            std::cerr << "[AssetManager] Failed to load font from VFS: " << filepath << std::endl;
        }
    } 
    else {
        // Standard Editor Mode: Load directly from the physical disk
        newFont = LoadFontEx(filepath.c_str(), 128, 0, 250);
    }

    // 3. Verify it loaded correctly (Raylib returns texture ID 0 on failure)
    if (newFont.texture.id != 0) {
        // Use Bilinear filtering for smooth modern fonts instead of pixelated edges
        SetTextureFilter(newFont.texture, TEXTURE_FILTER_BILINEAR); 
        
        // Cache it for future use so we don't re-parse the file/memory again
        s_fonts[filepath] = newFont;
        std::cout << "[AssetManager] Loaded Font into VRAM: " << filepath << std::endl;
        return newFont;
    }

    // Fallback if loading failed (corrupted file, wrong path, etc.)
    std::cerr << "[AssetManager] Error: Failed to load font at " << filepath << ", falling back to default." << std::endl;
    return GetFontDefault();
}

void AssetManager::Clear() {
    // Safely unload all stored textures from the GPU
    for (auto& pair : s_textures) {
        UnloadTexture(pair.second);
    }
    s_textures.clear();

    // Unload all Fonts
    for (auto& pair : s_fonts) {
        UnloadFont(pair.second);
    }
    s_fonts.clear();

    std::cout << "[AssetManager] Cleared all textures from VRAM." << std::endl;
}
