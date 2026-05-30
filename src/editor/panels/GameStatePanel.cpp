#include "GameStatePanel.hpp"
#include "core/GameStateManager.hpp"
#include <string>

#ifndef STANDALONE_MODE
#include <imgui.h>
#include <extras/IconsFontAwesome6.h>
#endif 

void GameStatePanel::Draw() {
    // Trigger the opening of the popup
    if (m_openRequested) {
        ImGui::OpenPopup("Global Variables");
        m_isOpen = true; // Set internal state
        m_openRequested = false; // Reset trigger
    }

    // Automatically center the modal on the screen when it appears
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    // Set a default size (Width: 800px, Height: 600px)
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_Appearing);

    // Render the modal
    if (ImGui::BeginPopupModal("Global Variables", &m_isOpen, ImGuiWindowFlags_NoSavedSettings)) {
        // Add new variable form
        static int varType = 0; // 0=Int, 1=Float, 2=Bool, 3=String
        static char nameBuffer[64] = "";
        
        ImGui::TextDisabled("Add a new variable to access it from Python via Globals.get_int('name')");
        ImGui::Spacing();
        
        ImGui::SetNextItemWidth(100);
        ImGui::Combo("##type", &varType, "Int\0Float\0Bool\0String\0");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(250);
        ImGui::InputTextWithHint("##name", "Variable Name (e.g., score)", nameBuffer, IM_ARRAYSIZE(nameBuffer));
        ImGui::SameLine();
        
        if (ImGui::Button(ICON_FA_PLUS " Add Variable")) {
            std::string key(nameBuffer);
            if (!key.empty()) {
                if (varType == 0 && !GameStateManager::Ints.contains(key)) GameStateManager::Ints[key] = 0;
                if (varType == 1 && !GameStateManager::Floats.contains(key)) GameStateManager::Floats[key] = 0.0f;
                if (varType == 2 && !GameStateManager::Bools.contains(key)) GameStateManager::Bools[key] = false;
                if (varType == 3 && !GameStateManager::Strings.contains(key)) GameStateManager::Strings[key] = "";
                
                GameStateManager::Save();
                nameBuffer[0] = '\0'; // Clear input
            }
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // List existing variables
        bool needsSave = false;

        // Integers
        if (!GameStateManager::Ints.empty()) {
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Integers");
            for (auto it = GameStateManager::Ints.begin(); it != GameStateManager::Ints.end(); ) {
                ImGui::PushID(it->first.c_str());
                ImGui::SetNextItemWidth(150);
                if (ImGui::DragInt(it->first.c_str(), &it->second)) needsSave = true;
                
                ImGui::SameLine(ImGui::GetWindowWidth() - 50);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                if (ImGui::Button(ICON_FA_TRASH)) {
                    it = GameStateManager::Ints.erase(it);
                    needsSave = true;
                } else { ++it; }
                ImGui::PopStyleColor();
                ImGui::PopID();
            }
            ImGui::Spacing();
        }

        // Floats
        if (!GameStateManager::Floats.empty()) {
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.8f, 1.0f), "Floats");
            for (auto it = GameStateManager::Floats.begin(); it != GameStateManager::Floats.end(); ) {
                ImGui::PushID(it->first.c_str());
                ImGui::SetNextItemWidth(150);
                if (ImGui::DragFloat(it->first.c_str(), &it->second, 0.1f)) needsSave = true;
                
                ImGui::SameLine(ImGui::GetWindowWidth() - 50);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                if (ImGui::Button(ICON_FA_TRASH)) {
                    it = GameStateManager::Floats.erase(it);
                    needsSave = true;
                } else { ++it; }
                ImGui::PopStyleColor();
                ImGui::PopID();
            }
            ImGui::Spacing();
        }

        // Bools
        if (!GameStateManager::Bools.empty()) {
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.4f, 1.0f), "Booleans");
            for (auto it = GameStateManager::Bools.begin(); it != GameStateManager::Bools.end(); ) {
                ImGui::PushID(it->first.c_str());
                if (ImGui::Checkbox(it->first.c_str(), &it->second)) needsSave = true;
                
                ImGui::SameLine(ImGui::GetWindowWidth() - 50);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                if (ImGui::Button(ICON_FA_TRASH)) {
                    it = GameStateManager::Bools.erase(it);
                    needsSave = true;
                } else { ++it; }
                ImGui::PopStyleColor();
                ImGui::PopID();
            }
            ImGui::Spacing();
        }

        // Strings
        if (!GameStateManager::Strings.empty()) {
            ImGui::TextColored(ImVec4(0.8f, 0.4f, 0.8f, 1.0f), "Strings");
            for (auto it = GameStateManager::Strings.begin(); it != GameStateManager::Strings.end(); ) {
                ImGui::PushID(it->first.c_str());
                char buf[256];
                strncpy(buf, it->second.c_str(), sizeof(buf));
                
                ImGui::SetNextItemWidth(250);
                if (ImGui::InputText(it->first.c_str(), buf, sizeof(buf))) {
                    it->second = buf;
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) needsSave = true;

                ImGui::SameLine(ImGui::GetWindowWidth() - 50);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                if (ImGui::Button(ICON_FA_TRASH)) {
                    it = GameStateManager::Strings.erase(it);
                    needsSave = true;
                } else { ++it; }
                ImGui::PopStyleColor();
                ImGui::PopID();
            }
        }

        // Save to JSON only when user stops dragging/editing to avoid file I/O spam
        if (needsSave && ImGui::IsMouseReleased(0)) {
            GameStateManager::Save();
        }

        ImGui::EndPopup();
    }
}
