#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <raylib.h>
#include <nlohmann/json.hpp>

// Mouse inputs (1000+)
#define INPUT_MOUSE_LEFT   1001
#define INPUT_MOUSE_RIGHT  1002
#define INPUT_MOUSE_MIDDLE 1003

// Touch inputs (2000+)
#define INPUT_TOUCH_1      2001
#define INPUT_TOUCH_2      2002
#define INPUT_TOUCH_3      2003

class InputManager {
    public:
        // Binds a specific Raylib key to a named action
        static void BindKey(const std::string& actionName, int key);

        // Removes a specific key from an action
        static void UnbindKey(const std::string& actionName, int key);

        // Checks if any of the keys bound to this action are currently held down
        static bool IsActionDown(const std::string& actionName);

        // Checks if any of the keys bound to this action were pressed this exact frame
        static bool IsActionPressed(const std::string& actionName);

        // Get all bindings (useful for the Editor UI)
        static std::unordered_map<std::string, std::vector<int>>& GetBindings();

        static std::string GetKeyName(int key);

        // Serialization to save/load input settings from a file (e.g., "inputs.json")
        static void Save(const std::string& filepath);
        static void Load(const std::string& filepath);

        static void Update();

    private:
        static bool CheckInputDown(int inputCode);
        static bool CheckInputPressed(int inputCode);

        // The dictionnary mapping an Action Name (e.g., "Jump") to a list of Keys (e.g. [KEY_SPACE, KEY_W])
        static std::unordered_map<std::string, std::vector<int>> s_bindings;

        // Track the number of touches in the previous frame to simulate "IsTouchedPressed"
        static int s_previousTouchCount;
};
