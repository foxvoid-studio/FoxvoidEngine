#pragma once

#include <raylib.h>

class Scene;

class LightingSystem {
    public:
        // Loads the shader based on the target platform (Desktop vs Web/Android)
        static void Initialize();

        // Unloads the shader from the GPU
        static void Shutdown();

        // Gathers all lights in the scene and sends their data to the shader
        static void Update(const Scene& scene, Camera3D camera);

        // Activates the lighting shader for rendering
        static void Begin();

        // Deactivates the lighting shader
        static void End();

        // Send the object's transform to the shader before drawing it
        static void SetObjectModelMatrix(Matrix modelMat);
        
        // Exposes the raw shader if needed by other systems (e.g., custom materials)
        static Shader GetShader();

    private:
        static Shader s_shader;
        static int s_viewPosLoc;
        static int s_lightCountLoc;

        // Locations for the Model and Normal matrices
        static int s_matModelLoc;
        static int s_matNormalLoc;
        
        // Cache the uniform locations for optimal performance
        struct LightLocs {
            int type;
            int position;
            int direction;
            int color;
            int intensity;
            int radius;
        };
        static LightLocs s_lightLocs[4]; // MAX_LIGHTS is 4 in our shader
};
