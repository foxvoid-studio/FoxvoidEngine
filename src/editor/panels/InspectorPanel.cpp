#include "InspectorPanel.hpp"
#include "editor/commands/CommandHistory.hpp"
#include "editor/commands/ChangeNameCommand.hpp"
#include "scripting/ScriptableObject.hpp"
#include "scripting/DataManager.hpp"
#include "editor/commands/ChangeTagCommand.hpp"

void InspectorPanel::Draw(GameObject*& selectedObject, py::object& selectedAsset, std::string& selectedAssetPath) {
    ImGui::Begin("Inspector");

    // ==========================================
    // MODE 1: SCRIPTABLE OBJECT (ASSET) INSPECTION
    // ==========================================
    if (!selectedAsset.is_none()) {
        try {
            // Cast the Python object to our C++ base class to access its methods
            ScriptableObject* nativeObj = selectedAsset.cast<ScriptableObject*>();
            if (nativeObj) {
                ImGui::TextDisabled("Asset Path: %s", selectedAssetPath.c_str());
                ImGui::Separator();
                ImGui::Spacing();

                // Call the introspection logic we built earlier
                nativeObj->OnInspector();

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                // Centered Save Button
                float windowWidth = ImGui::GetWindowSize().x;
                float buttonWidth = 150.0f;
                ImGui::SetCursorPosX((windowWidth - buttonWidth) * 0.5f);
                
                // When clicked, write the JSON back to the disk
                if (ImGui::Button("Save Asset", ImVec2(buttonWidth, 0))) {
                    DataManager::SaveAsset(selectedAsset, selectedAssetPath);
                }
            }
        } catch (const py::error_already_set& e) {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Asset Cast Error:");
            ImGui::TextWrapped("%s", e.what());
        }
    }
    // ==========================================
    // MODE 2: GAMEOBJECT INSPECTION
    // ==========================================
    else if (selectedObject != nullptr) {
        // Rename the entity
        char nameBuffer[256];
        strncpy(nameBuffer, selectedObject->name.c_str(), sizeof(nameBuffer));
        nameBuffer[sizeof(nameBuffer) - 1] = '\0'; 
            
        // Draw the text input box
        ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer));

        static std::string initialName;

        // The editing lifecycle
        if (ImGui::IsItemActivated()) {
            initialName = selectedObject->name;
        }

        if (ImGui::IsItemActive()) {
            selectedObject->name = std::string(nameBuffer); 
        }

        if (ImGui::IsItemDeactivatedAfterEdit()) {
            std::string finalName = std::string(nameBuffer);
            
            if (initialName != finalName) {
                selectedObject->name = initialName;
                CommandHistory::AddCommand(std::make_unique<ChangeNameCommand>(selectedObject, initialName, finalName));
            }
        } 
        else if (ImGui::IsItemDeactivated() && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            selectedObject->name = initialName;
        }

        // ==========================================
        // EDIT TAG
        // ==========================================
        char tagBuffer[256];
        strncpy(tagBuffer, selectedObject->tag.c_str(), sizeof(tagBuffer));
        tagBuffer[sizeof(tagBuffer) - 1] = '\0'; 
            
        // Draw the text input box for the tag
        ImGui::InputText("Tag", tagBuffer, sizeof(tagBuffer));

        static std::string initialTag;

        // The editing lifecycle for the tag
        if (ImGui::IsItemActivated()) {
            initialTag = selectedObject->tag;
        }

        if (ImGui::IsItemActive()) {
            selectedObject->tag = std::string(tagBuffer); 
        }

        if (ImGui::IsItemDeactivatedAfterEdit()) {
            std::string finalTag = std::string(tagBuffer);
            
            if (initialTag != finalTag) {
                // Keep the new tag
                selectedObject->tag = finalTag;
                
                CommandHistory::AddCommand(std::make_unique<ChangeTagCommand>(selectedObject, initialTag, finalTag));
            }
        } 
        else if (ImGui::IsItemDeactivated() && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            // Revert changes if the user presses Escape while editing
            selectedObject->tag = initialTag;
        }

        ImGui::Separator();

        // Display existing components
        Component* componentToRemove = nullptr;

        // Fetch a reference to the vector
        const auto& components = selectedObject->GetAllComponents();
        
        // Loop by index! This survives vector reallocations triggered by OnInspector()
        for (size_t i = 0; i < components.size(); ++i) {
            // Re-fetch the pointer dynamically at every iteration
            Component* comp = components[i].get();
            
            // Safety check in case something went terribly wrong
            if (!comp) continue;

            ImGui::PushID(comp);
                
            bool isOpen = ImGui::CollapsingHeader(comp->GetName().c_str(), ImGuiTreeNodeFlags_DefaultOpen);

            if (ImGui::BeginPopupContextItem()) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                if (ImGui::Selectable("Remove Component")) {
                    componentToRemove = comp;
                }
                ImGui::PopStyleColor();
                ImGui::EndPopup();
            }

            if (isOpen) {
                comp->OnInspector();
                
                // CRITICAL SAFETY CHECK:
                // If OnInspector caused an addition (e.g. via __require_components__),
                // the components vector size changed. If we don't break, our index
                // will keep going and draw the newly added component immediately, 
                // which might not be fully initialized or might mess up ImGui state.
                // Breaking here ends the loop, and the new component will be drawn safely on the next frame.
                if (components.size() > selectedObject->GetAllComponents().size()) {
                    ImGui::PopID(); // Clean up ImGui state before breaking
                    break;
                }
            }
                
            ImGui::PopID();
        }

        if (componentToRemove != nullptr) {
            selectedObject->RemoveComponent(componentToRemove);
        }

        ImGui::Separator();
        ImGui::Spacing();

        // Add new components
        float windowWidth = ImGui::GetWindowSize().x;
        float buttonWidth = 200.0f;
        ImGui::SetCursorPosX((windowWidth - buttonWidth) * 0.5f);

        if (ImGui::Button("Add Component", ImVec2(buttonWidth, 0))) {
            ImGui::OpenPopup("AddComponentPopup");
        }

        if (ImGui::BeginPopup("AddComponentPopup")) {
            // Static buffer to hold the search query
            static char searchBuffer[128] = "";

            // Auto-focus the search bar the moment the popup opens
            if (ImGui::IsWindowAppearing()) {
                ImGui::SetKeyboardFocusHere();
            }

            // Draw the search bar
            ImGui::InputText("Search...", searchBuffer, sizeof(searchBuffer));
            ImGui::Separator();

            std::string searchQuery(searchBuffer);

            // Convert the search query to lowercase for case-insensitive matching
            std::transform(searchQuery.begin(), searchQuery.end(), searchQuery.begin(), ::tolower);

            // Mode A: Empty search: Show categories
            if (searchQuery.empty()) {
                // Iterate through the sorted map of categories
                for (const auto& [category, types] : ComponentRegistry::categorizedTypes) {
                    // Create a sub-menu for each category
                    if (ImGui::BeginMenu(category.c_str())) {
                        // List all components within this category
                        for (const std::string& typeName : types) {
                            if (ImGui::Selectable(typeName.c_str())) {
                                ComponentRegistry::factories[typeName](*selectedObject);
                            }
                        }
                        ImGui::EndMenu();
                    }
                }
            }
            // Mode B: Active search: Show filtered list
            else {
                bool foundAny = false;

                // Iterate through the flat list of all registered types
                for (const std::string& typeName : ComponentRegistry::GetFlatRegisteredTypes()) {
                    std::string typeLower = typeName;
                    std::transform(typeLower.begin(), typeLower.end(), typeLower.begin(), ::tolower);
                    
                    // If the component name contains the search string
                    if (typeLower.find(searchQuery) != std::string::npos) {
                        foundAny = true;
                        
                        if (ImGui::Selectable(typeName.c_str())) {
                            ComponentRegistry::factories[typeName](*selectedObject);
                            
                            // Reset search and close popup after adding
                            searchBuffer[0] = '\0';
                            ImGui::CloseCurrentPopup();
                        }
                    }
                }

                if (!foundAny) {
                    ImGui::TextDisabled("No results found.");
                }
            }

            ImGui::EndPopup();
        }
    }
    // ==========================================
    // MODE 3: NOTHING SELECTED
    // ==========================================
    else {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Select an object or an asset.");
    }

    ImGui::End();
}
