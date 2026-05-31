#include "AssetRegistry.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <cstring>

#ifdef __ANDROID__
#include <raylib.h>
#endif

// Initialize static members
std::unordered_map<uint64_t, fs::path> AssetRegistry::s_Registry;
std::unordered_map<std::string, uint64_t> AssetRegistry::s_PathToUUID;
fs::path AssetRegistry::s_AssetsDirectory = "";

// Initialize VFS statics
bool AssetRegistry::s_IsPacked = false;
std::unordered_map<std::string, PakEntry> AssetRegistry::s_PakToc;
fs::path AssetRegistry::s_PakFilePath = "";

std::vector<unsigned char> AssetRegistry::s_PakMemory;

void AssetRegistry::Initialize(const fs::path& assetsDirectory) {
    s_AssetsDirectory = assetsDirectory;

    if (!s_IsPacked) {
        Refresh();
    }
}

void AssetRegistry::MountVFS(const fs::path& executableDirectory) {
#ifdef __ANDROID__
    // On Android, assets are packed inside the APK. We use Raylib to extract it.
    // Raylib assumes the root is the "assets" folder inside the APK.
    s_PakFilePath = "data.pak";
    
    int pakSize = 0;
    std::cout << "[AssetRegistry] Attempting to load data.pak via Raylib from APK..." << std::endl;
    unsigned char* fileData = LoadFileData(s_PakFilePath.string().c_str(), &pakSize);
    
    if (fileData != nullptr) {
        s_IsPacked = true;
        std::cout << "[AssetRegistry] VFS Mounted from RAM (Android). Size: " << pakSize << " bytes" << std::endl;

        // Copy the raw C-array from Raylib into our C++ vector for safe keeping
        s_PakMemory.assign(fileData, fileData + pakSize);
        UnloadFileData(fileData); // Free Raylib's internal memory

        // Parse the number of files (First 4 bytes)
        uint32_t numFiles = 0;
        std::memcpy(&numFiles, s_PakMemory.data(), sizeof(uint32_t));

        // Read the Table of Contents (TOC) directly from our RAM buffer
        std::vector<PakEntry> toc(numFiles);
        size_t offset = sizeof(uint32_t);
        std::memcpy(toc.data(), s_PakMemory.data() + offset, numFiles * sizeof(PakEntry));

        for (const auto& entry : toc) {
            // Recreate the absolute path standard expected by the engine
            std::string absolutePath = fs::absolute(executableDirectory / entry.path).lexically_normal().string();
            
            s_PakToc[absolutePath] = entry;
            s_PathToUUID[absolutePath] = entry.uuid;
            if (entry.uuid != 1) { 
                s_Registry[entry.uuid] = absolutePath;
            }
        }
    } else {
        std::cerr << "[AssetRegistry] FATAL ERROR: Could not load data.pak from Android assets!" << std::endl;
    }

#else
    s_PakFilePath = executableDirectory / "data.pak";
    
    if (fs::exists(s_PakFilePath)) {
        s_IsPacked = true;
        std::cout << "[AssetRegistry] VFS Mounted: " << s_PakFilePath << std::endl;

        std::ifstream pakFile(s_PakFilePath, std::ios::binary);
        if (pakFile.is_open()) {
            uint32_t numFiles = 0;
            pakFile.read(reinterpret_cast<char*>(&numFiles), sizeof(uint32_t));

            std::vector<PakEntry> toc(numFiles);
            pakFile.read(reinterpret_cast<char*>(toc.data()), numFiles * sizeof(PakEntry));

            for (const auto& entry : toc) {
                // On recrée le chemin absolu tel que le moteur s'y attend
                std::string absolutePath = fs::absolute(executableDirectory / entry.path).lexically_normal().string();
                
                s_PakToc[absolutePath] = entry;
                s_PathToUUID[absolutePath] = entry.uuid;
                if (entry.uuid != 1) { // Si ce n'est pas notre faux UUID du project.json
                    s_Registry[entry.uuid] = absolutePath;
                }
            }
            pakFile.close();
        }
    }
#endif
}

bool AssetRegistry::IsPacked() {
    return s_IsPacked;
}

std::vector<unsigned char> AssetRegistry::GetFileData(const std::string& path) {
    if (!s_IsPacked) return {}; // Failsafe

    std::string normalizedPath = fs::absolute(path).lexically_normal().string();
    auto it = s_PakToc.find(normalizedPath);

    if (it != s_PakToc.end()) {
#ifdef __ANDROID__
        // On Android, the pak is entirely in RAM. Just copy the needed bytes instantly.
        std::vector<unsigned char> buffer(it->second.size);
        std::memcpy(buffer.data(), s_PakMemory.data() + it->second.offset, it->second.size);
        return buffer;
#else
        // On Desktop, read the specific chunk from the hard drive to save RAM
        std::ifstream pakFile(s_PakFilePath, std::ios::binary);
        if (pakFile.is_open()) {
            // Jump instantly to the exact byte where our file starts in the .pak
            pakFile.seekg(it->second.offset);
            
            // Read exactly the size of our file
            std::vector<unsigned char> buffer(it->second.size);
            pakFile.read(reinterpret_cast<char*>(buffer.data()), it->second.size);
            
            return buffer;
        }
#endif
    }
    std::cerr << "[VFS] File not found in archive: " << path << std::endl;
    return {};
}

void AssetRegistry::Refresh() {
    s_Registry.clear();
    s_PathToUUID.clear();

    if (!fs::exists(s_AssetsDirectory)) {
        std::cerr << "[AssetRegistry] Assets directory does not exist: " << s_AssetsDirectory << std::endl;
        return;
    }

    std::cout << "[AssetRegistry] Scanning assets directory..." << std::endl;
    ProcessDirectory(s_AssetsDirectory);
    std::cout << "[AssetRegistry] Scan complete. Tracked assets: " << s_Registry.size() << std::endl;
}

void AssetRegistry::ProcessDirectory(const fs::path& directory) {
    for (const auto& entry : fs::directory_iterator(directory)) {
        if (entry.is_directory()) {
            if (entry.path().filename() == "__pycache__") continue;
            ProcessDirectory(entry.path());
        } 
        else {
            std::string extension = entry.path().extension().string();
            std::string filename = entry.path().filename().string();

            if (filename.empty() || filename[0] == '.') continue;
            if (extension == ".pyc") continue;
            if (extension == ".meta") continue;

            UUID assetUUID = LoadOrGenerateMetaFile(entry.path());
            
            s_Registry[(uint64_t)assetUUID] = entry.path();
            
            // Convert to absolute and standardized path before saving
            std::string normalizedPath = fs::absolute(entry.path()).lexically_normal().string();
            s_PathToUUID[normalizedPath] = (uint64_t)assetUUID;
        }
    }
}

UUID AssetRegistry::LoadOrGenerateMetaFile(const fs::path& assetPath) {
    fs::path metaPath = assetPath.string() + ".meta";
    
    if (fs::exists(metaPath)) {
        std::ifstream file(metaPath);
        if (file.is_open()) {
            nlohmann::json j;
            try {
                file >> j;
                if (j.contains("uuid")) {
                    return UUID(j["uuid"].get<uint64_t>());
                }
            } catch (const nlohmann::json::parse_error& e) {
                std::cerr << "[AssetRegistry] Failed to parse meta file: " << metaPath << " (" << e.what() << ")" << std::endl;
            }
        }
    }

    UUID newUUID;
    nlohmann::json j;
    j["uuid"] = (uint64_t)newUUID;

    std::ofstream outFile(metaPath);
    if (outFile.is_open()) {
        outFile << j.dump(4);
        outFile.close();
        std::cout << "[AssetRegistry] Generated new meta file for: " << assetPath.filename() << std::endl;
    } else {
        std::cerr << "[AssetRegistry] Failed to create meta file: " << metaPath << std::endl;
    }

    return newUUID;
}

fs::path AssetRegistry::GetPathForUUID(UUID uuid) {
    auto it = s_Registry.find((uint64_t)uuid);
    if (it != s_Registry.end()) {
        return it->second;
    }
    return fs::path(""); 
}

UUID AssetRegistry::GetUUIDForPath(const fs::path& path) {
    if (path.empty()) return UUID(0);

    // Convert the requested path to an absolute/standardized format to guarantee a match
    std::string normalizedPath = fs::absolute(path).lexically_normal().string();
    auto it = s_PathToUUID.find(normalizedPath);
    
    if (it != s_PathToUUID.end()) {
        return UUID(it->second);
    }

    // If we are in packed mode, DO NOT try to auto-register files from the hard drive!
    if (s_IsPacked) {
        std::cerr << "[VFS] Cannot resolve UUID dynamically in Standalone mode for: " << path << std::endl;
        return UUID(0);
    }

    // Just-In-Time (JIT) registration
    // If the file exists on disk but isn't in our registry yet, 
    // it means the user just added it via the OS explorer. Let's process it right now!
    if (fs::exists(normalizedPath) && !fs::is_directory(normalizedPath)) {
        // Ignore files that shouldn't have meta files
        std::string ext = fs::path(normalizedPath).extension().string();
        if (ext != ".meta" && ext != ".pyc" && fs::path(normalizedPath).filename().string()[0] != '.') {
            
            std::cout << "[AssetRegistry] Auto-registering new file: " << path.filename() << std::endl;
            
            UUID newUUID = LoadOrGenerateMetaFile(normalizedPath);
            s_Registry[(uint64_t)newUUID] = normalizedPath;
            s_PathToUUID[normalizedPath] = (uint64_t)newUUID;
            
            return newUUID;
        }
    }
    
    // Useful debug to know if a path failed to resolve
    std::cerr << "[AssetRegistry] Warning: Could not find UUID for path: " << normalizedPath << std::endl;
    return UUID(0); 
}
