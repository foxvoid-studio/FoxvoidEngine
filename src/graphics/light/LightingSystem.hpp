#pragma once

#include <raylib.h>
#include <raymath.h>

class Scene;

class [[gnu::visibility("default")]] LightingSystem {
    public:
        // Global ambient setting
        static float ambientIntensity;
        static bool castShadows;

        // Initializes the main lighting shader and the shadow mapping framebuffer
        static void Initialize();

        // Frees GPU memory for shaders and render textures
        static void Shutdown();

        // Gathers Directional and Point lights in the scene, calculates the shadow camera, and updates GPU uniforms
        static void Update(const Scene& scene, Camera3D camera);

        // Starts rendering the scene from the directional light's perspective to build the depth map
        static void BeginShadowRender();

        // Ends the shadow render pass and resets the rendering target
        static void EndShadowRender();

        // Sends a specific object's transformation matrices to the shader before drawing
        static void SetObjectModelMatrix(const Matrix& mat);
        
        // Exposes the raw shader so it can be applied to custom materials
        static Shader GetShader();
        
        // Returns the generated depth texture to be bound to materials
        static Texture2D GetShadowTexture();

        static bool IsShadowPass() { return s_isShadowPass; }

        static Shader GetDefaultShader();

    private:
        static Shader s_defaultShader;
        static bool s_isShadowPass;
        static Shader s_shader;
        static RenderTexture2D s_shadowMap;
        static Camera3D s_lightCamera;

        // Cached uniform locations for standard rendering parameters
        static int s_viewPosLoc;
        static int s_ambientLoc;
        static int s_lightCountLoc;
        static int s_matModelLoc;
        static int s_matNormalLoc;
        static int s_lightVPLoc; 
        
        // Cached uniform locations for the array of dynamic lights
        struct LightLocs {
            int type;
            int position;
            int direction;
            int color;
            int intensity;
            int radius;
        };
        static LightLocs s_lightLocs[4]; // MAX_LIGHTS = 4
};
