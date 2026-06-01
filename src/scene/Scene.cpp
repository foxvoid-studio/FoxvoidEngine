#include "Scene.hpp"

// Standard library includes
#include <fstream>
#include <iostream>
#include <algorithm>
#include <unordered_map>

// Engine includes
#include "PersistentComponent.hpp"
#include "ComponentRegistry.hpp"
#include "math/Transform2d.hpp"
#include "physics/PhysicsEngine.hpp"
#include "graphics/renderers/ShapeRenderer.hpp"
#include "graphics/renderers/SpriteRenderer.hpp"
#include "graphics/renderers/SpriteSheetRenderer.hpp"
#include "graphics/animation/Animation2d.hpp"
#include "graphics/animation/Animator2d.hpp"
#include "graphics/Camera2d.hpp"
#include "scripting/ScriptComponent.hpp"
#include "scripting/ScriptBindings.hpp"
#include "ui/core/Mask.hpp"
#include "ui/core/RectTransform.hpp"
#include "core/assets/AssetRegistry.hpp"
#include "core/events/EventBus.hpp"

#ifndef STANDALONE_MODE
#include "editor/commands/CommandHistory.hpp"
#endif

// Factory method to create a new GameObject inside this scene
GameObject* Scene::CreateGameObject(const std::string& name) {
    auto go = std::make_unique<GameObject>(name);
    GameObject* ptr = go.get();
    
    // Instead of pushing to the main list directly and crashing the current loop,
    // we put the new object in a waiting room (pending objects).
    m_pendingObjects.push_back(std::move(go));

    return ptr;
}

void Scene::Start() {
    m_isRunning = true;

    for (auto& go : m_gameObjects) {
        go->Start();
    }
    
    for (auto& go : m_pendingObjects) {
        go->Start();
    }
}

// Runs game logic
void Scene::Update(float deltaTime) {
    // Update all objects
    for (auto& go : m_gameObjects) {
        if (!go->IsActiveInHierarchy()) continue;
        
        go->Update(deltaTime);
    }

    // Run the physics engine
    PhysicsEngine::Update(*this, deltaTime);
}

// Memory management separated from game logic
// Safely adds pending objects and removes destroyed ones
void Scene::Flush() {
    // GARBAGE COLLECTION (Deferred Destruction)
    std::erase_if(m_gameObjects, [](const std::unique_ptr<GameObject>& go) {
        return go->IsPendingDestroy();
    });

    // SECURE NEW OBJECT MANAGEMENT
    // We use a 'while' loop because a call to Start() might instantiate NEW objects!
    while (!m_pendingObjects.empty()) {
        
        // Transfer pending objects to a temporary local list
        std::vector<std::unique_ptr<GameObject>> tempPending;
        tempPending.swap(m_pendingObjects); // m_pendingObjects is now empty

        for (auto& newGo : tempPending) {
            // Only call Start() if the game is in "Play" mode
            if (m_isRunning) {
                newGo->Start();
            }
            
            // Integrate the object into the official scene
            m_gameObjects.push_back(std::move(newGo));
        }
    }
}

// Triggers the render loop for all GameObjects in the scene based on Z-Index
void Scene::Render() {
    // Create a temporary list of raw pointers to sort
    std::vector<GameObject*> renderList;
    renderList.reserve(m_gameObjects.size());
    for (auto& go : m_gameObjects) {
        if (!go->IsActiveInHierarchy()) continue;

        renderList.push_back(go.get());
    }

    // Sort the list by Z-Index (Ascending: lower Z is drawn first, higher Z is drawn on top)
    // std::stable_sort is crucial here: it preserves the creation order for objects that have the SAME Z-Index
    std::stable_sort(renderList.begin(), renderList.end(), [](GameObject* a, GameObject* b) {
        int zA = 0;
        int zB = 0;
        
        auto tA = a->GetComponent<Transform2d>();
        auto tB = b->GetComponent<Transform2d>();
        
        if (tA) zA = tA->zIndex;
        if (tB) zB = tB->zIndex;
        
        return zA < zB;
    });

    // Render the sorted list
    for (auto* go : renderList) {
        go->Render();
    }
}

void Scene::RenderHUD() {
    for (auto& go : m_gameObjects) {
        if (go->GetParent() == nullptr && go->IsActiveInHierarchy()) {
            RenderHUDHierarchical(go.get());
        }
    }
}

void Scene::Clear(bool keepPersistent) {
    m_isRunning = false;

#ifndef STANDALONE_MODE
    CommandHistory::Clear();
#endif

    // Clear all active events to prevent ghost callbacks
    EventBus::Clear();

    if (!keepPersistent) {
        // Editor stop mode: Destroy absolutely everything to restore the backup
        m_gameObjects.clear();
    }
    else {
        // Gameplay Mode: Keep objects that have the PersistentComponent
        // Use std::erase_if (C++20) to remove only non-persistent objects
        std::erase_if(m_gameObjects, [](const std::unique_ptr<GameObject>& go) {
            // If the object has a PersistentComponent, we return false (do NOT erase)
            if (go->GetComponent<PersistentComponent>()) {
                return false;
            }
            return true; // Otherwise, erase it
        });
    }

    m_pendingObjects.clear();
}

// Saves the serialized scene to a JSON file on the disk
void Scene::SaveToFile(const std::string& filepath) const {
    nlohmann::json j = Serialize();

    std::ofstream file(filepath);
    if (file.is_open()) {
        // dump(4) adds an indentation of 4 spaces for readability
        file << j.dump(4);
        file.close();
        std::cout << "[Scene] Successfully saved to: " << filepath << std::endl;
    } else {
        std::cerr << "[Scene] Failed to open file for saving: " << filepath << std::endl;
    }
}

// Loads a scene from a JSON file, supporting both Editor and Standalone VFS modes
void Scene::LoadFromFile(const std::string& filepath, bool keepPersistent) {
    nlohmann::json j;
    bool loadSuccess = false;

    // Check if we are running in packed (VFS) mode
    if (AssetRegistry::IsPacked()) {
        // Read raw bytes from the data.pak file
        std::vector<unsigned char> fileData = AssetRegistry::GetFileData(filepath);
        
        if (!fileData.empty()) {
            try {
                // nlohmann::json can parse directly from memory iterators!
                j = nlohmann::json::parse(fileData.begin(), fileData.end());
                loadSuccess = true;
            } catch (const nlohmann::json::parse_error& e) {
                std::cerr << "[Scene] JSON parsing error in VFS " << filepath << ":\n" << e.what() << std::endl;
            }
        } else {
            std::cerr << "[Scene] Failed to load file from VFS: " << filepath << std::endl;
        }
    } 
    else {
        // Standard Editor Mode: Load directly from disk
        std::ifstream file(filepath);
        if (file.is_open()) {
            try {
                file >> j;
                loadSuccess = true;
            } catch (const nlohmann::json::parse_error& e) {
                std::cerr << "[Scene] JSON parsing error in " << filepath << ":\n" << e.what() << std::endl;
            }
            file.close();
        } else {
            std::cerr << "[Scene] Failed to open file for loading: " << filepath << std::endl;
        }
    }

    // Only proceed with instantiation if the JSON was successfully parsed
    if (loadSuccess) {
        Deserialize(j, keepPersistent);
        std::cout << "[Scene] Successfully loaded from: " << filepath << std::endl;
    }
}

nlohmann::json Scene::Serialize() const {
    nlohmann::json j;
    j["gameObjects"] = nlohmann::json::array();
    for (const auto& go : m_gameObjects) {
        j["gameObjects"].push_back(go->Serialize());
    }
    return j;
}

void Scene::Deserialize(const nlohmann::json& j, bool keepPersistent) {
    Clear(keepPersistent); // Ensure the scene is totally empty before loading

    if (!j.contains("gameObjects")) return;

    for (const auto& goJson : j["gameObjects"]) {
        std::string name = goJson.value("name", "Entity");
        
        // Prevent duplicate Persistent Objects
        // Check if an object with the exact same name survived the Clear() process
        bool isDuplicate = false;
        for (const auto& existingGo : m_gameObjects) {
            if (existingGo->name == name && existingGo->GetComponent<PersistentComponent>()) {
                isDuplicate = true;
                std::cout << "[Scene] Prevented spawning duplicate persistent object: " << name << std::endl;
                break;
            }
        }

        // If it already exists, skip loading this JSON object entirely
        if (isDuplicate) {
            continue;
        }

        GameObject* go = CreateGameObject(name);

        go->Deserialize(goJson);
    }

    // Now that all objects exist in memory, we can safely link children to parents
    for (auto& go : m_pendingObjects) {
        if (go->pendingParentId != 0) { // If it's supposed to have a parent
            
            // Search for the parent in the pending list
            GameObject* foundParent = nullptr;
            for (auto& potentialParent : m_pendingObjects) {
                if (potentialParent->id == go->pendingParentId) {
                    foundParent = potentialParent.get();
                    break;
                }
            }

            // If not found in pending, check the persistent active objects
            if (!foundParent) {
                for (auto& potentialParent : m_gameObjects) {
                    if (potentialParent->id == go->pendingParentId) {
                        foundParent = potentialParent.get();
                        break;
                    }
                }
            }

            // Link them!
            if (foundParent) {
                go->SetParent(foundParent);
            } else {
                std::cerr << "[Scene] Warning: Parent ID " << go->pendingParentId 
                            << " not found for child " << go->name << std::endl;
            }
        }
    }

    Flush(); // Crucial: move them from pending to active!
}

// Instantiates a new GameObject (or a full hierarchy) from a JSON prefab file
GameObject* Scene::Instantiate(const std::string& prefabPath) {
    nlohmann::json j;
    bool loadSuccess = false;

    // Check if we are running in packed (VFS) mode
    if (AssetRegistry::IsPacked()) {
        std::vector<unsigned char> fileData = AssetRegistry::GetFileData(prefabPath);
        
        if (!fileData.empty()) {
            try {
                j = nlohmann::json::parse(fileData.begin(), fileData.end());
                loadSuccess = true;
            } catch (const nlohmann::json::parse_error& e) {
                std::cerr << "[Scene] Prefab JSON parsing error in VFS " << prefabPath << ":\n" << e.what() << std::endl;
            }
        } else {
            std::cerr << "[Scene] Error: Could not open prefab file from VFS: " << prefabPath << std::endl;
            return nullptr; // Fail early
        }
    } 
    else {
        // Standard Editor Mode: Load directly from disk
        std::ifstream file(prefabPath);
        if (!file.is_open()) {
            std::cerr << "[Scene] Error: Could not open prefab file: " << prefabPath << std::endl;
            return nullptr; // Fail early
        }
        
        try {
            file >> j;
            loadSuccess = true;
        } catch (const nlohmann::json::parse_error& e) {
            std::cerr << "[Scene] Prefab JSON parsing error in " << prefabPath << ":\n" << e.what() << std::endl;
        }
        file.close();
    }

    // Safety check before trying to instantiate objects
    if (!loadSuccess) {
        return nullptr;
    }

    // Backward Compatibility
    // If it's an old prefab (just a single object, not an array)
    if (!j.contains("gameObjects")) {
        std::string name = j.value("name", "New Prefab") + " (Clone)";
        GameObject* newObj = CreateGameObject(name);
        newObj->Deserialize(j);
        newObj->RegenerateID();
        newObj->pendingParentId = 0;
        return newObj;
    }
    
    // We need to keep track of Old IDs vs New IDs to rebuild the parent-child links
    std::unordered_map<uint64_t, uint64_t> idMapping;
    std::vector<GameObject*> instantiatedObjects;
    GameObject* rootObject = nullptr;

    // PASS 1: Create all objects and map their IDs
    for (const auto& goJson : j["gameObjects"]) {
        std::string name = goJson.value("name", "Prefab Object");
        
        // Identify the root object (the one with no parent in the prefab)
        bool isRoot = (goJson.value("parentId", 0ULL) == 0);
        if (isRoot) {
            name += " (Clone)";
        }

        GameObject* newObj = CreateGameObject(name);
        
        // Deserialize loads the components AND overwrites the ID with the old one from the JSON
        newObj->Deserialize(goJson); 
        
        uint64_t oldId = newObj->id;
        
        // Generate a fresh unique ID for this specific clone
        newObj->RegenerateID(); 
        uint64_t newId = newObj->id;
        
        // Save the mapping
        idMapping[oldId] = newId;
        instantiatedObjects.push_back(newObj);

        if (isRoot) {
            rootObject = newObj;
            newObj->pendingParentId = 0; // Force the root to spawn at the top level of the Scene
        }
    }

    // Pass 2: Re-link parents using the new generated IDs
    for (GameObject* obj : instantiatedObjects) {
        if (obj != rootObject && obj->pendingParentId != 0) {
            // If the parent was part of this prefab, update its pending ID to the newly cloned parent
            if (idMapping.find(obj->pendingParentId) != idMapping.end()) {
                uint64_t newParentId = idMapping[obj->pendingParentId];
                obj->pendingParentId = newParentId;

                for (GameObject* potentialParent : instantiatedObjects) {
                    if (potentialParent->id == newParentId) {
                        obj->SetParent(potentialParent);
                        break;
                    }
                }
            }
            else {
                // Edge case: If the parent wasn't found in the prefab, try finding it in the scene
                for (const auto& potentialParent : m_gameObjects) {
                    if (potentialParent->id == obj->pendingParentId) {
                        obj->SetParent(potentialParent.get());
                        break;
                    }
                }
            }
        }
    }

    std::cout << "[Scene] Successfully instantiated hierarchy prefab: " << prefabPath << std::endl;
    
    // Return the absolute root of the prefab
    return rootObject;
}

// Returns the top-most GameObject at the given world position, or nullptr if empty
GameObject* Scene::PickObject(Vector2 worldPos) {
    // Gather all objects
    std::vector<GameObject*> pickList;
    pickList.reserve(m_gameObjects.size());
    for (auto& go : m_gameObjects) {
        // We cannot click on invisible objects
        if (!go->IsActiveInHierarchy()) continue;

        pickList.push_back(go.get());
    }

    // Sort the list by Z-Index (Descending: Highest Z is checked first!)
    std::stable_sort(pickList.begin(), pickList.end(), [](GameObject* a, GameObject* b) {
        int zA = 0, zB = 0;
        auto tA = a->GetComponent<Transform2d>();
        auto tB = b->GetComponent<Transform2d>();
        if (tA) zA = tA->zIndex;
        if (tB) zB = tB->zIndex;
        return zA > zB; // Note the > sign for descending order
    });

    // Check for collisions in sorted order
    for (auto* go : pickList) {
        auto transform = go->GetComponent<Transform2d>();
        if (!transform) continue;

        auto position = transform->GetGlobalPosition();

        // Default bounding box
        Rectangle bounds = { position.x - 25.0f, position.y - 25.0f, 50.0f, 50.0f };

        // Try to get precise bounds from a SpriteRenderer
        auto sprite = go->GetComponent<SpriteRenderer>();
        if (sprite && sprite->GetTexture().id != 0) {
            float width = sprite->GetTexture().width * transform->scale.x;
            float height = sprite->GetTexture().height * transform->scale.y;
            bounds = { position.x - (width / 2.0f), position.y - (height / 2.0f), width, height };
        }

        // Try to get precise bounds from a SpriteSheetRenderer
        auto spriteSheet = go->GetComponent<SpriteSheetRenderer>();
        if (spriteSheet && spriteSheet->GetTexture().id != 0) {
            Rectangle sourceRec = spriteSheet->GetSourceRec();
            float width = sourceRec.width * std::abs(transform->scale.x);
            float height = sourceRec.height * std::abs(transform->scale.y);
            bounds = { position.x - (width / 2.0f), position.y - (height / 2.0f), width, height };
        }

        auto shape = go->GetComponent<ShapeRenderer>();
        if (shape) {
            float width = shape->width * std::abs(transform->scale.x);
            float height = shape->height * std::abs(transform->scale.y);
            bounds = { position.x - (width / 2.0f), position.y - (height / 2.0f), width, height };
        }

        // If the click is inside the bounds, return this object immediately
        if (CheckCollisionPointRec(worldPos, bounds)) {
            return go;
        }
    }

    return nullptr;
}

// Remove the object from the scene but returns its ownership without destroying it
std::unique_ptr<GameObject> Scene::ExtractGameObject(GameObject* target) {
    // Find the object in our container
    auto it = std::find_if(m_gameObjects.begin(), m_gameObjects.end(),
        [target](const std::unique_ptr<GameObject>& go) { return go.get() == target; });

    if (it != m_gameObjects.end()) {
        // Transfer ownership out of the vector
        std::unique_ptr<GameObject> extractedObject = std::move(*it);

        // Remove the empty pointer from the vector
        m_gameObjects.erase(it);

        return extractedObject;
    }

    return nullptr;
}

// Puts an existing object back into the scene
void Scene::InjectGameObject(std::unique_ptr<GameObject> object) {
    if (object) {
        // Put the object back into the scene
        m_gameObjects.push_back(std::move(object));
    }
}

// Finds the primary Camera2d in the scene and returns its Raylib equivalent
Camera2D Scene::GetMainCamera(float screenWidth, float screenHeight) const {
    for (const auto& go : m_gameObjects) {
        auto cam = go->GetComponent<Camera2d>();
        if (cam && cam->isMain) {
            return cam->GetCamera(screenWidth, screenHeight);
        }
    }

    // Fallback: If no camera exists in the scene, return a static default camera
    // so the game doesn't crash or render as a black screen.
    Camera2D defaultCam = { 0 };
    defaultCam.zoom = 1.0f;
    defaultCam.offset.x = screenWidth / 2.0f;
    defaultCam.offset.y = screenHeight / 2.0f;
    return defaultCam;
}

// Retrieves the background color defined by the main camera
Color Scene::GetMainCameraBackgroundColor() const {
    for (const auto& go : m_gameObjects) {
        auto cam = go->GetComponent<Camera2d>();
        if (cam && cam->isMain) {
            return cam->backgroundColor;
        }
    }
    // Fallback color if no main camera is found in the scene
    return RAYWHITE; 
}

// Searches for a GameObject by its exact name across the entire scene
GameObject* Scene::FindObjectByName(const std::string& name) {
    // Iterate through the list of GameObjects in the scene
    for (const auto& go : m_gameObjects) {
        if (go && go->name == name) {
            return go.get();
        }
    }

    return nullptr;
}

// Searches for all active GameObjects matching the given tag
std::vector<GameObject*> Scene::FindObjectsWithTag(const std::string& targetTag) {
    std::vector<GameObject*> results;

    // Reserve a small capacity to avoid immediate reallocations
    results.reserve(10);

    for (const auto& go : m_gameObjects) {
        // We only return objects that exist, are active, and have the correct tag
        if (go && go->IsActiveInHierarchy() && go->tag == targetTag) {
            results.push_back(go.get());
        }
    }

    return results;
}

// Recursive function to draw the HUD hierarchy
void Scene::RenderHUDHierarchical(GameObject* node) {
    // Safety check during recursion
    // If we reach a child that is explicitly disabled, we stop rendering it AND its descendants
    if (!node->isActive) return;

    bool isMasking = false;
    
    // 1. Check if this node is a Mask
    Mask* mask = node->GetComponent<Mask>();
    if (mask && mask->isActive) {
        // We need the RectTransform to know WHERE to cut
        if (RectTransform* rt = node->GetComponent<RectTransform>()) {
            Rectangle rect = rt->GetScreenRect();
            
            // Raylib requires integers for Scissor Mode
            BeginScissorMode((int)rect.x, (int)rect.y, (int)rect.width, (int)rect.height);
            isMasking = true;
        }
    }

    // 2. Render this specific node
    node->RenderHUD();

    // 3. Render all children recursively (Painter's Algorithm: children are drawn on top)
    for (GameObject* child : node->GetChildren()) {
        RenderHUDHierarchical(child);
    }

    // 4. Close the mask if we opened one, so it doesn't affect siblings
    if (isMasking) {
        EndScissorMode();
    }
}
