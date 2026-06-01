#include "ProjectSettings.hpp"

#include "core/assets/AssetRegistry.hpp"
#include "scripting/PythonStubs.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

// Initialize static members
nlohmann::json ProjectSettings::s_config;
fs::path ProjectSettings::s_projectRoot;
fs::path ProjectSettings::s_engineRoot;

bool ProjectSettings::Load(const fs::path& projectFilePath) {
    std::string absolutePath = fs::absolute(projectFilePath).lexically_normal().string();

    try {
        if (AssetRegistry::IsPacked()) {
            std::vector<unsigned char> data = AssetRegistry::GetFileData(absolutePath);
            if (data.empty()) return false;
            
            s_config = nlohmann::json::parse(data.begin(), data.end());
            s_projectRoot = fs::path(absolutePath).parent_path();
            std::cout << "[ProjectSettings] Loaded project from VFS." << std::endl;
            
            return true;
        }

        // Check if the provided project configuration file actually exists on disk
        if (!fs::exists(projectFilePath)) {
            std::cerr << "[ProjectSettings] Error: File not found at " << projectFilePath << std::endl;
            return false;
        }

        // Open the file stream for reading
        std::ifstream file(projectFilePath);
        if (!file.is_open()) return false;

        try {
            // Parse the JSON file content into our static config object
            file >> s_config;
            
            // Store the directory containing the project.json file as the absolute project root
            s_projectRoot = projectFilePath.parent_path();
            std::cout << "[ProjectSettings] Successfully loaded project: " << GetProjectName() << std::endl;

            // Python Stubs Auto-Generation
            // Ensure the Python type hinting file (__init__.pyi) is always up to date 
            // when a project is loaded, so the IDE has the latest Engine API definitions.
            std::filesystem::path foxvoidDir = GetAssetsPath() / "scripts" / "foxvoid";
            std::filesystem::path pyiFile = foxvoidDir / "__init__.pyi";

            bool needsUpdate = true;

            // Check if the stubs file already exists
            if (std::filesystem::exists(pyiFile)) {
                // Open the file in binary mode to prevent Windows from converting \n to \r\n, 
                // which would cause the exact string comparison to fail unnecessarily.
                std::ifstream inFile(pyiFile, std::ios::binary); 
                if (inFile.is_open()) {
                    // Read the entire file content into a string buffer
                    std::stringstream buffer;
                    buffer << inFile.rdbuf();
                    
                    // Compare existing file content with the hardcoded expected C++ content
                    if (buffer.str() == FOXVOID_PYI_CONTENT) {
                        needsUpdate = false; // The file is already perfectly up to date
                    }
                    inFile.close();
                }
            } else {
                // If it doesn't exist, safely create the directory structure first
                std::filesystem::create_directories(foxvoidDir);
            }

            // Write or overwrite the file if an update is required
            if (needsUpdate) {
                // ios::trunc ensures any old content is completely wiped before writing
                std::ofstream outFile(pyiFile, std::ios::trunc | std::ios::binary); 
                if (outFile.is_open()) {
                    outFile << FOXVOID_PYI_CONTENT;
                    outFile.close();
                    std::cout << "[ProjectSettings] Successfully updated Python stubs at: " << pyiFile << std::endl;
                }
            }

            return true;
        } catch (const std::exception& e) {
            // Catch any JSON parsing errors or filesystem exceptions to prevent crashing
            std::cerr << "[ProjectSettings] JSON Parse Error: " << e.what() << std::endl;
            return false;
        }
    } 
    catch (...) {
        return false;
    }
}

bool ProjectSettings::CreateNewProject(const fs::path& rootDirectory, const std::string& projectName) {
    try {
        // Define the main folder for the new project
        fs::path projectFolder = rootDirectory / projectName;

        // Generate the standard directory structure
        fs::create_directories(projectFolder / "assets" / "scenes");
        fs::create_directories(projectFolder / "assets" / "scripts");
        fs::create_directories(projectFolder / "assets" / "textures");
        fs::create_directories(projectFolder / "assets" / "audio");
        fs::create_directories(projectFolder / "assets" / "settings");

        fs::path stubsFolder = projectFolder / "assets" / "scripts" / "foxvoid";
        fs::create_directories(stubsFolder);

        std::ofstream stubFile(stubsFolder / "__init__.pyi");
        if (stubFile.is_open()) {
            stubFile << FOXVOID_PYI_CONTENT;
            stubFile.close();
        }

        // Generate a default .gitignore file
        fs::path gitignorePath = projectFolder / ".gitignore";
        std::ofstream gitignoreFile(gitignorePath);
        if (gitignoreFile.is_open()) {
            gitignoreFile << "# Foxvoid Engine - Auto-generated .gitignore\n\n";

            gitignoreFile << "# Engine API Stubs (Auto-generated by the engine)\n";
            gitignoreFile << "assets/scripts/foxvoid/\n\n";

            gitignoreFile << "# Python Cache\n";
            gitignoreFile << "**/__pycache__/\n";
            gitignoreFile << "**/*.pyc\n\n";

            gitignoreFile << "# Build Artifacts\n";
            gitignoreFile << "build/\n";
            gitignoreFile << "build_standalone/\n";

            gitignoreFile << "# Crash Logs\n";
            gitignoreFile << "crash.log\n";
            
            gitignoreFile.close();
        }

        // Create the default project.json configuration
        nlohmann::json newConfig = {
            {"project", {
                {"name", projectName},
                {"version", "1.0.0"}
            }},
            {"display", {
                {"width", 1280},
                {"height", 720}
            }}
        };

        fs::path jsonPath = projectFolder / "project.json";
        std::ofstream file(jsonPath);

        if (file.is_open()) {
            // Write the JSON to disk with a 4-space indentation for readability
            file << newConfig.dump(4);
            file.close();

            // Immediately load the newly created project into memory
            return Load(jsonPath);
        }
    }
    catch (const std::exception& e) {
        std::cerr << "[ProjectSettings] Error creating project " << e.what() << std::endl;
    }

    return false;
}

std::string ProjectSettings::GetProjectName() {
    if (s_config.contains("project") && s_config["project"].contains("name")) {
        return s_config["project"]["name"];
    }
    return "New Foxvoid Game";
}

int ProjectSettings::GetWindowWidth() {
    if (s_config.contains("display") && s_config["display"].contains("width")) {
        return s_config["display"]["width"];
    }
    return 1280; // Default fallback width
}

int ProjectSettings::GetWindowHeight() {
    if (s_config.contains("display") && s_config["display"].contains("height")) {
        return s_config["display"]["height"];
    }
    return 720; // Default fallback height
}

std::string ProjectSettings::GetStartScenePath() {
    if (s_config.contains("build") && s_config["build"].contains("start_scene")) {
        return s_config["build"]["start_scene"];
    }
    return "";
}

void ProjectSettings::SetStartScenePath(const std::string& path) {
    // If the "build" object doesn't exist yet, we create it
    if (!s_config.contains("build")) {
        s_config["build"] = nlohmann::json::object();
    }
    
    // Update the value in memory
    s_config["build"]["start_scene"] = path;
}

fs::path ProjectSettings::GetEngineRoot() { 
    return s_engineRoot; 
}

void ProjectSettings::SetEngineRoot(const fs::path& path) { 
    s_engineRoot = path; 
}

bool ProjectSettings::Save() {
    // Safety check to ensure a project is actually loaded
    if (s_projectRoot.empty()) {
        std::cerr << "[ProjectSettings] Cannot save: No project is currently loaded." << std::endl;
        return false;
    }

    fs::path jsonPath = s_projectRoot / "project.json";
    std::ofstream file(jsonPath);

    if (file.is_open()) {
        // Write the JSON back to the file with a 4-space indent
        file << s_config.dump(4);
        file.close();
        std::cout << "[ProjectSettings] Successfully saved project.json" << std::endl;
        return true;
    }

    std::cerr << "[ProjectSettings] Failed to open project.json for writing." << std::endl;
    return false;
}

fs::path ProjectSettings::GetProjectRoot() { 
    return s_projectRoot; 
}

fs::path ProjectSettings::GetAssetsPath() { 
    return s_projectRoot / "assets"; 
}
