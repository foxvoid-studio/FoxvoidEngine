#include "GameStateManager.hpp"

#include "core/assets/AssetRegistry.hpp"

#include <fstream>
#include <iostream>

// Initialize static members
std::unordered_map<std::string, int> GameStateManager::Ints;
std::unordered_map<std::string, float> GameStateManager::Floats;
std::unordered_map<std::string, bool> GameStateManager::Bools;
std::unordered_map<std::string, std::string> GameStateManager::Strings;

void GameStateManager::Load(const std::string& path) {
    Ints.clear();
    Floats.clear();
    Bools.clear();
    Strings.clear();

    nlohmann::json j;
    bool loadSuccess = false;
    
    // Check if we are running in packed (VFS) mode
    if (AssetRegistry::IsPacked()) {
        std::vector<unsigned char> fileData = AssetRegistry::GetFileData(path);
        
        if (!fileData.empty()) {
            try {
                // Parse directly from memory iterators
                j = nlohmann::json::parse(fileData.begin(), fileData.end());
                loadSuccess = true;
            } catch (const nlohmann::json::parse_error& e) {
                std::cerr << "[GameState] VFS JSON parsing error in " << path << ":\n" << e.what() << std::endl;
            }
        }
    } 
    else {
        // Standard Editor Mode: Load from disk
        std::ifstream file(path);
        if (file.is_open()) {
            try {
                file >> j;
                loadSuccess = true;
            } catch (const nlohmann::json::parse_error& e) {
                std::cerr << "[GameState] JSON parsing error in " << path << ":\n" << e.what() << std::endl;
            }
            file.close();
        }
    }

    // Apply the parsed data if successful
    if (loadSuccess) {
        try {
            if (j.contains("Ints")) Ints = j["Ints"].get<std::unordered_map<std::string, int>>();
            if (j.contains("Floats")) Floats = j["Floats"].get<std::unordered_map<std::string, float>>();
            if (j.contains("Bools")) Bools = j["Bools"].get<std::unordered_map<std::string, bool>>();
            if (j.contains("Strings")) Strings = j["Strings"].get<std::unordered_map<std::string, std::string>>();
            
            std::cout << "[GameState] Loaded globals from " << (AssetRegistry::IsPacked() ? "VFS: " : "Disk: ") << path << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[GameState] Error mapping JSON data: " << e.what() << std::endl;
        }
    }
}

void GameStateManager::Save(const std::string& path) {
    nlohmann::json j;
    j["Ints"] = Ints;
    j["Floats"] = Floats;
    j["Bools"] = Bools;
    j["Strings"] = Strings;

    std::ofstream file(path);
    if (file.is_open()) {
        // Pretty print with an indent of 4 spaces
        file << j.dump(4);
    }
}
