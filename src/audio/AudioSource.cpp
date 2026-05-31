#include "AudioSource.hpp"

#include "core/AssetRegistry.hpp"

#include <iostream>
#include <filesystem>

#ifndef STANDALONE_MODE
#include "editor/EditorUI.hpp"
#include "editor/commands/CommandHistory.hpp"
#include "editor/commands/ModifyComponentCommand.hpp"
#endif

AudioSource::AudioSource() : m_isMusicLoaded(false), m_musicVolume(1.0f), playOnStart(true) {
    m_currentMusic = {0};
}

AudioSource::~AudioSource() {
    // We must free the GPU/Audio memory when the component is destroyed
    for (auto& pair : m_sfx) {
        UnloadSound(pair.second);
    }
    m_sfx.clear();

    if (m_isMusicLoaded) {
        UnloadMusicStream(m_currentMusic);
    }
}

void AudioSource::Start() {
    if (playOnStart && m_isMusicLoaded) {
        // Automatically start the background music if the toggle is checked
        PlayMusic();
    }
}

void AudioSource::Update(float deltaTime) {
    // Music streams need to be fed buffer data every single frame
    if (m_isMusicLoaded && IsMusicStreamPlaying(m_currentMusic)) {
        UpdateMusicStream(m_currentMusic);
    }
}

// Helper for UI/Scripting to resolve the path
void AudioSource::LoadSFX(const std::string& name, const std::string& path) {
    if (path.empty()) return;
    UUID assetId = AssetRegistry::GetUUIDForPath(path);
    LoadSFX(name, assetId);
}

// Core SFX loading using UUID
void AudioSource::LoadSFX(const std::string& name, UUID uuid) {
    if (uuid == 0) return;

    // If a sound with this name already exists, unload the old one first
    if (m_sfx.find(name) != m_sfx.end()) {
        UnloadSound(m_sfx[name]);
    }

    std::string resolvedPath = AssetRegistry::GetPathForUUID(uuid).string();

    if (!resolvedPath.empty()) {
        Sound newSound = {0};

        // Check if we are reading from the packed VFS or the physical disk
        if (AssetRegistry::IsPacked()) {
            std::vector<unsigned char> fileData = AssetRegistry::GetFileData(resolvedPath);
            if (!fileData.empty()) {
                std::string ext = std::filesystem::path(resolvedPath).extension().string();
                
                // Raylib requires us to parse the memory block into a Wave struct first
                Wave wave = LoadWaveFromMemory(ext.c_str(), fileData.data(), fileData.size());
                
                // Then upload it to the audio device as a Sound
                newSound = LoadSoundFromWave(wave);
                
                // We can safely free the Wave from RAM, the audio device has a copy
                UnloadWave(wave); 
            }
        } else {
            // Standard Editor loading
            newSound = LoadSound(resolvedPath.c_str());
        }

        if (newSound.frameCount > 0) { 
            m_sfx[name] = newSound;
            m_sfxUUIDs[name] = uuid; 
        } else {
            std::cerr << "[AudioSource] Failed to load SFX: " << resolvedPath << std::endl;
        }
    } else {
        std::cerr << "[AudioSource] Error: Could not resolve SFX UUID " << (uint64_t)uuid << std::endl;
    }
}

void AudioSource::PlaySFX(const std::string& name) {
    if (m_sfx.find(name) != m_sfx.end()) {
        // Raylib allows the same sound to be triggered multiple times overlapping
        PlaySound(m_sfx[name]); 
    } else {
        std::cerr << "[AudioSource] SFX not found: " << name << std::endl;
    }
}

// Helper for UI/Scripting to resolve the path
void AudioSource::LoadMusic(const std::string& path) {
    if (path.empty()) {
        LoadMusic(UUID(0));
        return;
    }
    UUID assetId = AssetRegistry::GetUUIDForPath(path);
    LoadMusic(assetId);
}

// Core Music loading using UUID
void AudioSource::LoadMusic(UUID uuid) {
    if (m_isMusicLoaded) {
        StopMusicStream(m_currentMusic); // Good practice to stop before unloading
        UnloadMusicStream(m_currentMusic);
        m_musicBuffer.clear(); // Free the old song's memory
        m_isMusicLoaded = false;
    }

    m_musicUUID = uuid;

    if (m_musicUUID != 0) {
        std::string resolvedPath = AssetRegistry::GetPathForUUID(m_musicUUID).string();

        if (!resolvedPath.empty()) {
            
            // Check if we are reading from the packed VFS or the physical disk
            if (AssetRegistry::IsPacked()) {
                // We store the data in the class member so it stays alive while playing!
                m_musicBuffer = AssetRegistry::GetFileData(resolvedPath);
                
                if (!m_musicBuffer.empty()) {
                    std::string ext = std::filesystem::path(resolvedPath).extension().string();
                    m_currentMusic = LoadMusicStreamFromMemory(ext.c_str(), m_musicBuffer.data(), m_musicBuffer.size());
                }
            } else {
                // Standard Editor loading
                m_currentMusic = LoadMusicStream(resolvedPath.c_str());
            }

            if (m_currentMusic.frameCount > 0) {
                m_isMusicLoaded = true;
                SetMusicVolume(m_musicVolume); 
            } else {
                std::cerr << "[AudioSource] Failed to load Music: " << resolvedPath << std::endl;
                m_musicBuffer.clear(); // Clean up the RAM if the parsing failed
            }
        } else {
            std::cerr << "[AudioSource] Error: Could not resolve Music UUID " << (uint64_t)m_musicUUID << std::endl;
        }
    }
}

void AudioSource::PlayMusic() {
    if (m_isMusicLoaded) {
        PlayMusicStream(m_currentMusic);
    }
}

void AudioSource::StopMusic() {
    if (m_isMusicLoaded) {
        StopMusicStream(m_currentMusic);
    }
}

void AudioSource::SetMusicVolume(float volume) {
    m_musicVolume = volume;
    if (m_isMusicLoaded) {
        ::SetMusicVolume(m_currentMusic, m_musicVolume); // Call Raylib's global function
    }
}

std::string AudioSource::GetName() const {
    return "Audio Source";
}

#ifndef STANDALONE_MODE
void AudioSource::OnInspector() {
    ImGui::TextDisabled("Background Music");
    
    // Dynamically fetch the current path from the registry for the UI
    std::string currentPath = m_musicUUID != 0 ? AssetRegistry::GetPathForUUID(m_musicUUID).string() : "";
    
    char musicBuffer[256];
    strncpy(musicBuffer, currentPath.c_str(), sizeof(musicBuffer));
    musicBuffer[sizeof(musicBuffer) - 1] = '\0';
    
    ImGui::InputText("Music Path", musicBuffer, sizeof(musicBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
    
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        std::string newPath(musicBuffer);
        if (newPath != currentPath) {
            nlohmann::json initialState = Serialize();
            LoadMusic(newPath); // Will translate path to UUID
            CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, initialState, Serialize()));
        }
    }

    // Drag & Drop for Background Music
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
            std::string droppedPath = (const char*)payload->Data;
            std::filesystem::path fsPath(droppedPath);
            std::string ext = fsPath.extension().string();
            
            // Accept standard audio formats
            if (ext == ".ogg" || ext == ".wav" || ext == ".mp3") {
                nlohmann::json initialState = Serialize();
                LoadMusic(droppedPath); // Will translate path to UUID
                CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, initialState, Serialize()));
            }
        }
        ImGui::EndDragDropTarget();
    }

    if (m_musicUUID != 0) {
        ImGui::TextDisabled("UUID: %llu", (uint64_t)m_musicUUID);
    }

    // Play on Start checkbox
    EditorUI::Checkbox("Play on Start", &playOnStart, this);

    // Music playback controls for the Editor
    if (m_isMusicLoaded) {
        if (ImGui::Button("Play BGM")) PlayMusic();
        ImGui::SameLine();
        if (ImGui::Button("Stop BGM")) StopMusic();

        if (EditorUI::DragFloat("Volume", &m_musicVolume, 0.01f, this, 0.0f, 1.0f)) {
            SetMusicVolume(m_musicVolume);
        }
    }

    ImGui::Separator();
    ImGui::TextDisabled("Sound Effects (SFX)");

    // Form to add a new SFX
    static char sfxName[64] = "";
    static char sfxPath[256] = "";

    ImGui::InputText("Name##sfx", sfxName, sizeof(sfxName));
    ImGui::InputText("Path##sfx", sfxPath, sizeof(sfxPath));
    
    // Drag & Drop for SFX
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
            std::string droppedPath = (const char*)payload->Data;
            std::filesystem::path fsPath(droppedPath);
            std::string ext = fsPath.extension().string();
            
            if (ext == ".ogg" || ext == ".wav" || ext == ".mp3") {
                // Fill the path buffer automatically
                strncpy(sfxPath, droppedPath.c_str(), sizeof(sfxPath));
                sfxPath[sizeof(sfxPath) - 1] = '\0';
                
                // Bonus: Auto-fill the name buffer with the filename (without extension) if empty
                if (strlen(sfxName) == 0) {
                    std::string stem = fsPath.stem().string();
                    strncpy(sfxName, stem.c_str(), sizeof(sfxName));
                    sfxName[sizeof(sfxName) - 1] = '\0';
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    if (ImGui::Button("Add SFX", ImVec2(-1, 0))) {
        std::string nameStr(sfxName);
        std::string pathStr(sfxPath);
        if (!nameStr.empty() && !pathStr.empty()) {
            nlohmann::json initialState = Serialize();
            LoadSFX(nameStr, pathStr); // Will translate path to UUID
            CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, initialState, Serialize()));
            
            sfxName[0] = '\0';
            sfxPath[0] = '\0';
        }
    }

    // List loaded SFX
    if (!m_sfxUUIDs.empty()) {
        ImGui::Spacing();
        for (auto it = m_sfxUUIDs.begin(); it != m_sfxUUIDs.end(); ) {
            ImGui::PushID(it->first.c_str());
            ImGui::TextUnformatted(it->first.c_str());
            ImGui::SameLine(ImGui::GetWindowWidth() - 90);
            
            if (ImGui::Button("Play")) {
                PlaySFX(it->first);
            }
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button("X")) {
                nlohmann::json initialState = Serialize();
                
                // Unload from GPU/RAM
                UnloadSound(m_sfx[it->first]);
                m_sfx.erase(it->first);
                it = m_sfxUUIDs.erase(it);
                
                CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, initialState, Serialize()));
                
                ImGui::PopStyleColor();
                ImGui::PopID();
                continue;
            }
            ImGui::PopStyleColor();
            ImGui::PopID();
            ++it;
        }
    }
}
#endif

nlohmann::json AudioSource::Serialize() const {
    nlohmann::json j;
    j["type"] = "AudioSource";
    j["musicUUID"] = (uint64_t)m_musicUUID;
    j["musicVolume"] = m_musicVolume;
    j["playOnStart"] = playOnStart;

    // Manually serialize the unordered_map of UUIDs
    nlohmann::json sfxJson = nlohmann::json::object(); 
    
    for (const auto& pair : m_sfxUUIDs) {
        sfxJson[pair.first] = (uint64_t)pair.second;
    }
    j["sfxUUIDs"] = sfxJson;

    return j;
}

void AudioSource::Deserialize(const nlohmann::json& j) {
    m_musicVolume = j.value("musicVolume", 1.0f);
    playOnStart = j.value("playOnStart", true);
    
    // Backward compatibility for Music
    if (j.contains("musicUUID")) {
        LoadMusic(UUID(j["musicUUID"].get<uint64_t>()));
    } else if (j.contains("musicPath")) {
        LoadMusic(j["musicPath"].get<std::string>());
    }

    // Backward compatibility for SFX
    if (j.contains("sfxUUIDs") && j["sfxUUIDs"].is_object()) {
        auto sfxMap = j["sfxUUIDs"].get<std::unordered_map<std::string, uint64_t>>();
        for (const auto& pair : sfxMap) {
            LoadSFX(pair.first, UUID(pair.second));
        }
    } else if (j.contains("sfxPaths") && j["sfxPaths"].is_object()) {
        auto paths = j["sfxPaths"].get<std::unordered_map<std::string, std::string>>();
        for (const auto& pair : paths) {
            LoadSFX(pair.first, pair.second);
        }
    }
}
