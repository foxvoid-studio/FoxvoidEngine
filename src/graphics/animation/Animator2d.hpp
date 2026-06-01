#pragma once

#include "scene/Component.hpp"
#include <string>
#include <unordered_map>
#include <vector>

// Forward declarations to avoid cyclic includes and speed up compilation
class SpriteSheetRenderer;

enum class LoopMode {
    Once = 0,
    Loop = 1,
    PingPong = 2
};

enum class PlayState {
    Playing,
    Paused,
    Stopped
};

// Struct to hold individual animation data
struct AnimationData {
    std::vector<int> frames;
    float frameDuration;
    LoopMode loopMode;
    bool flipX;
    bool flipY;
    std::unordered_map<int, std::vector<std::string>> events;
};

class Animator2d : public Component {
    public:
        Animator2d();
        ~Animator2d() override = default;

        // Called every frame by the Engine/Scene
        void Update(float deltaTime) override;

        void Render() override;

        // Registers a new animation state
        void AddAnimation(const std::string& name, const std::vector<int>& frames, float frameDuration, LoopMode loopMode, bool flipX = false, bool flipY = false, const std::unordered_map<int, std::vector<std::string>>& events = {});

        // Switches to a new animation if it exists and isn't already playing
        void Play(const std::string& name);

        // Playback Controls
        void Pause();
        void Resume();
        void Stop();

        // Status Queries
        bool IsPlaying() const;
        bool IsFinished() const;

        // Speed Accessors
        void SetPlaybackSpeed(float speed) { m_playbackSpeed = speed; }
        float GetPlaybackSpeed() const { return m_playbackSpeed; }

        std::string GetName() const override;

#ifndef STANDALONE_MODE
        void OnInspector() override;
#endif

        nlohmann::json Serialize() const override;
        void Deserialize(const nlohmann::json& j) override;

    private:
        // Updates the visual representation on the SpriteSheetRenderer
        void UpdateSprite();

        std::unordered_map<std::string, AnimationData> m_animations;
        std::string m_currentAnimation;
        
        int m_currentFrameIndex;
        float m_timer;

        // Tracks if we are reading frames forward (1) or backward (-1)
        int m_playbackDirection;

        // Global speed multiplier for this animator
        float m_playbackSpeed;

        PlayState m_playState;
        
        // Cached pointer to the renderer to avoid querying it every frame
        SpriteSheetRenderer* m_spriteRenderer;
};
