#include "InputSettingsPanel.hpp"
#include <raylib.h>
#include "input/InputManager.hpp"
#include <map>
#include <string>
#include <vector>

#ifndef STANDALONE_MODE
#include <imgui.h>
#include "extras/IconsFontAwesome6.h"
#endif

// A simple structure to represent a selectable key in the dropdown menu
struct KeyOption {
    int code;
    const char* name;
};

// The global list of supported inputs in Foxvoid Engine.
// This populates the dropdown when adding a new key bind, bridging Raylib 
// physical keys and our abstract touch/mouse codes.
static const KeyOption SUPPORTED_KEYS[] = {
    // Mouse & Touch Inputs (Abstract codes defined in InputManager.hpp)
    { INPUT_MOUSE_LEFT, "Mouse Left" },
    { INPUT_MOUSE_RIGHT, "Mouse Right" },
    { INPUT_MOUSE_MIDDLE, "Mouse Middle" },
    { INPUT_TOUCH_1, "Touch 1 (Main)" },
    { INPUT_TOUCH_2, "Touch 2 (Secondary)" },
    { INPUT_TOUCH_3, "Touch 3" },

    // Core Gameplay Keys
    { KEY_SPACE, "Space" },
    { KEY_ENTER, "Enter" },
    { KEY_ESCAPE, "Escape" },
    { KEY_UP, "Up Arrow" },
    { KEY_DOWN, "Down Arrow" },
    { KEY_LEFT, "Left Arrow" },
    { KEY_RIGHT, "Right Arrow" },
    { KEY_LEFT_SHIFT, "Left Shift" },
    { KEY_RIGHT_SHIFT, "Right Shift" },
    { KEY_LEFT_CONTROL, "Left Control" },

    // Letters
    { KEY_W, "W" }, { KEY_A, "A" }, { KEY_S, "S" }, { KEY_D, "D" },
    { KEY_Q, "Q" }, { KEY_E, "E" }, { KEY_R, "R" }, { KEY_F, "F" },
    { KEY_Z, "Z" }, { KEY_X, "X" }, { KEY_C, "C" }, { KEY_V, "V" },
    
    // Numbers
    { KEY_ZERO, "0" }, { KEY_ONE, "1" }, { KEY_TWO, "2" }, { KEY_THREE, "3" },
    { KEY_FOUR, "4" }, { KEY_FIVE, "5" }, { KEY_SIX, "6" }, { KEY_SEVEN, "7" },
    { KEY_EIGHT, "8" }, { KEY_NINE, "9" }
};

void InputSettingsPanel::Draw() {
#ifndef STANDALONE_MODE
    // Trigger the opening of the popup modal when requested
    if (m_openRequested) {
        ImGui::OpenPopup("Input Settings");
        m_isOpen = true; // Set internal state
        m_openRequested = false; // Reset trigger
    }

    // Automatically center the modal on the screen when it appears
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    // Set a default size (Width: 800px, Height: 600px)
    // Using ImGuiCond_Appearing ensures it only forces this size the first time it opens
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_Appearing);

    // ImGuiWindowFlags_NoSavedSettings prevents ImGui from writing this window's size to imgui.ini
    if (ImGui::BeginPopupModal("Input Settings", &m_isOpen, ImGuiWindowFlags_NoSavedSettings)) {
        
        // Retrieve the current bindings from the InputManager
        auto& bindings = InputManager::GetBindings();

        // Top bar: Save and Load buttons
        if (ImGui::Button(ICON_FA_FLOPPY_DISK " Save")) {
            InputManager::Save("assets/settings/inputs.json");
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_FOLDER_OPEN " Load")) {
            InputManager::Load("assets/settings/inputs.json");
        }

        ImGui::Separator();
        ImGui::Spacing();

        // Add New Action row
        ImGui::InputText("##NewAction", m_newActionBuffer, IM_ARRAYSIZE(m_newActionBuffer));
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_PLUS " Add Action")) {
            std::string newAction(m_newActionBuffer);
            
            // Only add if the name is not empty and doesn't already exist
            if (!newAction.empty() && bindings.find(newAction) == bindings.end()) {
                // Add an empty list of keys for this new action
                bindings[newAction] = std::vector<int>();
                m_newActionBuffer[0] = '\0'; // Clear the text input buffer

                // Auto-save on structural change to prevent data loss
                InputManager::Save("assets/settings/inputs.json");
            }
        }

        ImGui::Spacing();
        ImGui::Separator();

        // We use a static variable to track the dropdown selection index.
        // It's safe to be static because the UI only allows editing one action at a time.
        static int selectedKeyIndex = 0;

        // Iterate over existing actions
        for (auto it = bindings.begin(); it != bindings.end(); ) {
            const std::string& action = it->first;
            auto& keys = it->second;

            // Calculate dynamic height based on content to fit the dropdown menu
            // Base height (header + add button + padding) is roughly 100 pixels
            // Each bound key adds about 50 pixels of height
            float dynamicHeight = 100.0f + (keys.size() * 50.0f);

            // Draw a slightly darker background for each action block to separate them visually
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
            ImGui::BeginChild(action.c_str(), ImVec2(0, dynamicHeight), true);

            // Action Header (Gamepad Icon + Name + Delete Action Button)
            ImGui::TextDisabled("%s", ICON_FA_GAMEPAD);
            ImGui::SameLine();
            ImGui::TextUnformatted(action.c_str());
            
            // Align the delete button to the far right
            ImGui::SameLine(ImGui::GetWindowWidth() - 40);
            
            // Render a Red trash button
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f)); 
            if (ImGui::Button((std::string(ICON_FA_TRASH) + "##" + action).c_str())) {
                // Erase the action from the map and get the next iterator safely
                it = bindings.erase(it);
                ImGui::PopStyleColor();
                ImGui::EndChild();
                ImGui::PopStyleColor();
                
                // If we delete the action while trying to bind a key to it, cancel the listening state
                if (m_listeningAction == action) m_isListening = false;
                continue; // Skip the rest of the drawing for this deleted item
            }
            ImGui::PopStyleColor();

            // Display already bound keys for this action
            ImGui::Indent();
            for (size_t i = 0; i < keys.size(); ++i) {
                int key = keys[i];
                
                // Use the InputManager's translation function to show readable names
                ImGui::Text("- %s", InputManager::GetKeyName(key).c_str());
                
                // Align the remove button to the far right
                ImGui::SameLine(ImGui::GetWindowWidth() - 40);
                
                // Small 'x' button to unbind a specific key
                if (ImGui::Button((std::string(ICON_FA_XMARK) + "##" + action + std::to_string(key)).c_str())) {
                    keys.erase(keys.begin() + i);
                    break; // Break the inner loop to avoid iterator invalidation; it will refresh next frame
                }
            }
            ImGui::Unindent();

            ImGui::Spacing();

            // --- The Add Key Mechanism (Dropdown UI) ---
            if (m_isListening && m_listeningAction == action) {
                
                ImGui::SetNextItemWidth(180.0f);
                
                // Display the ImGui Combo Box (Dropdown menu)
                if (ImGui::BeginCombo(("##KeyCombo" + action).c_str(), SUPPORTED_KEYS[selectedKeyIndex].name)) {
                    for (int n = 0; n < IM_ARRAYSIZE(SUPPORTED_KEYS); n++) {
                        const bool is_selected = (selectedKeyIndex == n);
                        if (ImGui::Selectable(SUPPORTED_KEYS[n].name, is_selected)) {
                            selectedKeyIndex = n;
                        }

                        // Set the initial focus when opening the combo
                        if (is_selected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }

                ImGui::SameLine();
                
                // Confirm the selection and bind the key
                if (ImGui::Button(("Bind##" + action).c_str())) {
                    InputManager::BindKey(action, SUPPORTED_KEYS[selectedKeyIndex].code);
                    m_isListening = false;
                    selectedKeyIndex = 0; // Reset index for the next binding session
                }
                
                ImGui::SameLine();
                
                // Cancel the binding process
                if (ImGui::Button(("Cancel##" + action).c_str())) {
                    m_isListening = false;
                }

            } else {
                // Show the "Add Key" button when not in listening mode
                if (ImGui::Button((std::string(ICON_FA_PLUS) + " Add Key##" + action).c_str())) {
                    m_isListening = true;
                    m_listeningAction = action;
                    selectedKeyIndex = 0; // Reset dropdown to the top option
                }
            }

            ImGui::EndChild();
            ImGui::PopStyleColor();

            ++it; // Move to the next action
        }

        ImGui::EndPopup();
    }
#endif
}
