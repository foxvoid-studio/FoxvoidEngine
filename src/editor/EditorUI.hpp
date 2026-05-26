#pragma once

#ifndef STANDALONE_MODE
#include <imgui.h>
#endif
#include <raylib.h>
#include "world/Component.hpp"

class EditorUI {
    public:
        // Helper to draw a 2-float Drag control (e.g., Vector2) with Undo/Redo tracking
        static bool DragFloat2(const char* label, float v[2], float v_speed, Component* component, float v_min = 0.0f, float v_max = 0.0f);
        
        // Helper to draw a 3-float Drag control (e.g.n Vector3) with Undo/Redo tracking
        static bool DragFloat3(const char* label, float v[3], float v_speed, Component* component, float v_min = 0.0f, float v_max = 0.0f);

        // Helper to draw a 4-float Drag control (e.g., Rectangle bounds) with Undo/Redo tracking
        static bool DragFloat4(const char* label, float v[4], float v_speed, Component* component, float v_min = 0.0f, float v_max = 0.0f);

        // Helper to draw a single float Drag control with Undo/Redo tracking
        static bool DragFloat(const char* label, float* v, float v_speed, Component* component, float v_min = 0.0f, float v_max = 0.0f);

        // Helper to draw a Checkbox with Undo/Redo tracking
        static bool Checkbox(const char* label, bool* v, Component* component);

        // Helper to draw a Color picker that directly modifies a Raylib color
        static bool ColorEdit4(const char* label, Color* color, Component* component);

        // Helper to draw a single int Drag control with Undo/Redo tracking
        static bool DragInt(const char* label, int* v, float v_speed, Component* component, int v_min = 0, int v_max = 0);
};
