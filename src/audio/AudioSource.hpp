#pragma once

#include <raylib.h>
#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>

#include "scene/Component.hpp"
#include "core/utils/UUID.hpp"

class AudioSource : public Component {
    public:
        // Toggle to auto-play music when the scene starts
        bool playOnStart;

        AudioSource();
        ~AudioSource() override;

        void Start() override;
        void Update(float deltaTime) override;

        std::string GetName() const override;

#ifndef STANDALONE_MODE
        void OnInspector() override;
#endif

        nlohmann::json Serialize() const override;
        void Deserialize(const nlohmann::json& j) override;

        // SFX API (short sounds)
        void LoadSFX(const std::string& name, const std::string& path);
        void LoadSFX(const std::string& name, UUID uuid);
        void PlaySFX(const std::string& name);

        // Music API (Long streaming tracks)
        void LoadMusic(const std::string& path);
        void LoadMusic(UUID uuid);
        void PlayMusic();
        void StopMusic();
        void SetMusicVolume(float volume);

    private:
        // SFX Storage
        std::unordered_map<std::string, Sound> m_sfx;
        
        // Store UUIDs instead of paths for SFX serialization
        std::unordered_map<std::string, UUID> m_sfxUUIDs; 

        // Music Storage
        Music m_currentMusic;
        
        // Store the UUID instead of the hardcoded path
        UUID m_musicUUID = 0;

        // Buffer to keep the music data alive in memory while it streams from the VFS
        std::vector<unsigned char> m_musicBuffer;
        
        bool m_isMusicLoaded;
        float m_musicVolume;
};
