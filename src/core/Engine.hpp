#pragma once

#include <string>
#include <filesystem>
#include <memory>
#include <raylib.h>
#include "scene/Scene.hpp"

#ifndef STANDALONE_MODE
class Editor;
#endif

// The Engine class encapsulates the core loop and window management.
class Engine {
    public:
        // Constructor initializes the Raylib window and core systems
        Engine(int width, int height, const std::string& title);
        
        // Destructor ensures proper cleanup of Raylib resources
        ~Engine();

        // Deleted copy constructor and assignment operator to prevent 
        // duplicating the engine instance (Singleton-like behavior)
        Engine(const Engine&) = delete;
        Engine& operator=(const Engine&) = delete;

        // Starts the main game loop
        void Run();

        // Executes exactly one frame of the game.
        // Extracted for WebAssembly compatibility.
        void MainLoopStep();

        // Returns the active instance of the Engine
        static Engine* Get() { return s_instance; }
        
        // Returns a reference to the active scene to allow GameObject instantiation
        Scene& GetActiveScene() { return m_activeScene; }

        // Getters for internal game resolution
        int GetTargetWidth() const { return m_windowWidth; }
        int GetTargetHeight() const { return m_windowHeight; }

        // Allows updating the engine's internal resolution at runtime
        void UpdateResolution(int width, int height);

        // Requests a scene change for the start of the next frame
        void LoadScene(const std::string& scenePath);

        void SetPlaying(bool playing);
        bool IsPlaying() const;

        // Allows the editor to query the currently loaded scene file
        const std::string& GetCurrentScenePath() const { return m_currentScenePath; }

        // Allows the editor to forcefully restore the scene path after a play session
        void SetCurrentScenePath(const std::string& path) { m_currentScenePath = path; }

    private:
        // Core loop stages
        void ProcessInput();
        void Update(float deltaTime);
        void Render();

        // Engine state
        bool m_isRunning;

        // Play / Edit mode state
        // True when  the game is running, False when in Editor mode
        bool m_isPlaying;

        int m_windowWidth;
        int m_windowHeight;
        std::string m_windowTitle;

        Scene m_activeScene;
        nlohmann::json m_sceneBackup;

        // Render texture
        RenderTexture2D m_gameTexture; // What the Player sees

        std::string m_currentScenePath = "";

        // Stores the path of the scene waiting to be loaded
        std::string m_pendingScenePath = "";

#ifndef STANDALONE_MODE
        std::unique_ptr<Editor> m_editor;
#endif

        // Static pointer holding the unique Engine instance
        static Engine* s_instance;
};
