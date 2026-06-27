#pragma once

#include <vector>
#include <memory>
#include <string>
#include <iostream>
#include <iostream>
#include "Component.hpp"

class GameObject {
    public:
        std::string name;

        // Tag used to group and identify objects (e.g., "Enemy", "Player", "Projectile")
        std::string tag = "Untagged";

        // Unique identifier for serialization and scene references
        uint64_t id;

        // Stores the parent ID loaded from JSON until the Scene links them
        uint64_t pendingParentId = 0;

        // Controls visibility and logic updates
        bool isActive = true;

        GameObject(const std::string& name);
        ~GameObject();

        GameObject(const GameObject&) = delete;
        GameObject& operator=(const GameObject&) = delete;

        // Marks the GameObject to be destroyed at the end of the frame
        void Destroy() {
            m_pendingDestroy = true;

            // Cascade destruction to all children
            for (auto* child : children) {
                if (child) {
                    child->Destroy();
                }
            }
        }

        // Checks if the GameObject is scheduled for destruction
        bool IsPendingDestroy() const {
            return m_pendingDestroy;
        }

        // Checks if this object AND all its parents are active
        bool IsActiveInHierarchy() const {
            if (!isActive) return false;
            if (parent != nullptr) return parent->IsActiveInHierarchy();
            return true;
        }

        // Generates a completely new ID (Useful when instantiating Prefabs!)
        void RegenerateID();

#pragma region Hierarchy Methods
        
        // Assigns a new parent to this GameObject
        void SetParent(GameObject* newParent);

        // Returns the current parent
        GameObject* GetParent() const { return parent; }

        // Returns the list of children
        const std::vector<GameObject*>& GetChildren() const { return children; }

        // Recursively searches for a child GameObject by its exact name
        GameObject* GetChildByName(const std::string& targetName) {
            // Step 1: Check direct children first for optimal performance
            for (GameObject* child : children) {
                if (child != nullptr && child->name == targetName) {
                    return child;
                }
            }

            // Step 2: If not found, recursively search deeper in the hierarchy
            for (GameObject* child : children) {
                if (child != nullptr) {
                    GameObject* found = child->GetChildByName(targetName);
                    if (found != nullptr) {
                        return found;
                    }
                }
            }

            // Return nullptr if the child doesn't exist in this branch
            return nullptr;
        }

        // Internal helper to add a child (usually called by SetParent)
        void AddChild(GameObject* child);

        // Internal helper to remove a child (usually called by SetParent)
        void RemoveChild(GameObject* child);

#pragma endregion

#pragma region Lifecycle and Components
        void Start() {
            for (auto& component : components) {
                component->Start();
            }
        }

        // Iterate through all components and call their update methods
        void Update(float deltaTime) {
            for (auto& component : components) {
                component->Update(deltaTime);
            }
        }

        // Iterate through all components and call their render methods
        void Render() {
            for (auto& component : components) {
                component->Render();
            }
        }

        // Iterate and call HUD render methods
        void RenderHUD() {
            for (auto& component : components) {
                component->RenderHUD();
            }
        }

        // Relays the collision event to all attached components
        void OnCollision(const Collision2D& collision) {
            for (auto& component : components) {
                component->OnCollision(collision);
            }
        }

        void OnAnimationEvent(const std::string& eventName) {
            for (auto& component : components) {
                component->OnAnimationEvent(eventName);
            }
        }

        // Relays the GUI click event to all attached components
        void OnGUIClick(const std::string& buttonName) {
            for (auto& component : components) {
                component->OnGUIClick(buttonName);
            }
        }

        // Safely removes a component from the entity
        void RemoveComponent(Component* compToRemove) {
            if (!compToRemove) return;

            // std::erase_if will find the component with this memory address
            // and remove it from the vector (which automatically calls its destructor)
            std::erase_if(components, [compToRemove](const std::unique_ptr<Component>& comp) {
                return comp.get() == compToRemove;
            });
        }

        // Template method to dynamically add any component type to this GameObject
        // Args&& allows passing constructor arguments directly
        template<typename T, typename... Args>
        T* AddComponent(Args&&... args) {
            // Create the component using modern C++ memory management
            auto newComponent = std::make_unique<T>(std::forward<Args>(args)...);
            
            // Link the component back to this GameObject
            newComponent->owner = this;
            
            // Keep a raw pointer to return to the user
            T* rawPointer = newComponent.get();
            
            // Transfer ownership to the vector
            components.push_back(std::move(newComponent));
            
            // Trigger the start lifecycle method
            rawPointer->Start(); 
            
            return rawPointer;
        }

        // Template method to find and retrieve a component by its type
        template<typename T>
        T* GetComponent() {
            for (auto& component : components) {
                // dynamic_cast checks at runtime if the component is of type T
                if (T* target = dynamic_cast<T*>(component.get())) {
                    return target;
                }
            }
            return nullptr; // Return null if the component is not found
        }

        // Access to components for the Editor
        // Returns a constant reference to the components list
        const std::vector<std::unique_ptr<Component>>& GetAllComponents() const {
            return components;
        }

        template <typename T>
        std::vector<T*> GetComponents() {
            std::vector<T*> result;
            for (auto& comp : components) {
                T* casted = dynamic_cast<T*>(comp.get());
                if (casted) {
                    result.push_back(casted);
                }
            }
            
            return result;
        }

#pragma endregion

        nlohmann::json Serialize() const;

        // Reconstructs the GameObject and all its components from a JSON object
        void Deserialize(const nlohmann::json& j);

    private:
        // Helper to generate random 64-bit integers
        static uint64_t GenerateID();

        // We use unique_ptr so components are automatically destroyed when the GameObject dies
        std::vector<std::unique_ptr<Component>> components;

        // Hierarchy Data
        // We use raw pointers because the Scene still owns the GameObjects via unique_ptr
        GameObject* parent;
        std::vector<GameObject*> children;


        // The death flag
        bool m_pendingDestroy = false;
};
