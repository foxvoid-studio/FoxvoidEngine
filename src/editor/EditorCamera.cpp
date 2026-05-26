#include "EditorCamera.hpp"
#include <raymath.h>

EditorCamera::EditorCamera(float screenWidth, float screenHeight) {
    // --- 2D Setup ---
    m_camera2D = { 0 };
    m_camera2D.offset = { screenWidth / 2.0f, screenHeight / 2.0f };
    m_camera2D.target = { 0.0f, 0.0f };
    m_camera2D.rotation = 0.0f;
    m_camera2D.zoom = 1.0f;
    m_isPanning2D = false;

    // --- 3D Setup ---
    m_camera3D = { 0 };
    m_camera3D.position = { 0.0f, 5.0f, 10.0f }; // Slightly elevated and pulled back
    m_camera3D.target = { 0.0f, 0.0f, 0.0f };
    m_camera3D.up = { 0.0f, 1.0f, 0.0f };
    m_camera3D.fovy = 60.0f;
    m_camera3D.projection = CAMERA_PERSPECTIVE;
    
    m_isFlying3D = false;
    m_yaw = -90.0f;   // Look down the -Z axis
    m_pitch = -20.0f; // Look slightly down at the grid
    Update3DCameraTarget();
}

void EditorCamera::Update3DCameraTarget() {
    // Convert Euler angles (yaw, pitch) into a directional Forward vector
    Vector3 forward;
    forward.x = cos(m_yaw * DEG2RAD) * cos(m_pitch * DEG2RAD);
    forward.y = sin(m_pitch * DEG2RAD);
    forward.z = sin(m_yaw * DEG2RAD) * cos(m_pitch * DEG2RAD);
    
    // Normalize and add to the camera's position to define where it's looking
    forward = Vector3Normalize(forward);
    m_camera3D.target = Vector3Add(m_camera3D.position, forward);
}

void EditorCamera::Update(bool isWindowHovered) {
    float wheel = GetMouseWheelMove();
    float dt = GetFrameTime();

    // ==========================================
    // 2D CAMERA LOGIC (Middle Mouse)
    // ==========================================
    if (isWindowHovered && !m_isFlying3D) {
        if (wheel != 0) {
            m_camera2D.zoom += wheel * 0.1f;
            if (m_camera2D.zoom < 0.1f) m_camera2D.zoom = 0.1f;
            if (m_camera2D.zoom > 10.0f) m_camera2D.zoom = 10.0f;
        }
    }

    if (isWindowHovered && IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) {
        m_isPanning2D = true;
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_MIDDLE)) {
        m_isPanning2D = false;
    }

    if (m_isPanning2D) {
        Vector2 mouseDelta = GetMouseDelta();
        m_camera2D.target.x -= mouseDelta.x / m_camera2D.zoom;
        m_camera2D.target.y -= mouseDelta.y / m_camera2D.zoom;
    }

    // ==========================================
    // 3D CAMERA LOGIC (Right Mouse)
    // ==========================================
    if (isWindowHovered && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        m_isFlying3D = true;
        DisableCursor(); // Lock cursor to window for infinite FPS dragging
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) {
        m_isFlying3D = false;
        EnableCursor();
    }

    if (m_isFlying3D) {
        Vector2 mouseDelta = GetMouseDelta();
        
        // 1. Mouse Look (Rotation)
        float mouseSensitivity = 0.2f;
        m_yaw += mouseDelta.x * mouseSensitivity;
        m_pitch -= mouseDelta.y * mouseSensitivity; // Inverted Y-axis

        // Clamp pitch to avoid flipping upside down
        if (m_pitch > 89.0f) m_pitch = 89.0f;
        if (m_pitch < -89.0f) m_pitch = -89.0f;

        // 2. Calculate Direction Vectors for movement
        Vector3 forward = Vector3Subtract(m_camera3D.target, m_camera3D.position);
        forward = Vector3Normalize(forward);
        Vector3 right = Vector3CrossProduct(forward, m_camera3D.up);
        right = Vector3Normalize(right);

        // 3. WASD Movement
        float speed = 15.0f * dt;
        if (IsKeyDown(KEY_LEFT_SHIFT)) speed *= 3.0f; // Sprint

        if (IsKeyDown(KEY_W)) m_camera3D.position = Vector3Add(m_camera3D.position, Vector3Scale(forward, speed));
        if (IsKeyDown(KEY_S)) m_camera3D.position = Vector3Subtract(m_camera3D.position, Vector3Scale(forward, speed));
        if (IsKeyDown(KEY_D)) m_camera3D.position = Vector3Add(m_camera3D.position, Vector3Scale(right, speed));
        if (IsKeyDown(KEY_A)) m_camera3D.position = Vector3Subtract(m_camera3D.position, Vector3Scale(right, speed));
        if (IsKeyDown(KEY_E)) m_camera3D.position.y += speed; // Elevate
        if (IsKeyDown(KEY_Q)) m_camera3D.position.y -= speed; // Descend

        Update3DCameraTarget();
    } 
    else if (isWindowHovered && wheel != 0) {
        // Scroll to Dolly (move forward/backward) 3D camera when not flying
        Vector3 forward = Vector3Subtract(m_camera3D.target, m_camera3D.position);
        forward = Vector3Normalize(forward);
        m_camera3D.position = Vector3Add(m_camera3D.position, Vector3Scale(forward, wheel * 2.0f));
        Update3DCameraTarget();
    }
}

void EditorCamera::Begin2D() {
    BeginMode2D(m_camera2D);
}

void EditorCamera::End2D() {
    EndMode2D();
}

void EditorCamera::DrawGrid2D(int slices, float spacing) {
    int halfSlices = slices / 2;
    for (int i = -halfSlices; i <= halfSlices; i++) {
        Color xAxisColor = (i == 0) ? GREEN : LIGHTGRAY;
        Color yAxisColor = (i == 0) ? RED : LIGHTGRAY;
        DrawLine(i * spacing, -halfSlices * spacing, i * spacing, halfSlices * spacing, yAxisColor);
        DrawLine(-halfSlices * spacing, i * spacing, halfSlices * spacing, i * spacing, xAxisColor);
    }
}

void EditorCamera::Begin3D() {
    BeginMode3D(m_camera3D);
}

void EditorCamera::End3D() {
    EndMode3D();
}

void EditorCamera::DrawGrid3D(int slices, float spacing) {
    // Raylib has a native 3D grid function
    DrawGrid(slices, spacing);
}
