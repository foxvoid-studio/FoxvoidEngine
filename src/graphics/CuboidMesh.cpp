#include "CuboidMesh.hpp"
#include "physics/Transform3d.hpp"
#include <rlgl.h>

#ifndef STANDALONE_MODE
#include <imgui.h>
#include "editor/commands/CommandHistory.hpp"
#include "editor/commands/ModifyComponentCommand.hpp"
#include "editor/EditorUI.hpp"
#endif
#include "light/LightingSystem.hpp"

static Mesh s_cubeMesh = {0};
static Material s_cubeMaterial = {0};
static bool s_cubeInit = false;

CuboidMesh::CuboidMesh() 
    : size{1.0f, 1.0f, 1.0f}, color(WHITE), isVisible(true), drawWireframe(false) 
{
    // Generate a normalized 1x1x1 mesh. 
    // We will scale it dynamically during rendering using the 'size' property.
    if (!s_cubeInit) {
        s_cubeMesh = GenMeshCube(1.0f, 1.0f, 1.0f);
        s_cubeMaterial = LoadMaterialDefault();
        s_cubeInit = true;
    }
}

void CuboidMesh::Render() {
    if (!isVisible || !owner) return;

    auto transform = owner->GetComponent<Transform3d>();
    if (!transform) return;

    // 1. Calculate the final matrix (Scale the 1x1x1 mesh by our 'size' property)
    Matrix scaleMat = MatrixScale(size.x, size.y, size.z);
    
    // Multiply local size scale with the global transform matrix
    Matrix finalMat = MatrixMultiply(scaleMat, transform->GetGlobalMatrix());

    if (LightingSystem::IsShadowPass()) {
        s_cubeMaterial.shader = LightingSystem::GetDefaultShader();
        s_cubeMaterial.maps[MATERIAL_MAP_EMISSION].texture = { 0 };
    } else {
        Shader lightShader = LightingSystem::GetShader();
        if (lightShader.id != 0) {
            s_cubeMaterial.shader = lightShader;
            s_cubeMaterial.maps[MATERIAL_MAP_EMISSION].texture = LightingSystem::GetShadowTexture();
        }
    }

    // 4. Send the transformation matrix to the shader and draw the mesh!
    s_cubeMaterial.maps[MATERIAL_MAP_DIFFUSE].color = color;
    LightingSystem::SetObjectModelMatrix(finalMat);
    DrawMesh(s_cubeMesh, s_cubeMaterial, finalMat);

    // 5. Optional wireframe for debugging (Does not require lighting)
    if (drawWireframe) {
        float fmat[16] = {
            finalMat.m0, finalMat.m1, finalMat.m2, finalMat.m3,
            finalMat.m4, finalMat.m5, finalMat.m6, finalMat.m7,
            finalMat.m8, finalMat.m9, finalMat.m10, finalMat.m11,
            finalMat.m12, finalMat.m13, finalMat.m14, finalMat.m15
        };

        rlPushMatrix();
        rlMultMatrixf(fmat);
        
        Color wireColor = { 
            (unsigned char)(color.r * 0.5f), 
            (unsigned char)(color.g * 0.5f), 
            (unsigned char)(color.b * 0.5f), 
            255 
        };
        
        // Because finalMat already contains the scaling factor, we draw a 1x1x1 wire cube.
        DrawCubeWires(Vector3{0.0f, 0.0f, 0.0f}, 1.0f, 1.0f, 1.0f, wireColor);
        
        rlPopMatrix();
    }
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
