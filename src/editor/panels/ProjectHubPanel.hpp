#pragma once

#include <string>
#include <memory>
#include <vector>

// Forward declarations to avoid including portable-file-dialogs.h here
namespace pfd {
    class select_folder;
    class open_file;
}

class ProjectHubPanel {
    public:
        ProjectHubPanel();

        // Renders the Hub. Returns true if a project was successfully loaded or created.
        bool Draw();

    private:
        // Persistence logic for recent projects
        void LoadRecentProjects();
        void SaveRecentProjects();
        void AddRecentProject(const std::string& path);

        // Buffers for ImGui text inputs
        char m_newProjectPath[256] = "";
        char m_newProjectName[64] = "MyNewGame";
        
        char m_existingProjectPath[256] = "";
        
        // Holds error messages to display in the UI
        std::string m_errorMessage = "";

        // Asynchronous dialog pointers
        std::shared_ptr<pfd::select_folder> m_folderDialog;
        std::shared_ptr<pfd::open_file> m_fileDialog;

        // Stores the paths of recently opened projects
        std::vector<std::string> m_recentProjects;
};
