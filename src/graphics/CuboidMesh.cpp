#include "CuboidMesh.hpp"
#include "physics/Transform3d.hpp"
#include <rlgl.h> // Required to push matrices directly to the GPU

#ifndef STANDALONE_MODE
#include <imgui.h>
#include "editor/commands/CommandHistory.hpp"
#include "editor/commands/ModifyComponentCommand.hpp"
#include "editor/EditorUI.hpp"
#endif

CuboidMesh::CuboidMesh() 
    : size{1.0f, 1.0f, 1.0f}, color(WHITE), isVisible(true), drawWireframe(false) 
{
}

void CuboidMesh::Render() {
    if (!isVisible || !owner) return;

    auto transform = owner->GetComponent<Transform3d>();
    if (!transform) return;

    // Extract the global transformation matrix
    Matrix mat = transform->GetGlobalMatrix();

    // Raylib's rlMultMatrixf expects a flat float array (16 elements)
    // We safely unpack the matrix here to match the OpenGL memory layout
    float fmat[16] = {
        mat.m0, mat.m1, mat.m2, mat.m3,
        mat.m4, mat.m5, mat.m6, mat.m7,
        mat.m8, mat.m9, mat.m10, mat.m11,
        mat.m12, mat.m13, mat.m14, mat.m15
    };

    // Push the current matrix state to the OpenGL stack
    rlPushMatrix();
    
    // Apply our Transform3d matrix directly to the GPU rendering context
    rlMultMatrixf(fmat);

    // Because the translation, rotation, and scale are ALREADY applied by the matrix above,
    // we simply draw the cube at the local origin (0, 0, 0).
    DrawCube(Vector3{0.0f, 0.0f, 0.0f}, size.x, size.y, size.z, color);

    // Optional wireframe for debugging or PS1 aesthetic
    if (drawWireframe) {
        // Draw slightly darker lines around the edges
        Color wireColor = { 
            (unsigned char)(color.r * 0.5f), 
            (unsigned char)(color.g * 0.5f), 
            (unsigned char)(color.b * 0.5f), 
            255 
        };
        DrawCubeWires(Vector3{0.0f, 0.0f, 0.0f}, size.x, size.y, size.z, wireColor);
    }

    // Pop the matrix to avoid affecting other objects drawn after this one
    rlPopMatrix();
}

std::string CuboidMesh::GetName() const {
    return "Cuboid Mesh";
}

#ifndef STANDALONE_MODE
void CuboidMesh::OnInspector() {
    bool tempVis = isVisible;
    if (ImGui::Checkbox("Visible", &tempVis)) {
        nlohmann::json initialState = Serialize();
        isVisible = tempVis;
        CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, initialState, Serialize()));
    }

    ImGui::SameLine();

    bool tempWire = drawWireframe;
    if (ImGui::Checkbox("Wireframe", &tempWire)) {
        nlohmann::json initialState = Serialize();
        drawWireframe = tempWire;
        CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, initialState, Serialize()));
    }

    ImGui::Separator();

    // Size editing
    EditorUI::DragFloat3("Size", &size.x, 0.1f, this);

    // Color editing
    float colorf[4] = { 
        color.r / 255.0f, 
        color.g / 255.0f, 
        color.b / 255.0f, 
        color.a / 255.0f 
    };

    if (ImGui::ColorEdit4("Color", colorf)) {
        nlohmann::json initialState = Serialize();
        
        color.r = (unsigned char)(colorf[0] * 255.0f);
        color.g = (unsigned char)(colorf[1] * 255.0f);
        color.b = (unsigned char)(colorf[2] * 255.0f);
        color.a = (unsigned char)(colorf[3] * 255.0f);
        
        // Only register the command when the user finishes dragging the color picker
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, initialState, Serialize()));
        }
    }
}
#endif

nlohmann::json CuboidMesh::Serialize() const {
    return {
        { "type", "CuboidMesh" },
        { "sizeX", size.x },
        { "sizeY", size.y },
        { "sizeZ", size.z },
        { "color", { color.r, color.g, color.b, color.a } },
        { "isVisible", isVisible },
        { "drawWireframe", drawWireframe }
    };
}

void CuboidMesh::Deserialize(const nlohmann::json& j) {
    size.x = j.value("sizeX", 1.0f);
    size.y = j.value("sizeY", 1.0f);
    size.z = j.value("sizeZ", 1.0f);
    
    if (j.contains("color") && j["color"].is_array() && j["color"].size() == 4) {
        color.r = j["color"][0];
        color.g = j["color"][1];
        color.b = j["color"][2];
        color.a = j["color"][3];
    }

    isVisible = j.value("isVisible", true);
    drawWireframe = j.value("drawWireframe", false);
}
