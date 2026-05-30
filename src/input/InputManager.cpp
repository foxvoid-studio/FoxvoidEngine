#include "InputManager.hpp"
#include "core/AssetRegistry.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <filesystem>

std::unordered_map<std::string, std::vector<int>> InputManager::s_bindings;
int InputManager::s_previousTouchCount = 0;

void InputManager::Update() {
    // Keep track of how many fingers were on screen last frame
    s_previousTouchCount = GetTouchPointCount();
}

void InputManager::BindKey(const std::string& actionName, int key) {
    // Check if the key is already bound to avoid duplicates
    auto& keys = s_bindings[actionName];
    if (std::find(keys.begin(), keys.end(), key) == keys.end()) {
        keys.push_back(key);
    }
}

void InputManager::UnbindKey(const std::string& actionName, int key) {
    if (s_bindings.find(actionName) != s_bindings.end()) {
        auto& keys = s_bindings[actionName];
        keys.erase(std::remove(keys.begin(), keys.end(), key), keys.end());
    }
}

bool InputManager::CheckInputDown(int inputCode) {
    // Mouse & Primary Touch (Touch 1)
    // Raylib's internal event queue handles the first touch as a left mouse click perfectly.
    // This prevents us from missing extremely fast taps between frames.
    if (inputCode == INPUT_MOUSE_LEFT || inputCode == INPUT_TOUCH_1) return IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    if (inputCode == INPUT_MOUSE_RIGHT)  return IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    if (inputCode == INPUT_MOUSE_MIDDLE) return IsMouseButtonDown(MOUSE_BUTTON_MIDDLE);
    
    // Secondary Touches
    int currentTouches = GetTouchPointCount();
    if (inputCode == INPUT_TOUCH_2) return currentTouches >= 2;
    if (inputCode == INPUT_TOUCH_3) return currentTouches >= 3;

    // Keyboard (Fallback)
    return IsKeyDown(inputCode);
}

bool InputManager::CheckInputPressed(int inputCode) {
    // Mouse & Primary Touch (Touch 1)
    // Using IsMouseButtonPressed guarantees we catch fast 1-frame taps.
    if (inputCode == INPUT_MOUSE_LEFT || inputCode == INPUT_TOUCH_1) return IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    if (inputCode == INPUT_MOUSE_RIGHT)  return IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
    if (inputCode == INPUT_MOUSE_MIDDLE) return IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE);
    
    // Secondary Touches (Did this specific finger JUST touch the screen this frame?)
    int currentTouches = GetTouchPointCount();
    if (inputCode == INPUT_TOUCH_2) return currentTouches >= 2 && s_previousTouchCount < 2;
    if (inputCode == INPUT_TOUCH_3) return currentTouches >= 3 && s_previousTouchCount < 3;

    // Keyboard (Fallback)
    return IsKeyPressed(inputCode);
}

bool InputManager::IsActionDown(const std::string& actionName) {
    if (s_bindings.find(actionName) != s_bindings.end()) {
        for (int key : s_bindings[actionName]) {
            if (CheckInputDown(key)) {
                return true; // Return true as soon as ONE bound key is down
            }
        }
    }
    return false;
}

bool InputManager::IsActionPressed(const std::string& actionName) {
    if (s_bindings.find(actionName) != s_bindings.end()) {
        for (int key : s_bindings[actionName]) {
            if (CheckInputPressed(key)) {
                return true;
            }
        }
    }
    return false;
}

std::unordered_map<std::string, std::vector<int>>& InputManager::GetBindings() {
    return s_bindings;
}

std::string InputManager::GetKeyName(int key) {
    if (key == INPUT_MOUSE_LEFT) return "Mouse Left";
    if (key == INPUT_MOUSE_RIGHT) return "Mouse Right";
    if (key == INPUT_MOUSE_MIDDLE) return "Mouse Middle";
    
    if (key == INPUT_TOUCH_1) return "Touch 1 (Main)";
    if (key == INPUT_TOUCH_2) return "Touch 2 (Secondary)";
    if (key == INPUT_TOUCH_3) return "Touch 3";
    
    if (key >= 32 && key <= 126) {
        std::string name(1, (char)key);
        return "Key: " + name;
    }
    
    if (key == KEY_SPACE) return "Space";
    if (key == KEY_ENTER) return "Enter";
    if (key == KEY_ESCAPE) return "Escape";
    if (key == KEY_UP) return "Up Arrow";
    if (key == KEY_DOWN) return "Down Arrow";
    if (key == KEY_LEFT) return "Left Arrow";
    if (key == KEY_RIGHT) return "Right Arrow";

    return "Key Code: " + std::to_string(key);
}

void InputManager::Save(const std::string& filepath) {
    nlohmann::json j;
    
    // Convert the unordered_map into a JSON object
    for (const auto& pair : s_bindings) {
        j[pair.first] = pair.second;
    }

    // Ensute the parent directory exists before trying to save
    std::filesystem::path pathObj(filepath);
    if (pathObj.has_parent_path()) {
        std::filesystem::create_directories(pathObj.parent_path());
    }

    std::ofstream file(filepath);
    if (file.is_open()) {
        file << j.dump(4);
        file.close();
        std::cout << "[InputManager] Saved bindings to " << filepath << std::endl;
    }
}

void InputManager::Load(const std::string& filepath) {
    nlohmann::json j;
    bool loadSuccess = false;

    // Check if we are running in packed (VFS) mode
    if (AssetRegistry::IsPacked()) {
        std::vector<unsigned char> fileData = AssetRegistry::GetFileData(filepath);
        
        if (!fileData.empty()) {
            try {
                j = nlohmann::json::parse(fileData.begin(), fileData.end());
                loadSuccess = true;
            } catch (const nlohmann::json::parse_error& e) {
                std::cerr << "[InputManager] VFS JSON parsing error in " << filepath << ":\n" << e.what() << std::endl;
            }
        } else {
            std::cerr << "[InputManager] Could not find " << filepath << " in VFS." << std::endl;
        }
    } 
    else {
        // Standard Editor Mode: Load from disk
        std::ifstream file(filepath);
        if (file.is_open()) {
            try {
                file >> j;
                loadSuccess = true;
            } catch (const nlohmann::json::parse_error& e) {
                std::cerr << "[InputManager] JSON parsing error in " << filepath << ":\n" << e.what() << std::endl;
            }
            file.close();
        } else {
            std::cerr << "[InputManager] Could not open " << filepath << " (It may not exist yet)." << std::endl;
        }
    }

    // Apply the parsed bindings if successful
    if (loadSuccess) {
        s_bindings.clear();
        try {
            for (auto& el : j.items()) {
                std::vector<int> keys;
                for (auto& keyJson : el.value()) {
                    keys.push_back(keyJson.get<int>());
                }
                s_bindings[el.key()] = keys;
            }
            std::cout << "[InputManager] Loaded bindings from " << (AssetRegistry::IsPacked() ? "VFS: " : "Disk: ") << filepath << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[InputManager] Error mapping JSON data: " << e.what() << std::endl;
        }
    }
}
