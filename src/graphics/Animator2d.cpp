#include "Animator2d.hpp"
#include "scene/GameObject.hpp"
#include "SpriteSheetRenderer.hpp"
#include "core/Engine.hpp"
#include <iostream>
#include <sstream>

#ifndef STANDALONE_MODE
#include <rlImGui.h>
#include <editor/commands/CommandHistory.hpp>
#include <editor/commands/ModifyComponentCommand.hpp>
#include <extras/IconsFontAwesome6.h>
#endif

Animator2d::Animator2d() 
    : m_currentFrameIndex(0), 
      m_timer(0.0f), 
      m_currentAnimation(""), 
      m_playbackDirection(1),
      m_playbackSpeed(1.0f),
      m_playState(PlayState::Stopped),
      m_spriteRenderer(nullptr) 
{
}

void Animator2d::AddAnimation(const std::string& name, const std::vector<int>& frames, float frameDuration, LoopMode loopMode, bool flipX, bool flipY, const std::unordered_map<int, std::vector<std::string>>& events) {
    m_animations[name] = { frames, frameDuration, loopMode, flipX, flipY, events };
}

void Animator2d::Play(const std::string& name) {
    // If it's already the current animation and it's paused, just resume
    if (m_currentAnimation == name && m_playState != PlayState::Stopped) {
        Resume();
        return;
    }

    // Check if the animation exists in our map
    if (m_animations.find(name) != m_animations.end()) {
        m_currentAnimation = name;
        m_currentFrameIndex = 0;
        m_timer = 0.0f;
        m_playbackDirection = 1; // Always reset direction when playing a new animation
        m_playState = PlayState::Playing;

        // Immediately update the sprite to show the first frame of the new animation
        UpdateSprite();
    } else {
        std::cerr << "[Animator] Warning: Animation '" << name << "' not found!" << std::endl;
    }
}

void Animator2d::Pause() {
    if (m_playState == PlayState::Playing) {
        m_playState = PlayState::Paused;
    }
}

void Animator2d::Resume() {
    if (m_playState == PlayState::Paused && !m_currentAnimation.empty()) {
        m_playState = PlayState::Playing;
    }
}

void Animator2d::Stop() {
    m_playState = PlayState::Stopped;
    m_currentFrameIndex = 0; // Reset to the first frame
    m_timer = 0.0f;
    UpdateSprite();
}

bool Animator2d::IsPlaying() const {
    return m_playState == PlayState::Playing;
}

bool Animator2d::IsFinished() const {
    // If stopped or no animation, we consider it "finished"
    if (m_playState == PlayState::Stopped || m_currentAnimation.empty()) return true;
    
    auto it = m_animations.find(m_currentAnimation);
    if (it != m_animations.end()) {
        const AnimationData& anim = it->second;
        // Looping animations technically never finish
        if (anim.loopMode != LoopMode::Once) return false;
        
        // It's finished if we are on the last frame
        return m_currentFrameIndex >= (int)anim.frames.size() - 1;
    }
    return true;
}

void Animator2d::Update(float deltaTime) {
    // Do nothing if no animation is currently set
    if (m_playState != PlayState::Playing || m_currentAnimation.empty()) {
        return;
    }

    // Lazily fetch the SpriteSheetRenderer if we don't have it yet
    if (!m_spriteRenderer) {
        // We use the owner (GameObject) to find the sibling component
        m_spriteRenderer = owner->GetComponent<SpriteSheetRenderer>();
        
        // If the GameObject doesn't have a SpriteSheetRenderer, we can't animate
        if (!m_spriteRenderer) {
            return; 
        }
    }

    AnimationData& anim = m_animations[m_currentAnimation];
    
    // Safety check: ensure the animation actually has frames
    if (anim.frames.empty()) {
        return;
    }

    m_timer += deltaTime * m_playbackSpeed;

    // If enough time has passed, move to the next frame
    if (m_timer >= anim.frameDuration) {
        // Subtract the duration to keep the remainder for precise timing (smooth animations)
        m_timer -= anim.frameDuration; 

        // Save the previous frame before modifying it
        int previousFrame = m_currentFrameIndex;

        // Move forward or backward based on the current direction
        m_currentFrameIndex += m_playbackDirection;

        // Loop mode logic
        
        // We reached the end of the animation (moving forward)
        if (m_currentFrameIndex >= (int)anim.frames.size()) {
            if (anim.loopMode == LoopMode::Loop) {
                m_currentFrameIndex = 0;
            }
            else if (anim.loopMode == LoopMode::PingPong) {
                m_playbackDirection = -1; // Reverse direction
                // Go to the second-to-last frame (avoiding out of bounds if size is 1)
                m_currentFrameIndex = std::max(0, (int)anim.frames.size() - 2);
            }
            else { // Once
                m_currentFrameIndex = anim.frames.size() - 1;
                m_playState = PlayState::Stopped;
            }
        }
        // We reached the beginning of the animation (moving backward in PingPong)
        else if (m_currentFrameIndex < 0) {
            if (anim.loopMode == LoopMode::PingPong) {
                m_playbackDirection = 1; // Reverse back to forward
                // Go to the second frame
                m_currentFrameIndex = std::min((int)anim.frames.size() - 1, 1);
            }
            else if (anim.loopMode == LoopMode::Loop) {
                // Failsafe (usually handled by moving forward, but good to have)
                m_currentFrameIndex = anim.frames.size() - 1;
            }
            else { // Once
                m_currentFrameIndex = 0;
                m_playState = PlayState::Stopped;
            }
        }

        UpdateSprite();

        // Animation Events Trigger
        // Only trigger the event if we actually landed on a different frame
        if (m_currentFrameIndex != previousFrame) {
            auto eventIt = anim.events.find(m_currentFrameIndex);

            // Did we find events for this specific frame index ?
            if (eventIt != anim.events.end()) {
                for (const std::string& evtName : eventIt->second) {
                    owner->OnAnimationEvent(evtName);
                }
            }
        }
    }
}

void Animator2d::Render() {
    // We only want to force the frame synchronization in the Editor (when the game is stopped).
    // During Play mode, the Update() method naturally handles the animation timing.
    bool isPlaying = true;
    if (Engine::Get()) {
        isPlaying = Engine::Get()->IsPlaying();
    }

    if (!isPlaying && owner) {
        // If an animation is currently selected...
        if (!m_currentAnimation.empty() && m_animations.find(m_currentAnimation) != m_animations.end()) {
            
            if (auto sprite = owner->GetComponent<SpriteSheetRenderer>()) {
                const AnimationData& anim = m_animations[m_currentAnimation];
                
                // Ensure the frame index is valid before applying it
                if (!anim.frames.empty() && m_currentFrameIndex >= 0 && m_currentFrameIndex < anim.frames.size()) {
                    
                    // Instantly sync the visual state so the Editor Game View is perfectly accurate
                    sprite->SetFrame(anim.frames[m_currentFrameIndex]);
                    sprite->flipX = anim.flipX;
                    sprite->flipY = anim.flipY;
                }
            }
        }
    }
}

void Animator2d::UpdateSprite() {
    // Ensure we have a valid renderer and a valid animation playing
    if (m_spriteRenderer && !m_currentAnimation.empty()) {
        AnimationData& anim = m_animations[m_currentAnimation];
        
        if (!anim.frames.empty()) {
            // Apply the actual frame index to the SpriteSheetRenderer
            m_spriteRenderer->SetFrame(anim.frames[m_currentFrameIndex]);
        
            m_spriteRenderer->flipX = anim.flipX;
            m_spriteRenderer->flipY = anim.flipY;
        }
    }
}

// Returns the name of the component for the engine's reflection/UI systems
std::string Animator2d::GetName() const {
    return "Animator 2d";
}

#ifndef STANDALONE_MODE
void Animator2d::OnInspector() {
    // Read-only properties do not need Undo/Redo tracking
    // Display current state
    ImGui::Text("Current Animation: %s", m_currentAnimation.empty() ? "None" : m_currentAnimation.c_str());
    
    // Playback UI Controls
    const char* stateStr = (m_playState == PlayState::Playing) ? "Playing" : 
                           (m_playState == PlayState::Paused) ? "Paused" : "Stopped";
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "State: %s", stateStr);

    ImGui::Spacing();

    // Play / Resume Button
    if (m_playState == PlayState::Playing) {
        ImGui::BeginDisabled();
        ImGui::Button(ICON_FA_PLAY, ImVec2(30, 30));
        ImGui::EndDisabled();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f)); // Green
        if (ImGui::Button(ICON_FA_PLAY, ImVec2(30, 30))) {
            if (m_playState == PlayState::Paused) Resume();
            else if (!m_currentAnimation.empty()) Play(m_currentAnimation);
        }
        ImGui::PopStyleColor();
    }

    ImGui::SameLine();

    // Pause Button
    if (m_playState != PlayState::Playing) ImGui::BeginDisabled();
    if (ImGui::Button(ICON_FA_PAUSE, ImVec2(30, 30))) Pause();
    if (m_playState != PlayState::Playing) ImGui::EndDisabled();

    ImGui::SameLine();

    // Stop Button
    if (m_playState == PlayState::Stopped) ImGui::BeginDisabled();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f)); // Red
    if (ImGui::Button(ICON_FA_STOP, ImVec2(30, 30))) Stop();
    ImGui::PopStyleColor();
    if (m_playState == PlayState::Stopped) ImGui::EndDisabled();

    ImGui::Spacing();
    
    ImGui::Text("Frame Index: %d", m_currentFrameIndex);
    ImGui::Text("Timer: %.3f", m_timer);

    // Debug info to see the Ping-Pong effect
    ImGui::Text("Direction: %s", m_playbackDirection == 1 ? "Forward" : "Backward");

    ImGui::Spacing();

    EditorUI::DragFloat("Playback Speed", &m_playbackSpeed, 0.05f, this, 0.0f, 10.0f);

    ImGui::Separator();

    // Static variables hold the temporary state of the form inputs.
    // We declare them here so the "Edit" button below can populate them!
    static char nameBuffer[64] = "";
    static char framesBuffer[256] = "";
    static float duration = 0.1f;
    static int loopModeIdx = 1; // 0=Once, 1=Loop, 2=PingPong
    static bool flipX = false;
    static bool flipY = false;
    static std::vector<int> pickerFrames; // Holds the visual picker's temporary sequence

    // Buffer to hold temporary events while editing
    static std::unordered_map<int, std::vector<std::string>> formEvents;

    bool openPickerModal = false;

    // Display a list of all registered animations
    if (ImGui::TreeNode("Registered Animations")) {
        // We use an iterator to safely erase elements while looping through the map
        for (auto it = m_animations.begin(); it != m_animations.end(); ) {
            const std::string& animName = it->first;
            bool isCurrent = (m_currentAnimation == animName);
            
            // Push the ID for this specific animation row
            ImGui::PushID(animName.c_str());

            // Create a selectable list item for each animation
            // Clicking it will force the Animator to play this animation
            if (ImGui::Selectable(animName.c_str(), isCurrent, 0, ImVec2(ImGui::GetContentRegionAvail().x - 65, 0))) {
                // Capture the state BEFORE changing the animation
                nlohmann::json initialState = Serialize();
                
                // Apply the change
                Play(animName);
                
                // Push the modification to the Undo/Redo stack immediately
                CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, initialState, Serialize()));

                ImGui::PopID();
                break;
            }
            
            // Show a small tooltip with animation details when hovering
            if (ImGui::IsItemHovered()) {
                const char* loopStr = (it->second.loopMode == LoopMode::Once) ? "Once" : 
                                      (it->second.loopMode == LoopMode::PingPong) ? "PingPong" : "Loop";

                ImGui::SetTooltip("Frames: %zu | Duration: %.2fs | Loop: %s | Flip: %s%s", 
                                  it->second.frames.size(), 
                                  it->second.frameDuration, 
                                  loopStr,
                                  it->second.flipX ? "X " : "",
                                  it->second.flipY ? "Y" : (!it->second.flipX ? "None" : ""));
            }

            // Edit Button
            // Loads the selected animation's data into the form at the bottom
            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_PEN)) {
                strncpy(nameBuffer, animName.c_str(), sizeof(nameBuffer));
                
                // Convert the frame array back to a comma-separated string
                std::string fStr;
                for (size_t i = 0; i < it->second.frames.size(); ++i) {
                    fStr += std::to_string(it->second.frames[i]);
                    if (i < it->second.frames.size() - 1) fStr += ", ";
                }
                strncpy(framesBuffer, fStr.c_str(), sizeof(framesBuffer));
                
                duration = it->second.frameDuration;
                loopModeIdx = static_cast<int>(it->second.loopMode);
                flipX = it->second.flipX;
                flipY = it->second.flipY;
                formEvents = it->second.events;
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Edit this animation");

            // Deletion: Trash icon button at the end of the line
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f)); // Red tint

            if (ImGui::Button(ICON_FA_TRASH)) {
                nlohmann::json initialState = Serialize();

                // If the deleted animation was the one playing, we reset the state
                if (isCurrent) {
                    m_currentAnimation = "";
                    m_currentFrameIndex = 0;
                    m_timer = 0.0f;
                }

                // Erase from map and update iterator
                it = m_animations.erase(it);

                // Commit the change to Undo/Redo history
                CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, initialState, Serialize()));
                
                ImGui::PopStyleColor();
                ImGui::PopID();
                break;
            }
            ImGui::PopStyleColor();

            ImGui::PopID();
            ++it;
        }
        ImGui::TreePop();
    }

    ImGui::Separator();

    // Form to add or edit an animation
    // We use TreeNodeEx to keep it open by default
    if (ImGui::TreeNodeEx("Add / Edit Animation", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputText("Name", nameBuffer, IM_ARRAYSIZE(nameBuffer));
        
        // Tooltip to explain the expected format
        ImGui::InputText("Frames", framesBuffer, IM_ARRAYSIZE(framesBuffer));
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Enter frame indices separated by commas (e.g., 0, 1, 2, 3)");
        }

        // Visual Picker Button
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_IMAGE " Pick")) {
            // Parse the current buffer text into the visual picker's array
            pickerFrames.clear();
            std::stringstream ss(framesBuffer);
            std::string item;
            while (std::getline(ss, item, ',')) {
                try { pickerFrames.push_back(std::stoi(item)); } catch (...) {}
            }
            openPickerModal = true;
        }

        ImGui::DragFloat("Duration", &duration, 0.01f, 0.01f, 5.0f, "%.2f sec");
        
        // Combo box for Loop Mode instead of a Checkbox
        const char* loopModes[] = { "Once", "Loop", "PingPong" };
        ImGui::Combo("Mode", &loopModeIdx, loopModes, IM_ARRAYSIZE(loopModes));

        ImGui::Checkbox("Flip X", &flipX);
        ImGui::SameLine();
        ImGui::Checkbox("Flip Y", &flipY);

        ImGui::Spacing();
        ImGui::SeparatorText("Animation Events");

        static int newEventFrame = 0;
        static char newEventName[64] = "";

        float inputWidth = 150.0f;
        
        ImGui::SetNextItemWidth(inputWidth);
        ImGui::InputInt("Frame##evt", &newEventFrame);

        if (newEventFrame < 0) newEventFrame = 0;

        ImGui::SetNextItemWidth(inputWidth);
        ImGui::InputText("Event Name##evt", newEventName, IM_ARRAYSIZE(newEventName));

        if (ImGui::Button(ICON_FA_PLUS " Add Event")) {
            if (strlen(newEventName) > 0) {
                formEvents[newEventFrame].push_back(std::string(newEventName));
                newEventName[0] = '\0'; // Clear input
            }
        }

        ImGui::Spacing();

        // List existing events in the form
        if (!formEvents.empty()) {
            ImGui::BeginChild("EventsList", ImVec2(0, 100), true);
            for (auto it = formEvents.begin(); it != formEvents.end(); ) {
                int frameIdx = it->first;
                auto& evts = it->second;
                
                ImGui::TextColored(ImVec4(0.9f, 0.45f, 0.1f, 1.0f), "Frame %d:", frameIdx);
                
                for (size_t i = 0; i < evts.size(); ) {
                    ImGui::BulletText("%s", evts[i].c_str());
                    ImGui::SameLine();
                    
                    ImGui::PushID((std::to_string(frameIdx) + "_" + std::to_string(i)).c_str());
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                    if (ImGui::Button(ICON_FA_TRASH)) {
                        evts.erase(evts.begin() + i);
                    } else {
                        ++i;
                    }
                    ImGui::PopStyleColor();
                    ImGui::PopID();
                }
                
                // Cleanup empty keys from the map
                if (evts.empty()) {
                    it = formEvents.erase(it);
                } else {
                    ++it;
                }
            }
            ImGui::EndChild();
        }
        ImGui::Spacing();

        if (ImGui::Button("Save Animation", ImVec2(-1, 0))) {
            std::string animName(nameBuffer);
            
            if (!animName.empty()) {
                std::vector<int> parsedFrames;
                std::stringstream ss(framesBuffer);
                std::string item;

                // Parse the comma-separated string into integers
                while (std::getline(ss, item, ',')) {
                    try {
                        parsedFrames.push_back(std::stoi(item));
                    } catch (...) {
                        std::cerr << "[Animator] Invalid frame skipped: " << item << std::endl;
                    }
                }

                if (!parsedFrames.empty()) {
                    nlohmann::json initialState = Serialize();
                    
                    // AddAnimation overwrites existing keys, making it perfect for editing too!
                    LoopMode selectedMode = static_cast<LoopMode>(loopModeIdx);
                    AddAnimation(animName, parsedFrames, duration, selectedMode, flipX, flipY, formEvents);
                    
                    CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, initialState, Serialize()));

                    // Reset the form buffers for the next entry
                    nameBuffer[0] = '\0';
                    framesBuffer[0] = '\0';
                    duration = 0.1f;
                    loopModeIdx = 1;
                    flipX = false;
                    flipY = false;
                    formEvents.clear();
                } else {
                    std::cerr << "[Animator] Cannot save animation without frames." << std::endl;
                }
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Live Preview
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), ICON_FA_EYE " Live Preview");
        
        std::vector<int> previewFrames;
        std::stringstream ss(framesBuffer);
        std::string item;
        while (std::getline(ss, item, ',')) {
            try { previewFrames.push_back(std::stoi(item)); } catch (...) {}
        }

        if (!previewFrames.empty()) {
            SpriteSheetRenderer* sheet = owner->GetComponent<SpriteSheetRenderer>();
            if (sheet && sheet->GetTexture().id != 0) {
                // Local static variables for the UI animation timer
                static float previewTimer = 0.0f;
                static int previewLocalIndex = 0;
                static int previewDirection = 1;

                // Animate based on ImGui's DeltaTime and the current playback speed
                if (duration > 0.0f) {
                    previewTimer += ImGui::GetIO().DeltaTime * m_playbackSpeed;
                    if (previewTimer >= duration) {
                        previewTimer -= duration;
                        previewLocalIndex += previewDirection;
                        
                        // Loop mode logic simulation for the preview
                        if (previewLocalIndex >= (int)previewFrames.size()) {
                            if (loopModeIdx == 1) previewLocalIndex = 0; // Loop
                            else if (loopModeIdx == 2) { // PingPong
                                previewDirection = -1; 
                                previewLocalIndex = std::max(0, (int)previewFrames.size() - 2); 
                            } 
                            else previewLocalIndex = previewFrames.size() - 1; // Once
                        } else if (previewLocalIndex < 0) {
                            if (loopModeIdx == 2) { // PingPong
                                previewDirection = 1; 
                                previewLocalIndex = std::min((int)previewFrames.size() - 1, 1); 
                            }
                            else previewLocalIndex = 0;
                        }
                    }
                }

                // Safety fallback
                if (previewLocalIndex >= previewFrames.size() || previewLocalIndex < 0) previewLocalIndex = 0;

                int currentFrame = previewFrames[previewLocalIndex];
                Texture2D tex = sheet->GetTexture();
                int cols = sheet->GetColumns();
                int rows = sheet->GetRows();

                if (cols > 0 && rows > 0) {
                    // Calculate UV coordinates
                    int col = currentFrame % cols;
                    int row = currentFrame / cols;

                    float frameWidth = (float)tex.width / cols;
                    float frameHeight = (float)tex.height / rows;

                    // This bypasses the negative width bug in rlImGuiImageRect
                    float u0 = (col * frameWidth) / tex.width;
                    float v0 = (row * frameHeight) / tex.height;
                    float u1 = ((col + 1) * frameWidth) / tex.width;
                    float v1 = ((row + 1) * frameHeight) / tex.height;

                    // Define the source rectangle (negative width/height flips the image natively in Raylib!)
                    Rectangle sourceRec = { 
                        col * frameWidth, 
                        row * frameHeight, 
                        flipX ? -frameWidth : frameWidth, 
                        flipY ? -frameHeight : frameHeight 
                    };

                    // Swap the UV coordinates directly if flipped!
                    // This guarantees the exact same frame is drawn, just mirrored.
                    ImVec2 uv0 = ImVec2(flipX ? u1 : u0, flipY ? v1 : v0);
                    ImVec2 uv1 = ImVec2(flipX ? u0 : u1, flipY ? v0 : v1);

                    // Render setup
                    float previewHeight = 120.0f; // Fixed height for UI consistency
                    float previewWidth = previewHeight * (frameWidth / frameHeight);

                    // Center the image in the panel
                    float availX = ImGui::GetContentRegionAvail().x;
                    ImGui::SetCursorPosX((availX - previewWidth) * 0.5f);

                    // Draw!
                    ImGui::Image((ImTextureID)(uintptr_t)tex.id, ImVec2(previewWidth, previewHeight), uv0, uv1);
                }
            } else {
                ImGui::TextDisabled("Add a valid SpriteSheetRenderer to see the preview.");
            }
        } else {
            ImGui::TextDisabled("Enter frames to preview the animation.");
        }

        ImGui::TreePop();
    }

    if (openPickerModal) {
        ImGui::OpenPopup("AnimatorFramePickerModal");
    }

    // Visual frame picker modal
    if (ImGui::BeginPopupModal("AnimatorFramePickerModal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        SpriteSheetRenderer* sheet = owner->GetComponent<SpriteSheetRenderer>();
        
        if (!sheet || sheet->GetTexture().id == 0) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), ICON_FA_TRIANGLE_EXCLAMATION " No valid SpriteSheetRenderer found!");
            ImGui::Spacing();
            if (ImGui::Button("Close", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
        } else {
            ImGui::Text("Click frames in order to build your sequence.");
            
            // Build the sequence string for display
            std::string seqStr;
            for(size_t i = 0; i < pickerFrames.size(); ++i) {
                seqStr += std::to_string(pickerFrames[i]) + (i < pickerFrames.size() - 1 ? ", " : "");
            }
            ImGui::TextColored(ImVec4(0.9f, 0.45f, 0.1f, 1.0f), "Sequence: %s", seqStr.empty() ? "Empty" : seqStr.c_str());
            
            if (ImGui::Button(ICON_FA_TRASH " Clear Sequence")) {
                pickerFrames.clear();
            }

            ImGui::Separator();
            ImGui::Spacing();

            Texture2D tex = sheet->GetTexture();
            int cols = sheet->GetColumns();
            int rows = sheet->GetRows();

            float zoom = 2.0f; 
            if (tex.width * zoom > 600) zoom = 600.0f / tex.width; 
            if (zoom < 1.0f) zoom = 1.0f;

            ImVec2 imageSize(tex.width * zoom, tex.height * zoom);
            ImVec2 p = ImGui::GetCursorScreenPos();
            
            ImGui::Image((ImTextureID)(uintptr_t)tex.id, imageSize);
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            
            float cellWidth = imageSize.x / cols;
            float cellHeight = imageSize.y / rows;

            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                ImVec2 mousePos = ImGui::GetMousePos();
                int col = (mousePos.x - p.x) / cellWidth;
                int row = (mousePos.y - p.y) / cellHeight;
                
                if (col >= 0 && col < cols && row >= 0 && row < rows) {
                    int clickedFrame = row * cols + col;
                    pickerFrames.push_back(clickedFrame);
                }
            }

            for (int y = 0; y < rows; ++y) {
                for (int x = 0; x < cols; ++x) {
                    int frameIdx = y * cols + x;
                    ImVec2 cellMin(p.x + x * cellWidth, p.y + y * cellHeight);
                    ImVec2 cellMax(cellMin.x + cellWidth, cellMin.y + cellHeight);

                    drawList->AddRect(cellMin, cellMax, IM_COL32(255, 255, 255, 50));

                    for (size_t i = 0; i < pickerFrames.size(); ++i) {
                        if (pickerFrames[i] == frameIdx) {
                            drawList->AddRectFilled(cellMin, cellMax, IM_COL32(230, 115, 25, 120));
                            std::string stepStr = std::to_string(i + 1);
                            ImVec2 textSize = ImGui::CalcTextSize(stepStr.c_str());
                            ImVec2 textPos(
                                cellMin.x + (cellWidth - textSize.x) * 0.5f,
                                cellMin.y + (cellHeight - textSize.y) * 0.5f
                            );
                            drawList->AddText(ImVec2(textPos.x + 1, textPos.y + 1), IM_COL32(0, 0, 0, 255), stepStr.c_str());
                            drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), stepStr.c_str());
                        }
                    }
                }
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Apply", ImVec2(120, 0))) {
            if (pickerFrames.empty()) pickerFrames.push_back(0); 
            
            // Rebuild the text buffer from our visual selection
            std::string res;
            for(size_t i = 0; i < pickerFrames.size(); ++i) {
                res += std::to_string(pickerFrames[i]) + (i < pickerFrames.size() - 1 ? ", " : "");
            }
            strncpy(framesBuffer, res.c_str(), sizeof(framesBuffer));
            
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}
#endif

// Converts the component's data into a JSON object for saving scenes
nlohmann::json Animator2d::Serialize() const {
    nlohmann::json j;

    j["type"] = "Animator2d";
    
    // Save current playback state
    j["currentAnimation"] = m_currentAnimation;
    j["currentFrameIndex"] = m_currentFrameIndex;
    j["timer"] = m_timer;
    j["playbackDirection"] = m_playbackDirection;
    j["playbackSpeed"] = m_playbackSpeed;
    j["playState"] = static_cast<int>(m_playState);

    // Save all registered animations
    nlohmann::json animsJson;
    for (const auto& pair : m_animations) {
        nlohmann::json animData;
        animData["frames"] = pair.second.frames;
        animData["frameDuration"] = pair.second.frameDuration;
        animData["loopMode"] = static_cast<int>(pair.second.loopMode);
        animData["flipX"] = pair.second.flipX;
        animData["flipY"] = pair.second.flipY;

        nlohmann::json eventsJson;
        for (const auto& evtPair : pair.second.events) {
            eventsJson[std::to_string(evtPair.first)] = evtPair.second;
        }
        animData["events"] = eventsJson;
        
        // Use the animation name as the key in the JSON object
        animsJson[pair.first] = animData;
    }
    
    j["animations"] = animsJson;

    return j;
}

// Restores the component's data from a JSON object when loading scenes
void Animator2d::Deserialize(const nlohmann::json& j) {
    // Safely load the playback state
    if (j.contains("currentAnimation")) m_currentAnimation = j["currentAnimation"].get<std::string>();
    if (j.contains("currentFrameIndex")) m_currentFrameIndex = j["currentFrameIndex"].get<int>();
    if (j.contains("timer")) m_timer = j["timer"].get<float>();
    if (j.contains("playbackDirection")) m_playbackDirection = j["playbackDirection"].get<int>();
    
    m_playbackSpeed = j.value("playbackSpeed", 1.0f);
    m_playState = static_cast<PlayState>(j.value("playState", 2));

    // Safely load all registered animations
    if (j.contains("animations")) {
        m_animations.clear();
        
        // Iterate through the JSON object containing the animations
        for (auto it = j["animations"].begin(); it != j["animations"].end(); ++it) {
            AnimationData data;
            data.frames = it.value()["frames"].get<std::vector<int>>();
            data.frameDuration = it.value()["frameDuration"].get<float>();
            
            // Backward Compatibility
            // If the old scene file has a 'loop' boolean, convert it smoothly
            if (it.value().contains("loopMode")) {
                data.loopMode = static_cast<LoopMode>(it.value()["loopMode"].get<int>());
            } else if (it.value().contains("loop")) {
                bool oldLoop = it.value()["loop"].get<bool>();
                data.loopMode = oldLoop ? LoopMode::Loop : LoopMode::Once;
            } else {
                data.loopMode = LoopMode::Loop; // Default fallback
            }

            data.flipX = it.value().value("flipX", false);
            data.flipY = it.value().value("flipY", false);

            if (it.value().contains("events")) {
                for (auto evtIt = it.value()["events"].begin(); evtIt != it.value()["events"].end(); ++evtIt) {
                    try {
                        int frameIdx = std::stoi(evtIt.key());
                        data.events[frameIdx] = evtIt.value().get<std::vector<std::string>>();
                    } catch (const std::exception& e) {
                        std::cerr << "[Animator2d] Failed to parse event frame index: " << evtIt.key() << std::endl;
                    }
                }
            }

            // Re-insert into our unordered_map using the JSON key as the animation name
            m_animations[it.key()] = data;
        }
    }
}
