#pragma once

#include <vector>
#include <memory>
#include <string>
#include <raylib.h>
#include <nlohmann/json.hpp>

// Forward declaration is enough here, or direct include since they are deeply linked
#include "GameObject.hpp"

class Scene {
    public:
        // Factory method to create a new GameObject inside this scene
        GameObject* CreateGameObject(const std::string& name);

        // Read-only access to the game objects for the Editor
        // Returns a constant reference to the vector, preventing external modifications
        const std::vector<std::unique_ptr<GameObject>>& GetGameObjects() const {
            return m_gameObjects;
        }

        void Start();

        // Runs game logic
        void Update(float deltaTime);

        // Memory management separated from game logic
        // Safely adds pending objects and removes destroyed ones
        void Flush();

        // Triggers the render loop for all GameObjects in the scene based on Z-Index
        void Render();

        void RenderHUD();

        void Clear(bool keepPersistent = true);

        // Saves the serialized scene to a JSON file on the disk
        void SaveToFile(const std::string& filepath) const;

        // Loads a scene from a JSON file, supporting both Editor and Standalone VFS modes
        void LoadFromFile(const std::string& filepath, bool keepPersistent = true);

        nlohmann::json Serialize() const;

        void Deserialize(const nlohmann::json& j, bool keepPersistent = true);

        // Instantiates a new GameObject (or a full hierarchy) from a JSON prefab file
        GameObject* Instantiate(const std::string& prefabPath);

        // Returns the top-most GameObject at the given world position, or nullptr if empty
        GameObject* PickObject(Vector2 worldPos);

        // Remove the object from the scene but returns its ownership without destroying it
        std::unique_ptr<GameObject> ExtractGameObject(GameObject* target);

        // Puts an existing object back into the scene
        void InjectGameObject(std::unique_ptr<GameObject> object);

        // Finds the primary Camera2d in the scene and returns its Raylib equivalent
        Camera2D GetMainCamera(float screenWidth, float screenHeight) const;

        // Retrieves the background color defined by the main camera
        Color GetMainCameraBackgroundColor() const;

        // Searches for a GameObject by its exact name across the entire scene
        GameObject* FindObjectByName(const std::string& name);

        // Searches for all active GameObjects matching the given tag
        std::vector<GameObject*> FindObjectsWithTag(const std::string& targetTag);

    private:
        bool m_isRunning = false;

        // Recursive function to draw the HUD hierarchy
        void RenderHUDHierarchical(GameObject* node);

        // The scene owns the GameObjects
        std::vector<std::unique_ptr<GameObject>> m_gameObjects;

        // The waiting room for newly instantiated objects
        std::vector<std::unique_ptr<GameObject>> m_pendingObjects;
};
