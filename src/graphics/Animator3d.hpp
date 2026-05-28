#pragma once

#include "world/Component.hpp"
#include "world/GameObject.hpp"
#include <raylib.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

class MeshRenderer; // Forward declaration

// Structure to cache a single translation, rotation, or scale track for a specific object piece
struct CustomAnimationChannel {
    int targetNodeIdx = -1;
    std::string path; // "translation", "rotation", or "scale"
    std::vector<float> timelines;
    std::vector<Vector3> v3Values;
    std::vector<Quaternion> qValues;
};

// Structure holding a complete custom hierarchy animation
struct CustomAnimationData {
    std::string name;
    float duration = 0.0f;
    std::vector<CustomAnimationChannel> channels;
};

// Component responsible for loading and playing skeletal 3D animations (.glb, .iqm)
// It mutates the Model data inside the companion MeshRenderer component.
class Animator3d : public Component {
    public:
        std::string animsPath = "";
        int currentAnimIndex = 0;
        int currentFrame = 0;
        float speed = 1.0f;
        bool loop = true;
        bool isPlaying = false;

        Animator3d();
        ~Animator3d();

        std::string GetName() const override;

        // Loads animations from a file path (can be the same .glb file as the mesh)
        void LoadAnimationsFromPath(const std::string& path);
        void UnloadAnimations();

        // Playback controls
        void Play(int animIndex);
        void Pause();
        void Stop();

        // System overrides
        void Start() override;
        void Update(float deltaTime) override;

#ifndef STANDALONE_MODE
        void OnInspector() override;
#endif

        nlohmann::json Serialize() const override;
        void Deserialize(const nlohmann::json& j) override;

    private:
        // Raylib standard skeletal structures
        ModelAnimation* m_animations = nullptr;
        int m_animCount = 0;
        float m_frameTimeAccumulator = 0.0f;

        // Custom hierarchy data tracks
        std::vector<CustomAnimationData> m_customAnims;
        bool m_usesCustomAnims = false;

        void LoadCustomAnimationsFromMesh(MeshRenderer* mesh);
};
