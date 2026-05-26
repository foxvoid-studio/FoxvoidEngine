#include "MainMenuBar.hpp"
#include <imgui.h>
#include <iostream>
#include <filesystem>
#include <cctype>
#include "commands/CommandHistory.hpp"
#include "core/ProjectSettings.hpp"
#include <portable-file-dialogs.h>
#include <extras/IconsFontAwesome6.h>
#include <core/AssetRegistry.hpp>
#include "build/Build.hpp"
#include "build/IBuilder.hpp"

void MainMenuBar::Draw(Scene& activeScene, std::string& currentScenePath, bool& isRunning, GameObject*& selectedObject, InputSettingsPanel& inputPanel, GameStatePanel& gameStatePanel, bool& showGrid2D, bool& showGrid3D) {
    // Global Shortcuts
    ImGuiIO& io = ImGui::GetIO();

    // Check for Ctrl + S (and make sure we aren't currently typing in a text input box!)
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S) && !io.WantTextInput) {
        if (currentScenePath.empty()) {
            m_openSavePopup = true; // Open the Save As window
        } else {
            activeScene.SaveToFile(currentScenePath);
            std::cout << "[Editor] Scene saved via shortcut to " << currentScenePath << std::endl;
        }
    }

    // Check for Ctrl + Z (QWERTY) or Ctrl + W (AZERTY) for Undo
    if (io.KeyCtrl && (ImGui::IsKeyPressed(ImGuiKey_Z) || ImGui::IsKeyPressed(ImGuiKey_W)) && !io.WantTextInput) {
        CommandHistory::Undo();
    }

    // Check for Ctrl + Y (Redo)
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y) && !io.WantTextInput) {
        CommandHistory::Redo();
    }
    
    // Draw the top menu bar
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            // New scene
            if (ImGui::MenuItem("New Scene")) {
                activeScene.Clear();
                currentScenePath = ""; // Reset the path
                selectedObject = nullptr; // Deselect anything
            }

            // Save scene
            if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
                if (currentScenePath.empty()) {
                    // If it's a new scene, open the "Save As" popup
                    m_openSavePopup = true;
                }
                else {
                    // Otherwise, just overwrite the current file
                    activeScene.SaveToFile(currentScenePath);
                    std::cout << "[Editor] Scene saved to " << currentScenePath << std::endl;
                }
            }

            // Save scene as
            if (ImGui::MenuItem("Save Scene as...")) {
                m_openSavePopup = true;
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            // Undo
            ImGui::BeginDisabled(!CommandHistory::CanUndo());
            if (ImGui::MenuItem("Undo", "Ctrl+Z")) {
                CommandHistory::Undo();
            }
            ImGui::EndDisabled();

            // Redo
            ImGui::BeginDisabled(!CommandHistory::CanRedo());
            if (ImGui::MenuItem("Redo", "Ctrl+Y")) {
                CommandHistory::Redo();
            }
            ImGui::EndDisabled();

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Show 2D Grid", nullptr, &showGrid2D);
            ImGui::MenuItem("Show 3D Grid", nullptr, &showGrid3D);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Settings")) {
            if (ImGui::MenuItem("Input Settings")) {
                inputPanel.Open();
            }

            if (ImGui::MenuItem("Game State")) {
                gameStatePanel.Open();
            }
            
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Build")) {
            if (ImGui::MenuItem("Build Project...")) {
                m_openBuildPopup = true;
            }

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    // Trigger the popup
    if (m_openSavePopup) {
        ImGui::OpenPopup("Save Scene As...");
        m_openSavePopup = false; // Reset flag immediately so it doesn't try to open every frame
    }

    // Draw the Save As modal window
    if (ImGui::BeginPopupModal("Save Scene As...", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        static char nameBuffer[128] = "NewScene";
        ImGui::InputText("Scene Name", nameBuffer, IM_ARRAYSIZE(nameBuffer));

        if (ImGui::Button("Save", ImVec2(120, 0))) {
            std::filesystem::create_directories("assets/scenes");
            
            // Construct the path and force the .scene extension
            currentScenePath = std::string("assets/scenes/") + nameBuffer + ".scene";
            activeScene.SaveToFile(currentScenePath);
            
            std::cout << "[Editor] Scene saved as " << currentScenePath << std::endl;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Trigger the build popup
    if (m_openBuildPopup) {
        ImGui::OpenPopup("Build Project");
        m_openBuildPopup = false;
    }

    // Draw the Build Project modal window
    if (ImGui::BeginPopupModal("Build Project", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Configure Standalone Build");
        ImGui::Separator();
        ImGui::Spacing();

        // Select Start Scene
        static char startSceneBuffer[256] = "";
        
        // Pre-fill the buffer with the existing setting if it's currently empty
        if (strlen(startSceneBuffer) == 0) {
            strncpy(startSceneBuffer, ProjectSettings::GetStartScenePath().c_str(), sizeof(startSceneBuffer) - 1);
        }

        ImGui::TextUnformatted("Start Scene (.scene)");
        ImGui::SetNextItemWidth(300.0f);
        ImGui::InputText("##StartScene", startSceneBuffer, sizeof(startSceneBuffer));
        
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_FOLDER " Browse##Scene")) {
            // Open native file dialog starting in the assets folder
            auto file = pfd::open_file("Select starting scene", ProjectSettings::GetAssetsPath().string(), {"Scene Files", "*.scene"}).result();
            if (!file.empty()) {
                // Convert the absolute path to a relative path from the project root
                std::filesystem::path fullPath(file[0]);
                std::filesystem::path relativePath = std::filesystem::relative(fullPath, ProjectSettings::GetProjectRoot());
                strncpy(startSceneBuffer, relativePath.string().c_str(), sizeof(startSceneBuffer) - 1);
            }
        }

        ImGui::Spacing();

        // Select Output Directory
        static char outputDirBuffer[256] = "build_standalone";
        ImGui::TextUnformatted("Output Directory Name");
        ImGui::SetNextItemWidth(300.0f);
        ImGui::InputText("##OutputDir", outputDirBuffer, sizeof(outputDirBuffer));

        ImGui::Spacing();

        // Select Target OS
        static int targetOsIndex = 0;
        const char* osOptions[] = { "Linux", "Windows", "Android", "Web" };
        ImGui::TextUnformatted("Target Platform");
        ImGui::SetNextItemWidth(300.0f);
        ImGui::Combo("##TargetOS", &targetOsIndex, osOptions, IM_ARRAYSIZE(osOptions));

        ImGui::Spacing();

        // Android-specific settings section
        static int orientationIndex = 0; // 0 = Landscape, 1 = Portrait
        if (targetOsIndex == 2) { // 2 == Android
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "Android Options");
            
            const char* orientationOptions[] = { "Landscape (Horizontal)", "Portrait (Vertical)" };
            ImGui::TextUnformatted("Screen Orientation");
            ImGui::SetNextItemWidth(300.0f);
            ImGui::Combo("##Orientation", &orientationIndex, orientationOptions, IM_ARRAYSIZE(orientationOptions));
            ImGui::Spacing();
        }

        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Build", ImVec2(120, 0))) {
            std::string startSceneStr(startSceneBuffer);
            std::string outputDirStr(outputDirBuffer);
            
            // Map the dropdown index to our Enum
            TargetOS targetPlatform = TargetOS::Linux;
            if (targetOsIndex == 1) targetPlatform = TargetOS::Windows;
            if (targetOsIndex == 2) targetPlatform = TargetOS::Android;
            if (targetOsIndex == 3) targetPlatform = TargetOS::Web;
            
            // Map the orientation index to the enum
            ScreenOrientation orientation = (orientationIndex == 0) ? ScreenOrientation::Landscape : ScreenOrientation::Portrait;

            // Save the chosen start scene into the project configuration
            ProjectSettings::SetStartScenePath(startSceneStr);
            
            if (ProjectSettings::Save()) {
                std::cout << "[Editor] Triggering CMake build process..." << std::endl;
                
                std::filesystem::path projectRoot = ProjectSettings::GetProjectRoot();
                std::filesystem::path buildDir = projectRoot / outputDirStr;
                std::filesystem::create_directories(buildDir);
                std::string engineRoot = ProjectSettings::GetEngineRoot().string();

                m_openBuildProgressPopup = true; // Trigger the new progress modal

                Build::Start(startSceneStr, outputDirStr, projectRoot, engineRoot, targetPlatform, orientation);
            } 
            else {
                std::cerr << "[Editor] Failed to save build configuration." << std::endl;
            }
            
            // Close the modal whether the build succeeded or failed
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }

    if (m_openBuildProgressPopup) {
        ImGui::OpenPopup("Build Progress");
        m_openBuildProgressPopup = false;
    }

    // Modal size is fixed to make sure the console has enough room
    ImGui::SetNextWindowSize(ImVec2(700, 450), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Build Progress", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        
        // Thread-safe data retrieval from the static Build class
        // Fetch the current status message (e.g., "Step 1/3: Compiling...")
        std::string currentStatus = Build::GetStatusMessage();
        // Fetch the current progress percentage (0-100, or -1 for errors)
        int progress = Build::GetProgress();
        // Check if the background build thread is currently running
        bool isBuilding = Build::IsBuilding();
        // Fetch a thread-safe copy of all console logs generated so far
        std::vector<std::string> logs = Build::GetLogs();
        
        // Display the current status message at the top of the modal
        ImGui::TextUnformatted(currentStatus.c_str());
        ImGui::Spacing();

        if (ImGui::Button(ICON_FA_COPY " Copy Logs to Clipboard")) {
            std::string allLogs = "--- BUILD LOGS ---\nStatus: " + currentStatus + "\n\n";
            for (const auto& log : logs) {
                allLogs += log + "\n";
            }
            ImGui::SetClipboardText(allLogs.c_str());
        }
        ImGui::Spacing();

        // Setup a dark background color specifically for the console output area
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.05f, 0.05f, 0.05f, 1.0f)); 
        // Create a scrollable child window for the console logs with a fixed height (280px)
        ImGui::BeginChild("ConsoleLog", ImVec2(0, 280), true, ImGuiWindowFlags_HorizontalScrollbar);

        // Iterate through all fetched logs to display them
        for (const auto& log : logs) {
            // Create a lowercase copy of the log to easily search for keywords
            std::string lowerLog = log;
            std::transform(lowerLog.begin(), lowerLog.end(), lowerLog.begin(), ::tolower);

            // Colorize the text based on keywords (Errors in red, Warnings in yellow, Normal in default color)
            if (lowerLog.find("error") != std::string::npos || lowerLog.find("failed") != std::string::npos) {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", log.c_str());
            } else if (lowerLog.find("warning") != std::string::npos) {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%s", log.c_str());
            } else {
                ImGui::TextUnformatted(log.c_str());
            }
        }

        // Auto-scroll to the bottom: If we are building and the scrollbar is already near the bottom,
        // force it to stay at the bottom so the user sees the newest logs automatically.
        if (isBuilding && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(); // Restore the default child background color
        ImGui::Spacing();

        // Handle the progress bar or error message display
        if (progress >= 0) {
            // Convert percentage (0-100) to a fraction (0.0 - 1.0) for ImGui
            float fraction = progress / 100.0f;
            // Draw the progress bar. ImVec2(-1, 24) means "fill available width, 24px height"
            ImGui::ProgressBar(fraction, ImVec2(-1, 24)); 
        } else {
            // If progress is negative (-1), it means a fatal error occurred in the build thread
            ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), ICON_FA_TRIANGLE_EXCLAMATION " Build Process Terminated with Errors.");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Footer Actions: Disable the "Close" button while the build is actively running
        if (isBuilding) {
            ImGui::BeginDisabled();
            ImGui::Button(ICON_FA_SPINNER " Building...", ImVec2(120, 0));
            ImGui::EndDisabled();
        } else {
            // Once the build finishes (success or failure), allow the user to close the modal
            if (ImGui::Button("Close", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
        }
        
        ImGui::EndPopup();
    }
}
