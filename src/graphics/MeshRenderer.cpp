#include "MeshRenderer.hpp"
#include "graphics/light/LightingSystem.hpp"
#include <iostream>
#include <cstring>
#include <fstream>
#include <physics/Transform3d.hpp>

#ifndef STANDALONE_MODE
#include <imgui.h>
#include "editor/commands/CommandHistory.hpp"
#include "editor/commands/ModifyComponentCommand.hpp"
#endif
#include <core/AssetRegistry.hpp>

// Private helper to recursively parse GLTF nodes from JSON
namespace {
    ModelNode ParseGLTFNode(int nodeIndex, const nlohmann::json& gltf, const std::vector<std::vector<int>>& nodeToMeshes) {
        ModelNode node;
        node.gltfNodeIndex = nodeIndex; // Save the unique index
        const auto& jNode = gltf["nodes"][nodeIndex];

        if (jNode.contains("name")) node.name = jNode["name"];
        else node.name = "node_" + std::to_string(nodeIndex);

        if (jNode.contains("mesh")) {
            node.meshIndices = nodeToMeshes[nodeIndex];
        }

        // 1. Read static matrices safely without breaking them
        if (jNode.contains("matrix")) {
            node.hasMatrix = true;
            const auto& m = jNode["matrix"];
            
            node.localMatrix.m0 = m[0].get<float>();   node.localMatrix.m4 = m[4].get<float>();   node.localMatrix.m8 = m[8].get<float>();     node.localMatrix.m12 = m[12].get<float>();
            node.localMatrix.m1 = m[1].get<float>();   node.localMatrix.m5 = m[5].get<float>();   node.localMatrix.m9 = m[9].get<float>();     node.localMatrix.m13 = m[13].get<float>();
            node.localMatrix.m2 = m[2].get<float>();   node.localMatrix.m6 = m[6].get<float>();   node.localMatrix.m10 = m[10].get<float>();   node.localMatrix.m14 = m[14].get<float>();
            node.localMatrix.m3 = m[3].get<float>();   node.localMatrix.m7 = m[7].get<float>();   node.localMatrix.m11 = m[11].get<float>();   node.localMatrix.m15 = m[15].get<float>();
        } 
        // 2. Read animated TRS properties
        else {
            if (jNode.contains("translation")) {
                node.translation = { jNode["translation"][0], jNode["translation"][1], jNode["translation"][2] };
            }
            if (jNode.contains("rotation")) {
                node.rotation = { jNode["rotation"][0], jNode["rotation"][1], jNode["rotation"][2], jNode["rotation"][3] };
            }
            if (jNode.contains("scale")) {
                node.scale = { jNode["scale"][0], jNode["scale"][1], jNode["scale"][2] };
            }
        }

        if (jNode.contains("children")) {
            for (int childIndex : jNode["children"]) {
                node.children.push_back(ParseGLTFNode(childIndex, gltf, nodeToMeshes));
            }
        }
        return node;
    }
}

// Computes the initial static state of the hierarchy to reverse Raylib's vertex baking
void MeshRenderer::ComputeInitialTransforms(ModelNode& node, const Matrix& parentMatrix) {
    Matrix localMat;
    
    // Apply static matrix OR calculate from TRS properties
    if (node.hasMatrix) {
        localMat = node.localMatrix;
    } else {
        Matrix scaleMat = MatrixScale(node.scale.x, node.scale.y, node.scale.z);
        Matrix rotationMat = QuaternionToMatrix(node.rotation);
        Matrix translationMat = MatrixTranslate(node.translation.x, node.translation.y, node.translation.z);

        localMat = MatrixMultiply(MatrixMultiply(scaleMat, rotationMat), translationMat);
    }

    // Raylib internal hierarchy order: local * parent
    node.initialGlobalMatrix = MatrixMultiply(localMat, parentMatrix);
    
    node.inverseBindMatrix = MatrixInvert(node.initialGlobalMatrix);

    for (auto& child : node.children) {
        ComputeInitialTransforms(child, node.initialGlobalMatrix);
    }
}

void MeshRenderer::LoadCustomHierarchy(const std::string& path) {
    usesCustomHierarchy = false;
    rootNode.children.clear();
    rootNode.meshIndices.clear();
    m_gltfJsonCache.clear();
    m_glbBinBuffer.clear();

    std::ifstream file(path, std::ios::binary);
    if (!file) return;

    uint32_t magic, version, length;
    file.read((char*)&magic, 4);
    file.read((char*)&version, 4);
    file.read((char*)&length, 4);

    if (magic != 0x46546C67) return; 

    uint32_t chunkLength, chunkType;
    file.read((char*)&chunkLength, 4);
    file.read((char*)&chunkType, 4);

    if (chunkType != 0x4E4F534A) return; 

    std::string jsonStr;
    jsonStr.resize(chunkLength);
    file.read(&jsonStr[0], chunkLength);

    try {
        m_gltfJsonCache = nlohmann::json::parse(jsonStr);

        int sceneIndex = m_gltfJsonCache.value("scene", 0);
        const auto& scene = m_gltfJsonCache["scenes"][sceneIndex];

        if (model.boneCount == 0 && scene.contains("nodes")) {
            std::vector<std::vector<int>> nodeToRaylibMeshes(m_gltfJsonCache["nodes"].size());
            int currentRaylibMeshIndex = 0;
            
            for (size_t i = 0; i < m_gltfJsonCache["nodes"].size(); ++i) {
                if (m_gltfJsonCache["nodes"][i].contains("mesh")) {
                    int gltfMeshIdx = m_gltfJsonCache["nodes"][i]["mesh"];
                    int primitiveCount = m_gltfJsonCache["meshes"][gltfMeshIdx]["primitives"].size();
                    
                    for (int p = 0; p < primitiveCount; p++) {
                        nodeToRaylibMeshes[i].push_back(currentRaylibMeshIndex++);
                    }
                }
            }
            
            for (int rootIndex : scene["nodes"]) {
                rootNode.children.push_back(ParseGLTFNode(rootIndex, m_gltfJsonCache, nodeToRaylibMeshes));
            }

            file.seekg(12 + 8 + chunkLength, std::ios::beg);
            uint32_t binChunkLength = 0;
            uint32_t binChunkType = 0;
            if (file.read((char*)&binChunkLength, 4) && file.read((char*)&binChunkType, 4)) {
                m_glbBinBuffer.resize(binChunkLength);
                file.read((char*)m_glbBinBuffer.data(), binChunkLength);
            }

            Matrix identity = MatrixIdentity();
            for (auto& child : rootNode.children) {
                ComputeInitialTransforms(child, identity);
            }

            usesCustomHierarchy = true;
        }

    } catch (const std::exception& e) {
        std::cerr << "[MeshRenderer] Error parsing GLB JSON: " << e.what() << std::endl;
    }
}

MeshRenderer::MeshRenderer() = default;

MeshRenderer::~MeshRenderer() {
    UnloadCurrentModel();
}

std::string MeshRenderer::GetName() const {
    return "Mesh Renderer";
}

void MeshRenderer::LoadModelFromPath(const std::string& path) {
    if (path.empty()) {
        LoadModelFromUUID(UUID(0));
        return;
    }
    
    UUID assetId = AssetRegistry::GetUUIDForPath(path);
    LoadModelFromUUID(assetId);
}

void MeshRenderer::LoadModelFromUUID(UUID uuid) {
    m_modelUUID = uuid;
    UnloadCurrentModel();

    if (m_modelUUID != 0) {
        std::string resolvedPath = AssetRegistry::GetPathForUUID(m_modelUUID).string();
        
        if (!resolvedPath.empty()) {
            model = LoadModel(resolvedPath.c_str());
            
            if (model.meshCount > 0) {
                isLoaded = true;
                Shader lightShader = LightingSystem::GetShader();
                
                if (lightShader.id != 0) {
                    for (int i = 0; i < model.materialCount; i++) {
                        model.materials[i].shader = lightShader;
                    }
                }

                LoadCustomHierarchy(resolvedPath);
            } else {
                std::cerr << "[MeshRenderer] Failed to load model: " << resolvedPath << std::endl;
            }
        } else {
            std::cerr << "[MeshRenderer] Error: Could not resolve UUID " << (uint64_t)m_modelUUID << " to a valid path!" << std::endl;
        }
    }
}

void MeshRenderer::UnloadCurrentModel() {
    if (isLoaded) {
        UnloadModel(model);
        isLoaded = false;
        usesCustomHierarchy = false;
        m_gltfJsonCache.clear();
        m_glbBinBuffer.clear();
    }
}

void MeshRenderer::Render() {
    if (!isLoaded || !owner) return;

    auto transform = owner->GetComponent<Transform3d>();
    if (!transform) return;

    Matrix entityGlobalMat = transform->GetGlobalMatrix();

    if (usesCustomHierarchy) {
        RenderNodeRecursive(rootNode, entityGlobalMat);
    } 
    else {
        model.transform = entityGlobalMat;
        DrawModel(model, Vector3Zero(), 1.0f, tint);
    }
}

void MeshRenderer::RenderNodeRecursive(ModelNode& node, const Matrix& parentGlobalMatrix) {
    Matrix localMat;
    
    // Apply static matrix OR calculate animated TRS
    if (node.hasMatrix) {
        localMat = node.localMatrix;
    } else {
        Matrix scaleMat = MatrixScale(node.scale.x, node.scale.y, node.scale.z);
        Matrix rotationMat = QuaternionToMatrix(node.rotation);
        Matrix translationMat = MatrixTranslate(node.translation.x, node.translation.y, node.translation.z);

        localMat = MatrixMultiply(MatrixMultiply(scaleMat, rotationMat), translationMat);
    }

    // Raylib internal hierarchy order -> local * parent
    node.globalMatrix = MatrixMultiply(localMat, parentGlobalMatrix);

    if (!node.meshIndices.empty()) {
        
        // CRITICAL FIX : Inverse must be the left operand to unbake the raylib transform
        Matrix unbakedMatrix = MatrixMultiply(node.inverseBindMatrix, node.globalMatrix);
        
        LightingSystem::SetObjectModelMatrix(unbakedMatrix);

        for (int mIdx : node.meshIndices) {
            if (mIdx >= 0 && mIdx < model.meshCount) {
                Mesh mesh = model.meshes[mIdx];
                int matIndex = model.meshMaterial[mIdx];
                Material material = model.materials[matIndex];

                DrawMesh(mesh, material, unbakedMatrix);
            }
        }
    }

    for (auto& child : node.children) {
        RenderNodeRecursive(child, node.globalMatrix);
    }
}

#ifndef STANDALONE_MODE
void MeshRenderer::OnInspector() {
    std::string currentPath = "";
    if (m_modelUUID != 0) {
        currentPath = AssetRegistry::GetPathForUUID(m_modelUUID).string();
    }

    char buffer[256];
    strncpy(buffer, currentPath.c_str(), sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';

    if (ImGui::InputText("Model Path", buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
        std::string newPath(buffer);
        if (newPath != currentPath) {
            nlohmann::json initialState = Serialize();
            LoadModelFromPath(newPath); 
            CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, initialState, Serialize()));
        }
    }

    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
            
            std::string droppedPath = (const char*)payload->Data;
            std::filesystem::path fsPath(droppedPath);
            std::string ext = fsPath.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            
            if (ext == ".glb" || ext == ".gltf" || ext == ".obj" || ext == ".iqm") {
                nlohmann::json initialState = Serialize();
                LoadModelFromPath(droppedPath);
                CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, initialState, Serialize()));
            }
        }
        ImGui::EndDragDropTarget();
    }
    
    ImGui::TextDisabled("Press ENTER to load new model");

    if (isLoaded) {
        ImGui::Text("Meshes: %d | Materials: %d", model.meshCount, model.materialCount);
        ImGui::TextDisabled("UUID: %llu", (uint64_t)m_modelUUID);
    
        if (usesCustomHierarchy) {
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Mode: Hierarchical (Node Animation)");
        } else {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 1.0f, 1.0f), "Mode: Standard (Skeletal/Static)");
        }
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "No model loaded!");
    }

    float colorFloat[4] = { tint.r / 255.0f, tint.g / 255.0f, tint.b / 255.0f, tint.a / 255.0f };
    
    if (ImGui::ColorEdit4("Tint", colorFloat)) {
        tint.r = static_cast<unsigned char>(colorFloat[0] * 255.0f);
        tint.g = static_cast<unsigned char>(colorFloat[1] * 255.0f);
        tint.b = static_cast<unsigned char>(colorFloat[2] * 255.0f);
        tint.a = static_cast<unsigned char>(colorFloat[3] * 255.0f);
    }
}
#endif

nlohmann::json MeshRenderer::Serialize() const {
    return {
        { "type", "MeshRenderer" },
        { "modelUUID", (uint64_t)m_modelUUID },
        { "tint", { tint.r, tint.g, tint.b, tint.a } }
    };
}

void MeshRenderer::Deserialize(const nlohmann::json& j) {
    if (j.contains("tint") && j["tint"].is_array() && j["tint"].size() == 4) {
        tint.r = j["tint"][0];
        tint.g = j["tint"][1];
        tint.b = j["tint"][2];
        tint.a = j["tint"][3];
    }
    
    if (j.contains("modelUUID")) {
        LoadModelFromUUID(UUID(j["modelUUID"].get<uint64_t>()));
    } 
    else if (j.contains("modelPath")) {
        LoadModelFromPath(j["modelPath"].get<std::string>());
    }
}
