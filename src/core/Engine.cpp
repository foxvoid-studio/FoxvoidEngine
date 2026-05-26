#include "Engine.hpp"
#include <iostream>
#include <physics/Transform2d.hpp>
#include <graphics/ShapeRenderer.hpp>
#include <scripting/ScriptEngine.hpp>
#include <scripting/ScriptComponent.hpp>
#include <graphics/SpriteSheetRenderer.hpp>
#include <graphics/Animation2d.hpp>
#include "AssetManager.hpp"
#include <graphics/Animator2d.hpp>
#include "world/ComponentRegistration.hpp"
#include <world/ComponentRegistry.hpp>
#include "InputManager.hpp"
#include "graphics/TileMap.hpp"
#include "GameStateManager.hpp"
#include "Mouse.hpp"

#ifndef STANDALONE_MODE
#include "editor/Editor.hpp"
#include <imgui.h>
#include <rlImGui.h>
#include "ImGuizmo.h"
#include "extras/IconsFontAwesome6.h"
#endif

namespace fs = std::filesystem;

Engine* Engine::s_instance = nullptr;

Engine::Engine(int width, int height, const std::string& title)
    : m_windowWidth(width), 
      m_windowHeight(height), 
      m_windowTitle(title), 
      m_isRunning(false),
      m_isPlaying(false)
{
    // Assign the current instance to our static pointer 
    // so Engine::Get() can return it properly.
    s_instance = this;

    // Prevent double-initialization! 
    // On Android, main.cpp already called InitWindow to boot the OS surface.
    // Calling it again would corrupt the physical viewport (the bottom-left bug).
    if (!IsWindowReady()) {
        // Desktop can be resizable, but Web MUST be strictly locked
#ifndef __EMSCRIPTEN__
        SetConfigFlags(FLAG_WINDOW_RESIZABLE);
#endif
        InitWindow(m_windowWidth, m_windowHeight, m_windowTitle.c_str());
    } else {
        // Window already exists, just update the title
        SetWindowTitle(m_windowTitle.c_str());
    }
    
    InitAudioDevice();

    // Set a target frame rate to avoid burning CPU cycles
    SetTargetFPS(60);

    // One single call to register everything needed for the Scene loading
    EngineSetup::RegisterNativeComponents();

    // Create render texture
    m_gameTexture = LoadRenderTexture(m_windowWidth, m_windowHeight);

#ifndef STANDALONE_MODE
    m_editor = std::make_unique<Editor>(m_windowWidth, m_windowHeight);
#endif

    // If the window was successfully created, mark the engine as running
    if (IsWindowReady()) {
        m_isRunning = true;

#ifdef STANDALONE_MODE
        m_isPlaying = true;
#endif

        std::cout << "[Engine] Initialized successfully." << std::endl;
        ScriptEngine::Initialize();
    } else {
        std::cerr << "[Engine] Failed to initialize window." << std::endl;
    }
}

Engine::~Engine() {
    m_activeScene.Clear();

    ScriptEngine::Shutdown();

    AssetManager::Clear();

    // Free the VRAM
    // Safely unload the render textures from the GPU
    UnloadRenderTexture(m_gameTexture);

    CloseAudioDevice();

    // Close the Raylib window and free graphics resources
    CloseWindow();
    std::cout << "[Engine] Shutdown complete." << std::endl;
}

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>

// Emscripten requires a C-style function pointer for its main loop.
// We use this static wrapper to call the method on our Engine instance.
static void EmscriptenMainLoop(void* arg) {
    static_cast<Engine*>(arg)->MainLoopStep();
}
#endif

void Engine::MainLoopStep() {
    // Calculate the time taken by the last frame (Delta Time)
    float deltaTime = GetFrameTime();

    ProcessInput();
    Update(deltaTime);
    Render();

    // Update input states for the NEXT frame,
    // AFTER all scripts and logic have finished evaluating!
    InputManager::Update();
}

void Engine::Run() {
#if defined(__EMSCRIPTEN__)
    // For WebAssembly, we hand over control of the loop to the browser.
    // parameters: function_to_call, argument, fps (0 = let browser decide), simulate_infinite_loop (1 = true)
    emscripten_set_main_loop_arg(EmscriptenMainLoop, this, 0, 1);
#else
    // Main game loop for Desktop and Android
    while (m_isRunning && !WindowShouldClose()) {
        MainLoopStep();
    }
#endif
}

void Engine::ProcessInput() {
#ifdef STANDALONE_MODE
    // Fetch the physical screen size (which matches the browser window on Web)
    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();

    // Calculate the exact scaling ratio and offsets used by the letterboxing
    float scale = std::min((float)screenWidth / m_windowWidth, (float)screenHeight / m_windowHeight);
    float offsetX = (screenWidth - (m_windowWidth * scale)) * 0.5f;
    float offsetY = (screenHeight - (m_windowHeight * scale)) * 0.5f;

    // Get the raw mouse position in physical screen coordinates
    Vector2 mousePos = GetMousePosition();
    Vector2 virtualMouse;
    
    // Reverse the letterbox math to find the exact coordinate in our virtual game space
    virtualMouse.x = (mousePos.x - offsetX) / scale;
    virtualMouse.y = (mousePos.y - offsetY) / scale;

    // Clamp coordinates to prevent interacting outside the game bounds
    virtualMouse.x = std::clamp(virtualMouse.x, 0.0f, (float)m_windowWidth);
    virtualMouse.y = std::clamp(virtualMouse.y, 0.0f, (float)m_windowHeight);

    // Store the perfectly mapped position in our custom Mouse wrapper
    Mouse::SetVirtualPosition(virtualMouse);
#endif

    // Note: In Editor mode, we do NOT call GetMousePosition() here. 
    // Instead, the Editor will call SetGameMousePosition() during its ImGui pass.
}

void Engine::Update(float deltaTime) {
    // Check if a scene change was requested last frame
    if (!m_pendingScenePath.empty()) {
        std::cout << "[Engine] Loading new scene: " << m_pendingScenePath << std::endl;

        // Destroy all current non-persistent GameObjects and free memory
        m_activeScene.Clear();

        // Load the new JSON (.scene) file
        m_activeScene.LoadFromFile(m_pendingScenePath);

        // Update the editor's path so it knows what we are editing
        m_currentScenePath = m_pendingScenePath;

        // Clear the pending path so we don't load it again next frame
        m_pendingScenePath = "";

        // Trigger the Start() method for all the newly loaded components
        m_activeScene.Start();
    }

    // Only run game logic (Python scripts, animations) if PLAY is active
    if (m_isPlaying) {
        m_activeScene.Update(deltaTime);
    }

    // Always manage memory, even in Edit mode
    // This ensures objects created via the Editor or Engine::Run
    // are immediately added to the scene graph.
    m_activeScene.Flush();
}

void Engine::UpdateResolution(int width, int height) {
    m_windowWidth = width;
    m_windowHeight = height;

    // Unload the old game texture from VRAM to prevent memory leaks
    if (m_gameTexture.id != 0) {
        UnloadRenderTexture(m_gameTexture);
    }

    // Create the new render texture with the project's target resolution
    m_gameTexture = LoadRenderTexture(m_windowWidth, m_windowHeight);

    std::cout << "[Engine] Resolution updated to " << m_windowWidth << "x" << m_windowHeight << std::endl;
}

void Engine::Render() {
    // --- VIRTUAL SPACE (e.g., 1280x720) ---
    // Pass 1: Game Rendering (What the player sees)
    BeginTextureMode(m_gameTexture);
        ClearBackground(m_activeScene.GetMainCameraBackgroundColor());

        if (m_activeScene.Has3DCamera()) {
            BeginMode3D(m_activeScene.GetMainCamera3D());
                m_activeScene.Render3D();
            EndMode3D();
        }
        
        if (m_activeScene.Has2DCamera()) {
            BeginMode2D(m_activeScene.GetMainCamera2D((float)m_windowWidth, (float)m_windowHeight));
                // Draw all entities normally
                m_activeScene.Render2D();
            EndMode2D();
        }

        m_activeScene.RenderHUD();
    EndTextureMode();

#ifndef STANDALONE_MODE
    // Pass 2: Editor Rendering (Handles its own passes and screen drawing)
    BeginDrawing();
        ClearBackground(DARKGRAY);

        if (m_editor) {
            m_editor->Draw(m_activeScene, m_gameTexture, m_isRunning, m_currentScenePath, m_sceneBackup);
        }

    EndDrawing();
#else
    // --- PHYSICAL SCREEN SPACE (e.g., 1920x1080) ---
    // Pass 2: Standalone Rendering (Letterboxing / Pillarboxing)
    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();

    // Calculate scale to fit the target resolution perfectly inside the screen
    float scale = std::min((float)screenWidth / m_windowWidth, (float)screenHeight / m_windowHeight);

    // Calculate the centered offsets
    float offsetX = (screenWidth - (m_windowWidth * scale)) * 0.5f;
    float offsetY = (screenHeight - (m_windowHeight * scale)) * 0.5f;

    BeginDrawing();
        ClearBackground(BLACK); // Color of the letterbox bars

        // The source rectangle is inverted on Y because OpenGL framebuffers are upside down
        Rectangle sourceRec = { 0.0f, 0.0f, (float)m_gameTexture.texture.width, (float)-m_gameTexture.texture.height };
        
        // The destination rectangle positions and scales the texture on the physical screen
        Rectangle destRec = { offsetX, offsetY, m_windowWidth * scale, m_windowHeight * scale };
        
        Vector2 origin = { 0.0f, 0.0f };

        // Draw the final composition: Raylib scales and centers the texture onto the physical screen
        DrawTexturePro(m_gameTexture.texture, sourceRec, destRec, origin, 0.0f, WHITE);
    EndDrawing();
#endif
}

void Engine::LoadScene(const std::string& scenePath) {
    std::cout << "[Engine] Scene change requested: " << scenePath << std::endl;
    m_pendingScenePath = scenePath;
}

void Engine::SetPlaying(bool playing) {
    m_isPlaying = playing;
}

bool Engine::IsPlaying() const { 
    return m_isPlaying; 
}
