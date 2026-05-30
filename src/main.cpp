#include "core/Engine.hpp"
#include "core/ProjectSettings.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include "scripting/PythonStubs.hpp"
#include <raylib.h>

#ifdef STANDALONE_MODE
#include "input/InputManager.hpp"
#include "core/GameStateManager.hpp"
#include "core/AssetRegistry.hpp"
#include "scripting/ScriptEngine.hpp"
#endif

int main(int argc, char** argv) {

#ifdef __ANDROID__
    // ====================================================================
    // THE ANDROID CATCH-22 FIX (V3 - Final)
    // Raylib needs the Window initialized to read files on Android.
    // We call InitWindow manually here just to wake up Android's AssetManager
    // BEFORE creating the Engine, so the Engine gets the real resolution later!
    // ====================================================================
    InitWindow(0, 0, "Booting...");

    TraceLog(LOG_INFO, "Waiting for Android rendering surface to be ready...");
    while (!IsWindowReady()) {
        // Keep the app alive and process Android messages until the surface is valid
        BeginDrawing();
        ClearBackground(BLACK);
        EndDrawing();
    }
    TraceLog(LOG_INFO, "Android surface ready! Proceeding with engine boot.");

    // On Android, current_path() returns "/" which triggers severe security denials (SELinux).
    // Raylib configures the internal data path automatically when InitWindow is called.
    std::filesystem::path safeDir = GetWorkingDirectory(); 
    ProjectSettings::SetEngineRoot(safeDir);
#else
    // Save the original working directory (which contains the engine's CMakeLists.txt)
    // before the editor changes it to the project's directory later on.
    ProjectSettings::SetEngineRoot(std::filesystem::current_path());
#endif

    std::filesystem::path projectPath;

    // Determine the project path
    if (argc > 1) {
        // If passed via command line (useful for debugging or shortcuts)
        projectPath = argv[1];
        if (std::filesystem::is_directory(projectPath)) {
            projectPath /= "project.json";
        }
    } else {
#ifndef STANDALONE_MODE
        // In Editor mode, if no arguments are passed, we leave the path empty.
        // The Engine will boot and the Editor will show the Project Hub.
#else
        // In Standalone mode, we mount the VFS immediately using the executable's directory
#ifdef __ANDROID__
        std::filesystem::path exeDir = GetWorkingDirectory();
#else
        std::filesystem::path exeDir = std::filesystem::current_path();
#endif
        
        // On Android, Raylib is already initialized above, so this will succeed!
        AssetRegistry::MountVFS(exeDir);
        
        // We set the target to project.json, which will now be intercepted by the VFS
        projectPath = "project.json";
#endif
    }

    // Attempt to load the project configuration
    if (!projectPath.empty()) {
        if (!ProjectSettings::Load(projectPath)) {
            // Using Raylib's TraceLog ensures this prints to Android Logcat
            TraceLog(LOG_FATAL, "Failed to load project at: %s", projectPath.string().c_str());
#ifdef STANDALONE_MODE
            // In Standalone, failing to load the project configuration is a fatal error
            return -1;
#endif
        }
    } else if (argc > 1) {
        std::cerr << "Error: Provided project path is invalid: " << projectPath << std::endl;
    }

    try {
        // Initialize the Engine 
        // Uses loaded settings, or fallback defaults if in the Editor Hub.
        // On Android, Raylib ignores the redundant internal InitWindow call safely, 
        // BUT the Engine now gets the CORRECT width and height to create its rendering surface!
        Engine engine(
            ProjectSettings::GetWindowWidth(), 
            ProjectSettings::GetWindowHeight(), 
            ProjectSettings::GetProjectName()
        );

#ifdef STANDALONE_MODE
        // Initialize the global Asset Registry
        AssetRegistry::Initialize(ProjectSettings::GetAssetsPath());

        // Register the project's script folder in Python
        ScriptEngine::AddScriptPath(ProjectSettings::GetAssetsPath() / "scripts");

        // Load project specific settings in Standalone BEFORE loading the scene
        std::filesystem::path settingsPath = ProjectSettings::GetAssetsPath() / "settings";
        InputManager::Load((settingsPath / "inputs.json").string());
        GameStateManager::Load((settingsPath / "globals.json").string());
        
        std::cout << "[Standalone] Project settings loaded." << std::endl;

        std::string startScene = ProjectSettings::GetStartScenePath();
        if (!startScene.empty()) {
            engine.LoadScene(startScene);
            engine.GetActiveScene().Start(); 
        } else {
            TraceLog(LOG_FATAL, "WARNING: No Start Scene defined in project.json!");
        }
#endif

        // 5. Start the main game loop
        engine.Run();
        
    } catch (const std::exception& e) {
        TraceLog(LOG_FATAL, "Fatal C++ Exception: %s", e.what());
        
        std::ofstream crashLog("crash.log");
        if (crashLog.is_open()) {
            crashLog << "=== FATAL ERROR ===" << std::endl;
            crashLog << e.what() << std::endl;
            crashLog.close();
        }
        return -1;
    }

    return 0;
}
