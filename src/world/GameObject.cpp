#include "GameObject.hpp"
#include "ComponentRegistry.hpp"
#include <iostream>
#include <random>

// Use a static random engine for fast and robust 64-bit ID generation
uint64_t GameObject::GenerateID() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());

    // Start from 1, because 0 is conventionally used to mean "no parent" or "invalid ID"
    static std::uniform_int_distribution<uint64_t> dis(1);
    return dis(gen);
}

GameObject::GameObject(const std::string& name) : name(name), id(GenerateID()), parent(nullptr) {}

GameObject::~GameObject() {
    // If we have a parent, tell it to remove us from its children list.
    // This prevents the parent from keeping a dangling pointer to this destroyed object.
    if (parent != nullptr) {
        parent->RemoveChild(this);
    }

    // Unlink all children. 
    // We modify their internal 'parent' pointer directly instead of calling 
    // child->SetParent(nullptr) to avoid modifying our own 'children' vector 
    // while we are currently iterating over it.
    for (auto* child : children) {
        if (child != nullptr) {
            child->parent = nullptr; 
        }
    }
}

void GameObject::RegenerateID() {
    id = GenerateID();
}

void GameObject::SetParent(GameObject* newParent) {
    // Prevent settings self as parent (circular reference)
    if (newParent == this) return;

    // If we already have a parent, remove ourselves from its child list
    if (parent != nullptr) {
        parent->RemoveChild(this);
    }

    // Set the new parent
    parent = newParent;

    // Add ourselves to the new parent's child list
    if (parent != nullptr) {
        parent->AddChild(this);
    }
}

void GameObject::AddChild(GameObject* child) {
    // Safety check: ensure the child isn't null and isn't already in the list
    if (child != nullptr) {
        auto it = std::find(children.begin(), children.end(), child);
        if (it == children.end()) {
            children.push_back(child);
        }
    }
}

void GameObject::RemoveChild(GameObject* child) {
    // std::erase removes all elements equal to 'child' (C++20 feature)
    if (child != nullptr) {
        std::erase(children, child);
    }
}

nlohmann::json GameObject::Serialize() const {
    nlohmann::json j;

    // Save the identity and relationship data
    j["id"] = id;
    j["parentId"] = parent ? parent->id : 0; // 0 means no parent

    j["isActive"] = isActive;

    j["name"] = name;
    j["tag"] = tag;
    j["components"] = nlohmann::json::array();

    for (const auto& comp : components) {
        j["components"].push_back(comp->Serialize());
    }

    return j;
}

void GameObject::Deserialize(const nlohmann::json& j) {
    // Restore the ID (or generate a new one if it's somehow missing)
    id = j.value("id", GenerateID());

    // Store the parent ID so the Scene can link them later
    pendingParentId = j.value("parentId", 0ULL);

    // Update the name if it exists in the JSON data
    if (j.contains("name")) {
        name = j["name"];
    }

    // Update the tag if it exists in the JSON data
    if (j.contains("tag")) {
        tag = j["tag"];
    }

    isActive = j.value("isActive", true);

    // Clear any default components that might have been added during creation
    // (like a default Transform2d) to avoid duplicates when loading from the prefab
    components.clear();

    // Iterate through the saved components array
    if (j.contains("components") && j["components"].is_array()) {
        for (const auto& compJson : j["components"]) {
            // Extract the exact type string (e.g., "SpriteRenderer")
            std::string type = compJson.value("type", "");
                    
            // Check if we have a factory registered for this type
            if (!type.empty() && ComponentRegistry::factories.count(type)) {
                        
                // Use the factory to allocate and attach the new component to this object
                ComponentRegistry::factories[type](*this);
                        
                // The factory automatically pushes the new component to the back of our vector.
                // We grab it and ask it to load its specific variables (textures, paths, etc.)
                if (!components.empty()) {
                    components.back()->Deserialize(compJson);
                }
            } else {
                std::cerr << "[GameObject] Warning: Could not deserialize unknown component type: " << type << std::endl;
            }
        }
    }
}
