#pragma once

#include <filesystem>
#include <unordered_map>
#include <string>

#include "core/utils/UUID.hpp"
#include "PakEntry.hpp"

namespace fs = std::filesystem;

class [[gnu::visibility("default")]] AssetRegistry {
    public:
        // Scans the asset directory, reading or generating .meta files
        static void Initialize(const fs::path& assetsDirectory);

        // Mounts the Virtual File System before the engine fully boots
        static void MountVFS(const fs::path& executableDirectory);

        // Rescans the directory (useful if the user adds files externally)
        static void Refresh();

        // Returns the path associated with a UUID. Returns empty path if not found.
        static fs::path GetPathForUUID(UUID uuid);

        // Returns the UUID associated with a file path. Returns 0 (invalid) if not found.
        static UUID GetUUIDForPath(const fs::path& path);

        // Checks if the game is running from a data.pak file
        static bool IsPacked();

        // Extracts a file directly from the .pak into RAM
        static std::vector<unsigned char> GetFileData(const std::string& path);

    private:
        static void ProcessDirectory(const fs::path& directory);
        static UUID LoadOrGenerateMetaFile(const fs::path& assetPath);

        // The core dictionaries mapping UUIDs to paths and vice versa
        static std::unordered_map<uint64_t, fs::path> s_Registry;
        static std::unordered_map<std::string, uint64_t> s_PathToUUID;
        
        static fs::path s_AssetsDirectory;

        static bool s_IsPacked;
        static std::unordered_map<std::string, PakEntry> s_PakToc;
        static fs::path s_PakFilePath;

        // In-memory buffer for Android APK asset reading
        static std::vector<unsigned char> s_PakMemory;
};
