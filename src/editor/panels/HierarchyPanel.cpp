#include "HierarchyPanel.hpp"
#include "editor/commands/CommandHistory.hpp"
#include "editor/commands/CreateObjectCommand.hpp"
#include "editor/commands/DeleteObjectCommand.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include "extras/IconsFontAwesome6.h"

// Helper to prevent infinite loops: checks if the target is a descendant of the potential ancestor
bool IsDescendant(GameObject* target, GameObject* potentialAncestor) {
    GameObject* current = target;
    while (current != nullptr) {
        if (current == potentialAncestor) return true;
        current = current->GetParent();
    }
    return false;
}

// Recursive helper to collect a node and all its descendants into a flat list
void CollectHierarchy(GameObject* node, std::vector<GameObject*>& list) {
    if (!node) return;
    list.push_back(node);
    for (auto* child : node->GetChildren()) {
        CollectHierarchy(child, list);
    }
}

void HierarchyPanel::Draw(Scene& activeScene, GameObject*& selectedObject) {
    ImGui::Begin("Hierarchy");

    // Button to create a new root game object on the fly
    if (ImGui::Button("+ Add Game Object", ImVec2(-1, 0))) {
        GameObject* newObj = activeScene.CreateGameObject("New Game Object");
        CommandHistory::AddCommand(std::make_unique<CreateObjectCommand>(activeScene, newObj));
        selectedObject = newObj;
    }

    ImGui::Separator();

    // Deferred deletion pointer
    GameObject* objectToDelete = nullptr;

    // Iterate through ROOT objects only (objects with no parent)
    for (const auto& go : activeScene.GetGameObjects()) {
        if (go->GetParent() == nullptr) {
            DrawNode(activeScene, go.get(), selectedObject, objectToDelete);
        }
    }

    // Empty space drop target and deselection
    ImVec2 availableSpace = ImGui::GetContentRegionAvail();

    if (availableSpace.y > 0.0f) {
        ImGui::InvisibleButton("##HierarchyEmptySpace", availableSpace);

        // Click on empty space to deselect
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            selectedObject = nullptr;
        }

        // Handle Drag & Drop (Dropping in empty space to UNPARENT an object)
        if (ImGui::BeginDragDropTarget()) {
            
            // Unparent an existing hierarchy object
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_GAMEOBJECT")) {
                GameObject* droppedNode = *(GameObject**)payload->Data;
                droppedNode->SetParent(nullptr); // Move to root
            }
            
            // Instantiate a prefab from the Content Browser
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                std::string droppedPath = (const char*)payload->Data;
                std::filesystem::path fsPath(droppedPath);
                
                if (fsPath.extension() == ".prefab") {
                    GameObject* newObj = activeScene.Instantiate(droppedPath);
                    if (newObj) {
                        selectedObject = newObj; 
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }
    }

    // Execute the deletion safely outside the loop
    if (objectToDelete) {
        CommandHistory::AddCommand(std::make_unique<DeleteObjectCommand>(activeScene, objectToDelete));
    }

    ImGui::End();
}

void HierarchyPanel::DrawNode(Scene& activeScene, GameObject* node, GameObject*& selectedObject, GameObject*& objectToDelete) {
    // Render the Eye Toggle Button
    // Push an ID so multiple buttons on the same level don't conflict
    ImGui::PushID(node);

    // Pick the icon based on the state
    const char* icon = node->isActive ? ICON_FA_EYE : ICON_FA_EYE_SLASH;

    // Use a flat button (no background) for a cleaner look
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Text, node->isActive ? ImVec4(0.8f, 0.8f, 0.8f, 1.0f) : ImVec4(0.4f, 0.4f, 0.4f, 1.0f));

    if (ImGui::Button(icon)) {
        node->isActive = !node->isActive;
    }

    ImGui::PopStyleColor(2);
    
    // Draw the button and the tree node on the same line
    ImGui::SameLine();
    
    ImGui::PopID();
    
    ImGuiTreeNodeFlags flags = ((selectedObject == node) ? ImGuiTreeNodeFlags_Selected : 0);
    flags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;

    bool isLeaf = node->GetChildren().empty();
    if (isLeaf) {
        // If it has no children, render it as a flat leaf without an expansion arrow
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    std::string displayName = node->name.empty() ? "<Unnamed>" : node->name;

    // Draw the tree node
    bool isOpen = ImGui::TreeNodeEx((void*)node, flags, "%s", displayName.c_str());

    // Selection
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        selectedObject = node;
    }

    // Drag & Drop SOURCE (Picking up this node)
    if (ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload("HIERARCHY_GAMEOBJECT", &node, sizeof(GameObject*));
        ImGui::Text("Move %s", displayName.c_str());
        ImGui::EndDragDropSource();
    }

    // Drag & Drop TARGET (Dropping ONTO this node to make a child)
    if (ImGui::BeginDragDropTarget()) {
        
        // Handle reparenting an existing GameObject
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_GAMEOBJECT")) {
            GameObject* droppedNode = *(GameObject**)payload->Data;
            
            // Safety: A node cannot be parented to itself or to its own children
            if (droppedNode != node && !IsDescendant(node, droppedNode)) {
                droppedNode->SetParent(node);
            }
        }

        // Handle dropping a prefab onto this node
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
            std::string droppedPath = (const char*)payload->Data;
            std::filesystem::path fsPath(droppedPath);
            
            if (fsPath.extension() == ".prefab") {
                GameObject* newObj = activeScene.Instantiate(droppedPath);
                if (newObj) {
                    newObj->SetParent(node);
                    CommandHistory::AddCommand(std::make_unique<CreateObjectCommand>(activeScene, newObj));
                    selectedObject = newObj;
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    // Context Menu (Right Click)
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::Selectable("Create Empty Child")) {
            GameObject* childObj = activeScene.CreateGameObject("New Child");
            childObj->SetParent(node);
            CommandHistory::AddCommand(std::make_unique<CreateObjectCommand>(activeScene, childObj));
            selectedObject = childObj;
        }

        ImGui::Separator();

        if (ImGui::Selectable("Save as Prefab")) {
            std::filesystem::create_directories("assets/prefabs");
            
            // 1. Collect the parent and all its children
            std::vector<GameObject*> nodesToSave;
            CollectHierarchy(node, nodesToSave);

            // 2. Create a mini-scene structure
            nlohmann::json prefabJson;
            prefabJson["gameObjects"] = nlohmann::json::array();

            // 3. Serialize everyone
            for (GameObject* n : nodesToSave) {
                nlohmann::json nJson = n->Serialize();
                
                // Crucial: The absolute root of the prefab MUST have parentId = 0
                // so it doesn't try to link to its original scene parent when loaded later.
                if (n == node) {
                    nJson["parentId"] = 0;
                }
                
                prefabJson["gameObjects"].push_back(nJson);
            }

            std::string path = "assets/prefabs/" + node->name + ".prefab";
            std::ofstream file(path);

            if (file.is_open()) {
                file << prefabJson.dump(4);
                file.close();
                std::cout << "[Editor] Saved hierarchy prefab successfully: " << path << std::endl;
            } else {
                std::cerr << "[Editor] Error: Could not save prefab to " << path << std::endl;
            }
        }

        if (ImGui::Selectable("Delete Game Object")) {
            objectToDelete = node;
            if (selectedObject == node) selectedObject = nullptr;
        }
        ImGui::EndPopup();
    }

    // Render Children (Recursion)
    if (isOpen && !isLeaf) {
        // We make a temporary copy of the children vector to iterate safely, 
        // preventing iterator invalidation if the list changes during ImGui rendering.
        std::vector<GameObject*> childrenCopy = node->GetChildren();
        
        for (auto* child : childrenCopy) {
            DrawNode(activeScene, child, selectedObject, objectToDelete);
        }
        
        ImGui::TreePop(); // Close the Tree Node
    }
}
