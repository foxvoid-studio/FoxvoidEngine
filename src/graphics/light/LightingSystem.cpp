#include "LightingSystem.hpp"
#include "DirectionalLight.hpp"
#include "PointLight.hpp"
#include "world/Scene.hpp"
#include "physics/Transform3d.hpp"
#include <iostream>
#include <algorithm>
#include <rlgl.h>

// Default initialization for global settings
float LightingSystem::ambientIntensity = 0.7f;
bool LightingSystem::castShadows = true;

Shader LightingSystem::s_defaultShader = { 0 };
bool LightingSystem::s_isShadowPass = false;
Shader LightingSystem::s_shader = { 0 };
RenderTexture2D LightingSystem::s_shadowMap = { 0 };
Camera3D LightingSystem::s_lightCamera = { 0 };

int LightingSystem::s_viewPosLoc = -1;
int LightingSystem::s_ambientLoc = -1;
int LightingSystem::s_lightCountLoc = -1;
int LightingSystem::s_matModelLoc = -1;
int LightingSystem::s_matNormalLoc = -1;
int LightingSystem::s_lightVPLoc = -1;
LightingSystem::LightLocs LightingSystem::s_lightLocs[4] = {0};

void LightingSystem::Initialize() {
    // Load the custom lighting shaders based on the target platform
#if defined(GRAPHICS_API_OPENGL_33)
    s_shader = LoadShader("assets/shaders/lighting_v330.vs", "assets/shaders/lighting_v330.fs");
#else
    s_shader = LoadShader("assets/shaders/lighting_v100.vs", "assets/shaders/lighting_v100.fs");
#endif

    if (s_shader.id == 0) {
        std::cerr << "[LightingSystem] ERROR: Failed to load lighting shaders!" << std::endl;
        return;
    }

    // Cache standard uniform locations
    s_viewPosLoc = GetShaderLocation(s_shader, "viewPos");
    s_ambientLoc = GetShaderLocation(s_shader, "ambientIntensity");
    s_lightCountLoc = GetShaderLocation(s_shader, "lightCount");
    s_matModelLoc = GetShaderLocation(s_shader, "matModel");
    s_matNormalLoc = GetShaderLocation(s_shader, "matNormal");
    s_lightVPLoc = GetShaderLocation(s_shader, "lightVP");

    // Bind the shadow map to the emission map slot as a workaround 
    // to pass it to the shader via Raylib's default material pipeline
    s_shader.locs[SHADER_LOC_MAP_EMISSION] = GetShaderLocation(s_shader, "shadowMap");
    s_shader.locs[SHADER_LOC_MATRIX_MODEL] = s_matModelLoc;

    // Cache the locations for the dynamic light array elements
    for (int i = 0; i < 4; i++) {
        s_lightLocs[i].type = GetShaderLocation(s_shader, TextFormat("lights[%i].type", i));
        s_lightLocs[i].position = GetShaderLocation(s_shader, TextFormat("lights[%i].position", i));
        s_lightLocs[i].direction = GetShaderLocation(s_shader, TextFormat("lights[%i].direction", i));
        s_lightLocs[i].color = GetShaderLocation(s_shader, TextFormat("lights[%i].color", i));
        s_lightLocs[i].intensity = GetShaderLocation(s_shader, TextFormat("lights[%i].intensity", i));
        s_lightLocs[i].radius = GetShaderLocation(s_shader, TextFormat("lights[%i].radius", i));
    }

    s_defaultShader = LoadMaterialDefault().shader;

    // Initialize the high-resolution framebuffer for depth rendering
    int shadowResolution = 2048;
    s_shadowMap = LoadRenderTexture(shadowResolution, shadowResolution);
    
    // Raylib generates a Renderbuffer for the depth attachment by default. 
    // We override it here with a true Texture2D so the fragment shader can sample it.
    s_shadowMap.depth.id = rlLoadTextureDepth(shadowResolution, shadowResolution, false);
    s_shadowMap.depth.width = shadowResolution;
    s_shadowMap.depth.height = shadowResolution;
    s_shadowMap.depth.format = 19;       // PIXELFORMAT_UNCOMPRESSED_GRAYSCALE
    s_shadowMap.depth.mipmaps = 1;

    // Re-attach the new depth texture to the Framebuffer
    rlEnableFramebuffer(s_shadowMap.id);
    rlFramebufferAttach(s_shadowMap.id, s_shadowMap.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);
    rlDisableFramebuffer();

    // Configure the orthographic camera used to simulate directional sunlight
    s_lightCamera.target = { 0.0f, 0.0f, 0.0f };
    s_lightCamera.up = { 0.0f, 1.0f, 0.0f };
    s_lightCamera.fovy = 50.0f; // Width/Height of the orthographic projection volume
    s_lightCamera.projection = CAMERA_ORTHOGRAPHIC;

    std::cout << "[LightingSystem] Initialized successfully." << std::endl;
}

void LightingSystem::Shutdown() {
    if (s_shadowMap.id != 0) UnloadRenderTexture(s_shadowMap);
    if (s_shader.id != 0) UnloadShader(s_shader);
}

Shader LightingSystem::GetShader() { 
    return s_shader; 
}

Texture2D LightingSystem::GetShadowTexture() { 
    return s_shadowMap.depth; 
}

void LightingSystem::Update(const Scene& scene, Camera3D camera) {
    // Abort if the shader isn't loaded properly
    if (s_shader.id == 0) return;

    int activeLightCount = 0;
    bool foundDirLight = false;
    // Fallback direction pointing straight down
    Vector3 sunDirNorm = { 0.0f, -1.0f, 0.0f };

    // --- 1. First Pass: Search for the main Directional Light ---
    // The shader strictly uses lights[0] for global shadow mapping.
    for (const auto& go : scene.GetGameObjects()) {
        if (!go->IsActiveInHierarchy()) continue;
        
        auto dirLight = go->GetComponent<DirectionalLight>();
        if (dirLight && !foundDirLight) {
            auto transform = go->GetComponent<Transform3d>();
            if (transform) {
                Matrix mat = transform->GetGlobalMatrix();
                // Extract the Forward vector (-Z axis) from the rotation matrix.
                // The negative signs are critical to ensure the light points IN the right direction,
                // rather than originating FROM it (which would flip lighting and break shadows).
                sunDirNorm = Vector3Normalize({ -mat.m8, -mat.m9, -mat.m10 });
            }

            int typeDir = 0; // 0 = Directional Light
            Vector4 colorNorm = { dirLight->color.r / 255.0f, dirLight->color.g / 255.0f, dirLight->color.b / 255.0f, dirLight->color.a / 255.0f };

            // Send the directional light data to the shader at index 0
            SetShaderValue(s_shader, s_lightLocs[0].type, &typeDir, SHADER_UNIFORM_INT);
            SetShaderValue(s_shader, s_lightLocs[0].direction, &sunDirNorm, SHADER_UNIFORM_VEC3);
            SetShaderValue(s_shader, s_lightLocs[0].color, &colorNorm, SHADER_UNIFORM_VEC4);
            SetShaderValue(s_shader, s_lightLocs[0].intensity, &dirLight->intensity, SHADER_UNIFORM_FLOAT);

            foundDirLight = true;
            activeLightCount = 1;
        }
    }

    // If no directional light is found, inject a dummy light at index 0 with zero intensity.
    // This prevents the shader from reading uninitialized garbage data and corrupting the scene.
    if (!foundDirLight) {
        int typeDir = 0;
        float zeroIntensity = 0.0f;
        Vector4 blackColor = { 0.0f, 0.0f, 0.0f, 1.0f };
        
        SetShaderValue(s_shader, s_lightLocs[0].type, &typeDir, SHADER_UNIFORM_INT);
        SetShaderValue(s_shader, s_lightLocs[0].direction, &sunDirNorm, SHADER_UNIFORM_VEC3);
        SetShaderValue(s_shader, s_lightLocs[0].color, &blackColor, SHADER_UNIFORM_VEC4);
        SetShaderValue(s_shader, s_lightLocs[0].intensity, &zeroIntensity, SHADER_UNIFORM_FLOAT);
        
        activeLightCount = 1; 
    }

    // --- 2. Second Pass: Gather Point Lights ---
    // Fill the remaining slots (indices 1 to MAX_LIGHTS - 1)
    for (const auto& go : scene.GetGameObjects()) {
        if (activeLightCount >= 4) break; // Maximum supported by the current shader
        if (!go->IsActiveInHierarchy()) continue;

        auto ptLight = go->GetComponent<PointLight>();
        if (ptLight) {
            int typePoint = 1; // 1 = Point Light
            auto transform = go->GetComponent<Transform3d>();
            Vector3 pos = transform ? transform->GetGlobalPosition() : Vector3Zero();
            Vector4 col = { ptLight->color.r / 255.0f, ptLight->color.g / 255.0f, ptLight->color.b / 255.0f, ptLight->color.a / 255.0f };

            SetShaderValue(s_shader, s_lightLocs[activeLightCount].type, &typePoint, SHADER_UNIFORM_INT);
            SetShaderValue(s_shader, s_lightLocs[activeLightCount].position, &pos, SHADER_UNIFORM_VEC3);
            SetShaderValue(s_shader, s_lightLocs[activeLightCount].color, &col, SHADER_UNIFORM_VEC4);
            SetShaderValue(s_shader, s_lightLocs[activeLightCount].intensity, &ptLight->intensity, SHADER_UNIFORM_FLOAT);
            SetShaderValue(s_shader, s_lightLocs[activeLightCount].radius, &ptLight->radius, SHADER_UNIFORM_FLOAT);
            
            activeLightCount++;
        }
    }

    // Notify the shader of the total number of active lights to evaluate
    SetShaderValue(s_shader, s_lightCountLoc, &activeLightCount, SHADER_UNIFORM_INT);

    // --- 3. Update the Shadow Camera Positioning ---
    
    // Target the absolute center of the scene to guarantee we capture the static geometry
    s_lightCamera.target = { 0.0f, 0.0f, 0.0f }; 
    
    // Pull the camera far back into the sky along the light's reverse direction
    float distanceToSun = 100.0f;
    s_lightCamera.position = Vector3Subtract(s_lightCamera.target, Vector3Scale(sunDirNorm, distanceToSun));
    
    // Prevent Gimbal Lock: If the light points straight down/up, fallback to the Z-axis for the "Up" vector
    if (fabs(sunDirNorm.x) < 0.001f && fabs(sunDirNorm.z) < 0.001f) {
        s_lightCamera.up = { 0.0f, 0.0f, -1.0f };
    } else {
        s_lightCamera.up = { 0.0f, 1.0f, 0.0f };
    }
    
    // Define the ground area covered by shadows (A 40x40 unit orthographic box)
    s_lightCamera.fovy = 40.0f; 

    // Let Raylib calculate the precise View Matrix based on the parameters above
    Matrix lightView = GetCameraMatrix(s_lightCamera);
    
    // Recreate Raylib's internal Orthographic Projection Matrix
    // We use an aspect ratio of 1.0 because our shadow map FBO is perfectly square (2048x2048)
    double top = s_lightCamera.fovy / 2.0;
    double right = top * 1.0; 
    Matrix lightProj = MatrixOrtho(-right, right, -top, top, 0.01, 1000.0);
    
    // Multiply View and Projection. 
    // Note: In Raylib's math layout, multiplying View * Proj yields the correct MVP logic.
    Matrix lightVP = MatrixMultiply(lightView, lightProj);
    
    // Send the final shadow mapping matrix to the Vertex Shader
    SetShaderValueMatrix(s_shader, s_lightVPLoc, lightVP);

    // --- 4. Send remaining global uniforms ---
    SetShaderValue(s_shader, s_viewPosLoc, &camera.position, SHADER_UNIFORM_VEC3);
    SetShaderValue(s_shader, s_ambientLoc, &ambientIntensity, SHADER_UNIFORM_FLOAT);
}

void LightingSystem::BeginShadowRender() {
    if (!castShadows) return;
    s_isShadowPass = true;
    BeginTextureMode(s_shadowMap);
    ClearBackground(WHITE);
    BeginMode3D(s_lightCamera);
}

void LightingSystem::EndShadowRender() {
    if (!castShadows) return;
    EndMode3D();
    EndTextureMode();
    s_isShadowPass = false;
}

void LightingSystem::SetObjectModelMatrix(const Matrix& mat) {
    if (s_shader.id == 0) return;
    
    SetShaderValueMatrix(s_shader, s_matModelLoc, mat);
    
    // The normal matrix (inverse transpose) is strictly required to calculate 
    // light reflections accurately when non-uniform scaling is applied to the object.
    Matrix matNormal = MatrixTranspose(MatrixInvert(mat));
    SetShaderValueMatrix(s_shader, s_matNormalLoc, matNormal);
}

Shader LightingSystem::GetDefaultShader() { 
    return s_defaultShader; 
}
