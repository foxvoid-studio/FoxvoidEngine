#include "Animator3d.hpp"
#include "MeshRenderer.hpp"
#include <core/AssetRegistry.hpp>
#include <iostream>
#include <cstring>
#include <filesystem>
#include <algorithm>

#ifndef STANDALONE_MODE
#include <imgui.h>
#include "editor/commands/CommandHistory.hpp"
#include "editor/commands/ModifyComponentCommand.hpp"
#endif

// Private tree walker to locate a specific ModelNode by id
namespace {
    ModelNode* FindNodeInTree(ModelNode& node, int index) {
        if (node.gltfNodeIndex == index) return &node;
        for (auto& child : node.children) {
            ModelNode* found = FindNodeInTree(child, index);
            if (found) return found;
        }
        return nullptr;
    }
}

Animator3d::Animator3d() = default;

Animator3d::~Animator3d() {
    UnloadAnimations();
}

std::string Animator3d::GetName() const {
    return "Animator 3D";
}

void Animator3d::LoadCustomAnimationsFromMesh(MeshRenderer* mesh) {
    m_customAnims.clear();
    m_usesCustomAnims = false;

    if (!mesh || !mesh->usesCustomHierarchy || !mesh->m_gltfJsonCache.contains("animations")) return;

    const auto& gltf = mesh->m_gltfJsonCache;
    const auto& bin = mesh->m_glbBinBuffer;

    try {
        for (const auto& jAnim : gltf["animations"]) {
            CustomAnimationData animData;
            animData.name = jAnim.value("name", "Animation_" + std::to_string(m_customAnims.size()));

            for (const auto& jChannel : jAnim["channels"]) {
                if (!jChannel.contains("target") || !jChannel["target"].contains("node")) continue;

                int samplerIdx = jChannel.value("sampler", -1);
                if (samplerIdx < 0 || !jAnim.contains("samplers")) continue;

                const auto& jSampler = jAnim["samplers"][samplerIdx];
                int targetNodeIdx = jChannel["target"]["node"];
                std::string path = jChannel["target"]["path"];
                
                // Ensure the node name exactly matches the fallback we generated in MeshRenderer
                std::string nodeName = gltf["nodes"][targetNodeIdx].value("name", "node_" + std::to_string(targetNodeIdx));

                CustomAnimationChannel channel;
                channel.targetNodeIdx = targetNodeIdx;
                channel.path = path;

                // 1. Parse Keyframe Timestamps (Input)
                int inputAccessorIdx = jSampler.value("input", -1);
                if (inputAccessorIdx < 0) continue;
                    
                const auto& inputAccessor = gltf["accessors"][inputAccessorIdx];
                int bufferViewIdx = inputAccessor.value("bufferView", -1);
                if (bufferViewIdx < 0) continue;

                const auto& inputView = gltf["bufferViews"][bufferViewIdx];
                size_t inputOffset = inputView.value("byteOffset", 0) + inputAccessor.value("byteOffset", 0);
                    
                // Respect byteStride
                int inputStride = inputView.value("byteStride", 4); // Default to sizeof(float)
                if (inputStride == 0) inputStride = 4;

                int count = inputAccessor.value("count", 0);
                if (count == 0 || inputOffset + (count - 1) * inputStride + 4 > bin.size()) continue;

                channel.timelines.resize(count);
                for(int i = 0; i < count; ++i) {
                    std::memcpy(&channel.timelines[i], bin.data() + inputOffset + (i * inputStride), 4);
                }

                if (count > 0 && channel.timelines.back() > animData.duration) {
                    animData.duration = channel.timelines.back();
                }

                // 2. Parse Transform Values (Output)
                int outputAccessorIdx = jSampler.value("output", -1);
                if (outputAccessorIdx < 0) continue;

                const auto& outputAccessor = gltf["accessors"][outputAccessorIdx];
                int outBufferViewIdx = outputAccessor.value("bufferView", -1);
                if (outBufferViewIdx < 0) continue;

                const auto& outputView = gltf["bufferViews"][outBufferViewIdx];
                size_t outputOffset = outputView.value("byteOffset", 0) + outputAccessor.value("byteOffset", 0);

                // Fetch the data type to prevent memory corruption!
                int compType = outputAccessor.value("componentType", 5126);

                // Respect byteStride for outputs
                int outStride = outputView.value("byteStride", 0);

                if (path == "translation" || path == "scale") {
                    if (outStride == 0) outStride = 12; // 3 floats * 4 bytes
                    if (outputOffset + (count - 1) * outStride + 12 <= bin.size()) {
                        channel.v3Values.resize(count);
                        for(int i = 0; i < count; ++i) {
                            std::memcpy(&channel.v3Values[i], bin.data() + outputOffset + (i * outStride), 12);
                        }
                    }
                } else if (path == "rotation") {
                    channel.qValues.resize(count);
                        
                    if (compType == 5126) { // 32-bit FLOAT
                        if (outStride == 0) outStride = 16; // 4 floats * 4 bytes
                        if (outputOffset + (count - 1) * outStride + 16 <= bin.size()) {
                            for(int i = 0; i < count; ++i) {
                                std::memcpy(&channel.qValues[i], bin.data() + outputOffset + (i * outStride), 16);
                            }
                        }
                    } 
                    else if (compType == 5122) { // 16-bit SHORT
                        if (outStride == 0) outStride = 8; // 4 shorts * 2 bytes
                        if (outputOffset + (count - 1) * outStride + 8 <= bin.size()) {
                            for (int i = 0; i < count; i++) {
                                const int16_t* ptr = reinterpret_cast<const int16_t*>(bin.data() + outputOffset + (i * outStride));
                                channel.qValues[i] = QuaternionNormalize({
                                    std::max(-1.0f, ptr[0] / 32767.0f),
                                    std::max(-1.0f, ptr[1] / 32767.0f),
                                    std::max(-1.0f, ptr[2] / 32767.0f),
                                    std::max(-1.0f, ptr[3] / 32767.0f)
                                });
                            }
                        }
                    }
                    else if (compType == 5120) { // 8-bit BYTE
                        if (outStride == 0) outStride = 4; // 4 bytes * 1 byte
                        if (outputOffset + (count - 1) * outStride + 4 <= bin.size()) {
                            for (int i = 0; i < count; i++) {
                                const int8_t* ptr = reinterpret_cast<const int8_t*>(bin.data() + outputOffset + (i * outStride));
                                channel.qValues[i] = QuaternionNormalize({
                                    std::max(-1.0f, ptr[0] / 127.0f),
                                    std::max(-1.0f, ptr[1] / 127.0f),
                                    std::max(-1.0f, ptr[2] / 127.0f),
                                    std::max(-1.0f, ptr[3] / 127.0f)
                                });
                            }
                        }
                    }
                }

                animData.channels.push_back(channel);
            }
            m_customAnims.push_back(animData);
        }

        if (!m_customAnims.empty()) {
            m_usesCustomAnims = true;
            m_animCount = m_customAnims.size();
        }
    } catch (const std::exception& e) {
        std::cerr << "[Animator3d] Exception parsing custom animations: " << e.what() << std::endl;
    }
}

void Animator3d::LoadAnimationsFromPath(const std::string& path) {
    if (path.empty()) return;

    UnloadAnimations();

    // Standard skeletal loading fallback
    m_animations = LoadModelAnimations(path.c_str(), &m_animCount);
    
    if (m_animCount > 0) {
        animsPath = path;
        currentAnimIndex = 0;
        currentFrame = 0;
        isPlaying = true;
    } else {
        // If Raylib failed, we preserve the path to let Update() perform the custom binary parse once MeshRenderer arrives
        animsPath = path;
        m_animations = nullptr;

        // Try to load custom animations immediately instead of waiting for Update()
        if (owner) {
            auto mesh = owner->GetComponent<MeshRenderer>();
            if (mesh && mesh->isLoaded && mesh->usesCustomHierarchy) {
                LoadCustomAnimationsFromMesh(mesh);
            }
        }
    }
}

void Animator3d::UnloadAnimations() {
    if (m_animations != nullptr) {
        // Explicitly unload animations from memory
        UnloadModelAnimations(m_animations, m_animCount);
        m_animations = nullptr;
        m_animCount = 0;
        isPlaying = false;
    }
}

void Animator3d::Play(int animIndex) {
    if (animIndex >= 0 && animIndex < m_animCount) {
        currentAnimIndex = animIndex;
        currentFrame = 0;
        m_frameTimeAccumulator = 0.0f;
        isPlaying = true;
    }
}

void Animator3d::Pause() {
    isPlaying = false;
}

void Animator3d::Stop() {
    isPlaying = false;
    currentFrame = 0;
    m_frameTimeAccumulator = 0.0f;
}

void Animator3d::Start() {
    // Auto-Link Feature: Resolve UUID from MeshRenderer via AssetRegistry
    if (animsPath.empty() && owner) {
        auto mesh = owner->GetComponent<MeshRenderer>();
        if (mesh && mesh->m_modelUUID != 0) {
            std::string resolvedPath = AssetRegistry::GetPathForUUID(mesh->m_modelUUID).string();
            if (!resolvedPath.empty()) {
                LoadAnimationsFromPath(resolvedPath);
            }
        }
    } 
    // Standard reload on Start
    else if (!animsPath.empty() && m_animations == nullptr) {
        LoadAnimationsFromPath(animsPath);
    }
}

void Animator3d::Update(float deltaTime) {
    if (!owner) return;

    auto meshRenderer = owner->GetComponent<MeshRenderer>();
    if (!meshRenderer || !meshRenderer->isLoaded) return;

    // Trigger lazy custom parsing if dealing with a node-hierarchical model
    // MUST BE DONE BEFORE checking isPlaying!
    if (meshRenderer->usesCustomHierarchy && m_animations == nullptr && !m_usesCustomAnims) {
        LoadCustomAnimationsFromMesh(meshRenderer);
    }

    // Now we can exit if the user hasn't pressed play
    if (!isPlaying) return;

    // Path A : Node Animation (Kenney style)
    if (m_usesCustomAnims) {
        if (currentAnimIndex >= m_customAnims.size()) return;
        const auto& activeAnim = m_customAnims[currentAnimIndex];

        m_frameTimeAccumulator += deltaTime * speed;
        if (m_frameTimeAccumulator > activeAnim.duration) {
            if (loop) {
                m_frameTimeAccumulator = std::fmod(m_frameTimeAccumulator, activeAnim.duration);
            } else {
                m_frameTimeAccumulator = activeAnim.duration;
                isPlaying = false;
            }
        }

        // Process every animation channel tracking a node segment
        for (const auto& channel : activeAnim.channels) {
            if (channel.timelines.empty()) continue;

            // Find current keyframe window
            size_t keyIdx = 0;
            while (keyIdx < channel.timelines.size() - 1 && channel.timelines[keyIdx + 1] < m_frameTimeAccumulator) {
                keyIdx++;
            }

            size_t nextKeyIdx = (keyIdx + 1) < channel.timelines.size() ? (keyIdx + 1) : keyIdx;
            float t0 = channel.timelines[keyIdx];
            float t1 = channel.timelines[nextKeyIdx];
            
            // Clamp Alpha to prevent NaN and Infinite extrapolation!
            float alpha = (t0 >= t1) ? 0.0f : std::clamp((m_frameTimeAccumulator - t0) / (t1 - t0), 0.0f, 1.0f);

            ModelNode* targetNode = FindNodeInTree(meshRenderer->rootNode, channel.targetNodeIdx);
            if (!targetNode) continue;

            // Interpolate and mutate values directly inside the Mesh hierarchy tree
            if (channel.path == "translation") {
                targetNode->translation = Vector3Lerp(channel.v3Values[keyIdx], channel.v3Values[nextKeyIdx], alpha);
            } else if (channel.path == "scale") {
                targetNode->scale = Vector3Lerp(channel.v3Values[keyIdx], channel.v3Values[nextKeyIdx], alpha);
            } else if (channel.path == "rotation") {
                // Force Normalize before and after Slerp to ensure valid Matrix creation
                Quaternion q0 = QuaternionNormalize(channel.qValues[keyIdx]);
                Quaternion q1 = QuaternionNormalize(channel.qValues[nextKeyIdx]);
                targetNode->rotation = QuaternionNormalize(QuaternionSlerp(q0, q1, alpha));
            }
        }
        currentFrame = static_cast<int>(m_frameTimeAccumulator * 30.0f); // Simulated frame counter for UI
    } 
    // Path B : Skeletal Animation
    else if (m_animations != nullptr) {
        ModelAnimation activeAnim = m_animations[currentAnimIndex];
        m_frameTimeAccumulator += deltaTime * speed;
        float targetFrameTime = 1.0f / 30.0f; 

        while (m_frameTimeAccumulator >= targetFrameTime) {
            m_frameTimeAccumulator -= targetFrameTime;
            currentFrame++;
            if (currentFrame >= activeAnim.frameCount) {
                if (loop) currentFrame = 0;
                else { currentFrame = activeAnim.frameCount - 1; isPlaying = false; break; }
            }
        }
        UpdateModelAnimation(meshRenderer->model, activeAnim, currentFrame);
    }
}

#ifndef STANDALONE_MODE
void Animator3d::OnInspector() {
    // Ensure Inspector triggers lazy loading immediately if available
    if (owner && m_animCount == 0 && !animsPath.empty()) {
        auto mesh = owner->GetComponent<MeshRenderer>();
        if (mesh && mesh->isLoaded && mesh->usesCustomHierarchy && !m_usesCustomAnims) {
            LoadCustomAnimationsFromMesh(mesh);
        }
    }

    // Check for sibling MeshRenderer and resolve its UUID to offer manual auto-link in the UI
    if (owner && animsPath.empty()) {
        auto mesh = owner->GetComponent<MeshRenderer>();
        if (mesh && mesh->m_modelUUID != 0) {
            std::string resolvedPath = AssetRegistry::GetPathForUUID(mesh->m_modelUUID).string();
            
            if (!resolvedPath.empty()) {
                ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "MeshRenderer detected!");
                if (ImGui::Button("Auto-Link from MeshRenderer", ImVec2(-1, 0))) {
                    nlohmann::json before = Serialize();
                    LoadAnimationsFromPath(resolvedPath);
                    CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, before, Serialize()));
                }
                ImGui::Spacing();
            }
        }
    }

    static char pathBuffer[256] = "";
    if (std::string(pathBuffer) != animsPath && !ImGui::IsItemActive()) {
        strncpy(pathBuffer, animsPath.c_str(), sizeof(pathBuffer));
        pathBuffer[sizeof(pathBuffer) - 1] = '\0';
    }

    ImGui::Text("Animation File (.glb, .iqm):");
    if (ImGui::InputText("##animspath", pathBuffer, sizeof(pathBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
        std::string newPath = pathBuffer;
        if (newPath != animsPath) {
            nlohmann::json before = Serialize();
            LoadAnimationsFromPath(newPath);
            CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, before, Serialize()));
        }
    }

    // Drag and Drop support for Animation files
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
            
            std::string droppedPath = (const char*)payload->Data;
            std::filesystem::path fsPath(droppedPath);
            
            std::string ext = fsPath.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            
            if (ext == ".glb" || ext == ".gltf" || ext == ".iqm") {
                nlohmann::json initialState = Serialize();
                LoadAnimationsFromPath(droppedPath);
                CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, initialState, Serialize()));
            }
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::TextDisabled("Press ENTER or Drag & Drop a file");
    ImGui::Separator();

    if (m_animCount > 0) {
        ImGui::Text("Total Animations: %d", m_animCount);
        
        std::string previewName = "Unknown";
        if (m_usesCustomAnims && currentAnimIndex < m_customAnims.size()) {
            previewName = m_customAnims[currentAnimIndex].name;
        } else if (m_animations != nullptr && currentAnimIndex < m_animCount) {
            previewName = m_animations[currentAnimIndex].name;
            // Fallback if the exporter left the name empty
            if (previewName.empty()) previewName = "Animation_" + std::to_string(currentAnimIndex);
        }

        if (ImGui::BeginCombo("Current Anim", previewName.c_str())) {
            for (int i = 0; i < m_animCount; i++) {
                std::string itemName = "Animation_" + std::to_string(i);
                
                if (m_usesCustomAnims && i < m_customAnims.size()) {
                    itemName = m_customAnims[i].name;
                } else if (m_animations != nullptr) {
                    itemName = m_animations[i].name;
                    if (itemName.empty()) itemName = "Animation_" + std::to_string(i);
                }

                bool isSelected = (currentAnimIndex == i);
                if (ImGui::Selectable(itemName.c_str(), isSelected)) {
                    if (currentAnimIndex != i) {
                        nlohmann::json before = Serialize();
                        currentAnimIndex = i;
                        Play(currentAnimIndex); // Auto-play the new selection
                        CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, before, Serialize()));
                    }
                }
                
                // Keep focus on the selected item when opening the dropdown
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        
        // Custom hierarchy shows time, skeletal shows frames
        if (m_usesCustomAnims) {
            ImGui::Text("Time: %.2f / %.2f s", m_frameTimeAccumulator, m_customAnims[currentAnimIndex].duration);
        } else {
            ImGui::Text("Current Frame: %d / %d", currentFrame, m_animations[currentAnimIndex].frameCount);
        }
        
        ImGui::DragFloat("Playback Speed", &speed, 0.05f, 0.0f, 10.0f);
        ImGui::Checkbox("Looping", &loop);

        if (isPlaying) {
            if (ImGui::Button("Pause")) Pause();
        } else {
            if (ImGui::Button("Play")) Play(currentAnimIndex);
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop")) Stop();
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "No animations loaded.");
    }
}
#endif

nlohmann::json Animator3d::Serialize() const {
    return {
        { "type", "Animator3d" },
        { "animsPath", animsPath },
        { "currentAnimIndex", currentAnimIndex },
        { "speed", speed },
        { "loop", loop }
    };
}

void Animator3d::Deserialize(const nlohmann::json& j) {
    speed = j.value("speed", 1.0f);
    loop = j.value("loop", true);
    currentAnimIndex = j.value("currentAnimIndex", 0);
    
    std::string path = j.value("animsPath", "");
    if (!path.empty()) {
        LoadAnimationsFromPath(path);
        currentAnimIndex = j.value("currentAnimIndex", 0); // Re-apply index after load
    }
}
