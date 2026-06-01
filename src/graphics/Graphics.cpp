#include "Graphics.hpp"
#include "core/assets/AssetRegistry.hpp"
#include <vector>
#include <filesystem>
#include <iostream>

// We default to false.
bool Graphics::pixelArtMode = false; 

Texture2D Graphics::LoadTextureFiltered(const std::string& path) {
    Texture2D texture = {0}; // Empty texture fallback

    if (AssetRegistry::IsPacked()) {
        // 1. Fetch the raw bytes from the .pak file
        std::vector<unsigned char> fileData = AssetRegistry::GetFileData(path);
        
        if (!fileData.empty()) {
            // 2. Raylib needs the file extension to know how to decode the bytes (PNG, JPG, etc.)
            std::string ext = std::filesystem::path(path).extension().string();
            
            // 3. Load from RAM!
            Image img = LoadImageFromMemory(ext.c_str(), fileData.data(), fileData.size());
            texture = LoadTextureFromImage(img);
            UnloadImage(img);
        }
    } else {
        // Standard Editor Mode: Load directly from the physical hard drive
        texture = LoadTexture(path.c_str());
    }

    // Prevent Texture Edge Bleeding
    if (texture.id != 0) {
        SetTextureWrap(texture, TEXTURE_WRAP_CLAMP);

        // Apply the correct filter
        if (pixelArtMode) {
            SetTextureFilter(texture, TEXTURE_FILTER_POINT);
        } else {
            SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
        }
    } else {
        std::cerr << "[Graphics] Failed to load texture: " << path << std::endl;
    }

    return texture;
}
