#pragma once

#include <string>
#include <filesystem>
#include <nlohmann/json.hpp>

class ProjectSettings {
    public:
        // Attempts to load an existing project from a project.json file
        // Returns true if successful, false otherwise.
        static bool Load(const std::filesystem::path& projectFilePath);

        // Generates a new project directory structure and a default project.json
        static bool CreateNewProject(const std::filesystem::path& rootDirectory, const std::string& projectName);

        // Getters
        static std::string GetProjectName();
        static int GetWindowWidth();
        static int GetWindowHeight();

        // Build Settings
        static std::string GetStartScenePath();
        static void SetStartScenePath(const std::string& path);

        // Engine Directory Tracking
        static std::filesystem::path GetEngineRoot();
        static void SetEngineRoot(const std::filesystem::path& path);

        // Saves the current JSON configuration back to the project.json file on disk
        static bool Save();

        // Returns the root directory of the currently loaded project
        static std::filesystem::path GetProjectRoot();

        // Helper to get the full path to the asseets folder of the current project
        static std::filesystem::path GetAssetsPath();

    private:
        // Stores the parsed JSON configuration in memory
        static nlohmann::json s_config;

        // Store the absolute path to the project's root folder
        static std::filesystem::path s_projectRoot;

        // Stores the original path where the engine was launched
        static std::filesystem::path s_engineRoot;
};
