#pragma once

#include <raylib.h>

class EditorCamera {
    public:
        // Initialize both 2D and 3D cameras
        EditorCamera(float screenWidth, float screenHeight);

        // Handles user input for zooming, panning (2D) and flying (3D)
        void Update(bool isWindowHovered);

        // --- 2D API ---
        void Begin2D();
        void End2D();
        void DrawGrid2D(int slices, float spacing);
        Camera2D GetCamera2D() const { return m_camera2D; }

        // --- 3D API ---
        void Begin3D();
        void End3D();
        void DrawGrid3D(int slices, float spacing);
        Camera3D GetCamera3D() const { return m_camera3D; }

    private:
        // 2D State
        Camera2D m_camera2D;
        bool m_isPanning2D;

        // 3D State
        Camera3D m_camera3D;
        bool m_isFlying3D;
        float m_yaw;
        float m_pitch;

        // Internal math helper for FPS rotation
        void Update3DCameraTarget();
};
