#pragma once

#include "scene/Component.hpp"
#include <sstream>
#include <vector>

class SpriteSheetRenderer; // Forward declaration

class Animation2d : public Component {
public:
    // Initializes the animation with a frame sequence, speed, and looping option
    Animation2d(const std::vector<int>& frames = {0}, float speed = 0.15f, bool loop = true, bool flipX = false, bool flipY = false);
    ~Animation2d() override = default;

    void Start() override;
    void Update(float deltaTime) override;

    // Render override for editor synchronization
    void Render() override;

    std::string GetName() const override;

#ifndef STANDALONE_MODE
    void OnInspector() override;
#endif

    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

private:
    std::vector<int> m_frames; // The sequence of frames to play
    float m_speed;             // Time in seconds per frame
    bool m_loop;               // Should the animation restart at the end?
    
    // Flip states
    bool m_flipX;
    bool m_flipY;

    int m_currentIndex;        // Current position in the m_frames array
    float m_timer;             // Time accumulator
    
    SpriteSheetRenderer* m_sprite; // Pointer to the renderer we want to control

    // Helpers to easily edit the vector in ImGui text input
    std::string GetFramesAsString() const;
    void ParseFramesFromString(const std::string& str);

    // Temporary backup for the visual picker (Cancel logic)
    std::vector<int> m_pickerBackupFrames;
};
