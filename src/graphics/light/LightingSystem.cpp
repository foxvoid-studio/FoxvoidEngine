#include "LightingSystem.hpp"
#include "DirectionalLight.hpp"
#include "PointLight.hpp"
#include "physics/Transform3d.hpp"
#include <raymath.h>
#include <iostream>
#include "world/Scene.hpp"

// Cross-platform GLSL version detection
#if defined(__EMSCRIPTEN__) || defined(PLATFORM_ANDROID)
    #define GLSL_VERSION 100
#else
    #define GLSL_VERSION 330
#endif

Shader LightingSystem::s_shader = { 0 };
int LightingSystem::s_viewPosLoc = 0;
int LightingSystem::s_lightCountLoc = 0;
LightingSystem::LightLocs LightingSystem::s_lightLocs[4];
int LightingSystem::s_matModelLoc = 0;
int LightingSystem::s_matNormalLoc = 0;

void LightingSystem::Initialize() {
    // Generate the correct file paths based on the platform
    const char* vsPath = TextFormat("assets/shaders/lighting_v%i.vs", GLSL_VERSION);
    const char* fsPath = TextFormat("assets/shaders/lighting_v%i.fs", GLSL_VERSION);

    s_shader = LoadShader(vsPath, fsPath);

    if (s_shader.id != 0) {
        std::cout << "[LightingSystem] Successfully loaded shaders (GLSL " << GLSL_VERSION << ")." << std::endl;
        
        // Get the location of the camera position uniform
        s_viewPosLoc = GetShaderLocation(s_shader, "viewPos");
        s_lightCountLoc = GetShaderLocation(s_shader, "lightCount");

        // Get the locations for the object matrices
        s_matModelLoc = GetShaderLocation(s_shader, "matModel");
        s_matNormalLoc = GetShaderLocation(s_shader, "matNormal");

        // Cache the locations of the light arrays
        for (int i = 0; i < 4; i++) {
            s_lightLocs[i].type      = GetShaderLocation(s_shader, TextFormat("lights[%i].type", i));
            s_lightLocs[i].position  = GetShaderLocation(s_shader, TextFormat("lights[%i].position", i));
            s_lightLocs[i].direction = GetShaderLocation(s_shader, TextFormat("lights[%i].direction", i));
            s_lightLocs[i].color     = GetShaderLocation(s_shader, TextFormat("lights[%i].color", i));
            s_lightLocs[i].intensity = GetShaderLocation(s_shader, TextFormat("lights[%i].intensity", i));
            s_lightLocs[i].radius    = GetShaderLocation(s_shader, TextFormat("lights[%i].radius", i));
        }
    } else {
        std::cerr << "[LightingSystem] Failed to load shaders!" << std::endl;
    }
}

void LightingSystem::Shutdown() {
    if (s_shader.id != 0) {
        UnloadShader(s_shader);
    }
}

void LightingSystem::Update(const Scene& scene, Camera3D camera) {
    if (s_shader.id == 0) return;

    // 1. Update Camera Position for Specular lighting
    SetShaderValue(s_shader, s_viewPosLoc, &camera.position, SHADER_UNIFORM_VEC3);

    // 2. Gather Lights
    int lightCount = 0;

    for (const auto& go : scene.GetGameObjects()) {
        if (!go->IsActiveInHierarchy()) continue;

        auto dirLight = go->GetComponent<DirectionalLight>();
        auto pointLight = go->GetComponent<PointLight>();
        auto transform = go->GetComponent<Transform3d>();

        if (!transform) continue;

        // Process Directional Lights
        if (dirLight && lightCount < 4) {
            int type = 0;
            SetShaderValue(s_shader, s_lightLocs[lightCount].type, &type, SHADER_UNIFORM_INT);
            
            // Calculate forward direction (Looking down the -Z axis of the object)
            Vector3 forward = Vector3RotateByQuaternion({0.0f, 0.0f, -1.0f}, transform->GetGlobalQuaternion());
            SetShaderValue(s_shader, s_lightLocs[lightCount].direction, &forward, SHADER_UNIFORM_VEC3);
            
            // Normalize color to 0.0 - 1.0 range
            Vector4 colorNorm = { dirLight->color.r / 255.0f, dirLight->color.g / 255.0f, dirLight->color.b / 255.0f, dirLight->color.a / 255.0f };
            SetShaderValue(s_shader, s_lightLocs[lightCount].color, &colorNorm, SHADER_UNIFORM_VEC4);
            
            SetShaderValue(s_shader, s_lightLocs[lightCount].intensity, &dirLight->intensity, SHADER_UNIFORM_FLOAT);
            
            lightCount++;
        }
        
        // Process Point Lights
        if (pointLight && lightCount < 4) {
            int type = 1;
            SetShaderValue(s_shader, s_lightLocs[lightCount].type, &type, SHADER_UNIFORM_INT);
            
            Vector3 pos = transform->GetGlobalPosition();
            SetShaderValue(s_shader, s_lightLocs[lightCount].position, &pos, SHADER_UNIFORM_VEC3);
            
            // Normalize color to 0.0 - 1.0 range
            Vector4 colorNorm = { pointLight->color.r / 255.0f, pointLight->color.g / 255.0f, pointLight->color.b / 255.0f, pointLight->color.a / 255.0f };
            SetShaderValue(s_shader, s_lightLocs[lightCount].color, &colorNorm, SHADER_UNIFORM_VEC4);
            
            SetShaderValue(s_shader, s_lightLocs[lightCount].intensity, &pointLight->intensity, SHADER_UNIFORM_FLOAT);
            SetShaderValue(s_shader, s_lightLocs[lightCount].radius, &pointLight->radius, SHADER_UNIFORM_FLOAT);
            
            lightCount++;
        }
    }

    // 3. Inform the shader of how many lights are active this frame
    SetShaderValue(s_shader, s_lightCountLoc, &lightCount, SHADER_UNIFORM_INT);
}

void LightingSystem::Begin() {
    if (s_shader.id != 0) {
        BeginShaderMode(s_shader);
    }
}

void LightingSystem::End() {
    if (s_shader.id != 0) {
        EndShaderMode();
    }
}

void LightingSystem::SetObjectModelMatrix(Matrix modelMat) {
    if (s_shader.id == 0) return;

    // Send the absolute world position/rotation/scale of the object to the Shader
    SetShaderValueMatrix(s_shader, s_matModelLoc, modelMat);

    // Calculate and send the Normal Matrix
    // The Normal Matrix is the Inverse Transpose of the Model Matrix.
    // It guarantees that light bounces correctly even if the object is squashed (non-uniform scale).
    Matrix matNormal = MatrixTranspose(MatrixInvert(modelMat));
    SetShaderValueMatrix(s_shader, s_matNormalLoc, matNormal);
}

Shader LightingSystem::GetShader() {
    return s_shader;
}
