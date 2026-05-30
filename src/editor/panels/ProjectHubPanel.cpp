#include "ProjectHubPanel.hpp"
#include "core/ProjectSettings.hpp"
#include <fstream>
#include <algorithm>
#include <nlohmann/json.hpp>

#ifndef STANDALONE_MODE
#include <imgui.h>
#include <extras/IconsFontAwesome6.h>
#include <portable-file-dialogs.h>
#endif

ProjectHubPanel::ProjectHubPanel() {
    LoadRecentProjects();
}

void ProjectHubPanel::LoadRecentProjects() {
    if (std::filesystem::exists("recent_projects.json")) {
        std::ifstream file("recent_projects.json");
        if (file.is_open()) {
            nlohmann::json j;
            try {
                file >> j;
                if (j.contains("recent_projects")) {
                    m_recentProjects = j["recent_projects"].get<std::vector<std::string>>();
                }
            } catch (...) {
                // Silently fail if the file is corrupted
            }
        }
    }
}

void ProjectHubPanel::SaveRecentProjects() {
    nlohmann::json j;
    j["recent_projects"] = m_recentProjects;
    std::ofstream file("recent_projects.json");
    if (file.is_open()) {
        file << j.dump(4);
    }
}

void ProjectHubPanel::AddRecentProject(const std::string& path) {
    // Check if it's already in the list and remove it (to push it to the top later)
    auto it = std::find(m_recentProjects.begin(), m_recentProjects.end(), path);
    if (it != m_recentProjects.end()) {
        m_recentProjects.erase(it);
    }

    // Insert at the very beginning
    m_recentProjects.insert(m_recentProjects.begin(), path);

    // Keep only the 10 most recent projects
    if (m_recentProjects.size() > 10) {
        m_recentProjects.pop_back();
    }

    SaveRecentProjects();
}

bool ProjectHubPanel::Draw() {
    bool projectLoaded = false;

#ifndef STANDALONE_MODE
    // Async PFD Handlers
    // Check if the folder creation dialog is ready
    if (m_folderDialog && m_folderDialog->ready()) {
        auto dir = m_folderDialog->result();
        if (!dir.empty()) {
            strncpy(m_newProjectPath, dir.c_str(), sizeof(m_newProjectPath) - 1);
        }
        m_folderDialog.reset(); // Destroy the dialog object to free memory
    }

    // Check if the open project dialog is ready
    if (m_fileDialog && m_fileDialog->ready()) {
        auto file = m_fileDialog->result();
        if (!file.empty()) {
            strncpy(m_existingProjectPath, file[0].c_str(), sizeof(m_existingProjectPath) - 1);
        }
        m_fileDialog.reset(); // Destroy the dialog object
    }

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    // Use WorkPos/WorkSize to respect OS taskbars
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    
    // Remove global margins & borders
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | 
                             ImGuiWindowFlags_NoDocking | 
                             ImGuiWindowFlags_NoMove | 
                             ImGuiWindowFlags_NoSavedSettings | 
                             ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::Begin("Project Hub", nullptr, flags);
    ImGui::PopStyleVar(3); 

    // Centered Hub Content
    ImVec2 windowSize = ImGui::GetWindowSize();
    float boxWidth = windowSize.x;
    float boxHeight = windowSize.y;
    ImGui::SetCursorPos(ImVec2((windowSize.x - boxWidth) * 0.5f, (windowSize.y - boxHeight) * 0.35f));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(30.0f, 30.0f));
    ImGui::BeginChild("HubContent", ImVec2(boxWidth, boxHeight), true, ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();
    
    // Header Title
    ImGui::TextUnformatted(ICON_FA_CUBES " Foxvoid Engine - Project Hub");
    ImGui::Separator();
    ImGui::Spacing();

    // Split layout into 2 columns
    if (ImGui::BeginTable("HubLayout", 2, ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("MainControls", ImGuiTableColumnFlags_WidthStretch, 0.65f); // 65% width
        ImGui::TableSetupColumn("RecentProjects", ImGuiTableColumnFlags_WidthStretch, 0.35f); // 35% width
        ImGui::TableNextRow();

        // Column 1: Create and Open
        ImGui::TableSetColumnIndex(0);

        float availWidth = ImGui::GetContentRegionAvail().x - 20.0f; // slight padding
        float browseButtonWidth = 140.0f;
        float inputWithButtonWidth = availWidth - browseButtonWidth - ImGui::GetStyle().ItemSpacing.x;

        // Section 1: Create a new project
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), ICON_FA_PLUS " Create New Project");
        ImGui::Spacing();

        ImGui::TextUnformatted("Project Name");
        ImGui::SetNextItemWidth(availWidth);
        ImGui::InputText("##ProjectName", m_newProjectName, sizeof(m_newProjectName));
        
        ImGui::Spacing();

        ImGui::TextUnformatted("Directory");
        ImGui::SetNextItemWidth(inputWithButtonWidth);
        ImGui::InputText("##Directory", m_newProjectPath, sizeof(m_newProjectPath));
        
        ImGui::SameLine();
        
        if (m_folderDialog) {
            ImGui::BeginDisabled();
            ImGui::Button(ICON_FA_SPINNER " Browsing...", ImVec2(browseButtonWidth, 0));
            ImGui::EndDisabled();
        } else {
            if (ImGui::Button(ICON_FA_FOLDER " Browse...##Create", ImVec2(browseButtonWidth, 0))) {
                m_folderDialog = std::make_shared<pfd::select_folder>("Choose where to save your new project");
            }
        }

        ImGui::Spacing();

        if (ImGui::Button("Create Project", ImVec2(150, 40))) {
            std::string pathStr(m_newProjectPath);
            std::string nameStr(m_newProjectName);
            
            if (!pathStr.empty() && !nameStr.empty()) {
                if (ProjectSettings::CreateNewProject(pathStr, nameStr)) {
                    // Add to recents automatically
                    std::filesystem::path newProjFile = std::filesystem::path(pathStr) / nameStr / "project.json";
                    AddRecentProject(newProjFile.string());
                    
                    projectLoaded = true;
                } else {
                    m_errorMessage = "Failed to create project. Check path and permissions.";
                }
            } else {
                m_errorMessage = "Path and Name cannot be empty.";
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Section 2: Open an existing project
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.8f, 1.0f), ICON_FA_FOLDER_OPEN " Open Existing Project");
        ImGui::Spacing();

        ImGui::TextUnformatted("Project File");
        ImGui::SetNextItemWidth(inputWithButtonWidth);
        ImGui::InputText("##ProjectFile", m_existingProjectPath, sizeof(m_existingProjectPath));
        
        ImGui::SameLine();
        
        if (m_fileDialog) {
            ImGui::BeginDisabled();
            ImGui::Button(ICON_FA_SPINNER " Browsing...", ImVec2(browseButtonWidth, 0));
            ImGui::EndDisabled();
        } else {
            if (ImGui::Button(ICON_FA_FOLDER " Browse...##Open", ImVec2(browseButtonWidth, 0))) {
                std::vector<std::string> filters = {"Foxvoid Project", "project.json"};
                m_fileDialog = std::make_shared<pfd::open_file>("Select a project.json", ".", filters);
            }
        }

        ImGui::Spacing();

        if (ImGui::Button("Open Project", ImVec2(150, 40))) {
            std::string pathStr(m_existingProjectPath);
            if (!pathStr.empty()) {
                std::filesystem::path p(pathStr);
                if (std::filesystem::is_directory(p)) {
                    p /= "project.json";
                }

                if (ProjectSettings::Load(p)) {
                    // Add to recents
                    AddRecentProject(p.string());
                    projectLoaded = true;
                } else {
                    m_errorMessage = "Could not find or load project.json at this location.";
                }
            }
        }

        if (!m_errorMessage.empty()) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f), ICON_FA_TRIANGLE_EXCLAMATION " %s", m_errorMessage.c_str());
        }

        // Column 2: Recent Projects
        ImGui::TableSetColumnIndex(1);
        
        // Indent slightly for visual comfort
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 15.0f);
        ImGui::BeginGroup();

        ImGui::TextColored(ImVec4(0.8f, 0.6f, 0.2f, 1.0f), ICON_FA_CLOCK_ROTATE_LEFT " Recent Projects");
        ImGui::Spacing();

        if (m_recentProjects.empty()) {
            ImGui::TextDisabled("No recent projects found.");
        } else {
            for (size_t i = 0; i < m_recentProjects.size(); ++i) {
                std::filesystem::path p(m_recentProjects[i]);
                
                // Get the parent folder name (e.g. ".../First Platformer/project.json" -> "First Platformer")
                std::string projectName = p.parent_path().filename().string(); 
                if (projectName.empty()) projectName = p.filename().string();

                ImGui::PushID(i);
                
                // Make it a selectable row
                if (ImGui::Selectable(projectName.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)) {
                    if (ProjectSettings::Load(p)) {
                        AddRecentProject(p.string()); // Move it back to the top of the list!
                        projectLoaded = true;
                    } else {
                        m_errorMessage = "Failed to load: " + projectName + "\nIt may have been moved or deleted.";
                    }
                }
                
                // Show the full path on hover
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", p.string().c_str());
                }
                
                ImGui::PopID();
            }
        }
        
        ImGui::EndGroup();
        ImGui::EndTable();
    }

    ImGui::EndChild();
    ImGui::End();
#endif

    return projectLoaded;
}
