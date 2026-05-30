#include "Animation2d.hpp"
#include "SpriteSheetRenderer.hpp"
#include "scene/GameObject.hpp"
#include <iostream>
#include <string>
#include <cstring>
#include <core/Engine.hpp>

#ifndef STANDALONE_MODE
#include <editor/commands/CommandHistory.hpp>
#include <editor/commands/ModifyComponentCommand.hpp>
#include <editor/EditorUI.hpp>
#include "extras/IconsFontAwesome6.h"
#endif

Animation2d::Animation2d(const std::vector<int>& frames, float speed, bool loop, bool flipX, bool flipY)
    : m_frames(frames), m_speed(speed), m_loop(loop),
      m_flipX(flipX), m_flipY(flipY),
      m_currentIndex(0), m_timer(0.0f), m_sprite(nullptr) {}

void Animation2d::Start() {
    // Fetch the renderer attached to the same GameObject
    m_sprite = owner->GetComponent<SpriteSheetRenderer>();
    
    if (!m_sprite) {
        std::cerr << "[Animation2d] Warning: No SpriteSheetRenderer found!" << std::endl;
    } else if (!m_frames.empty()) {
        // Apply the very first frame immediately
        m_sprite->SetFrame(m_frames[0]);

        // Apply flip states immediately
        m_sprite->flipX = m_flipX;
        m_sprite->flipY = m_flipY;
    }
}

void Animation2d::Update(float deltaTime) {
    // Do nothing if we lack a sprite or frames
    if (!m_sprite || m_frames.empty()) return;

    // Accumulate the time passed since the last frame
    m_timer += deltaTime;

    // Check if it's time to advance the animation
    if (m_timer >= m_speed) {
        m_timer -= m_speed; // Keep the remainder for perfect timing precision
        m_currentIndex++;

        // Handle the end of the animation sequence
        if (m_currentIndex >= m_frames.size()) {
            if (m_loop) {
                m_currentIndex = 0; // Back to the beginning
            } else {
                m_currentIndex = m_frames.size() - 1; // Stay on the last frame
            }
        }
        
        // Tell the renderer to display the new frame
        m_sprite->SetFrame(m_frames[m_currentIndex]);

        // Ensure flip states are continuously applied
        m_sprite->flipX = m_flipX;
        m_sprite->flipY = m_flipY;
    }
}

void Animation2d::Render() {
    // We only want to force the frame synchronization in the Editor (when the game is stopped).
    // During Play mode, the Update() method naturally handles the animation timing.
    bool isPlaying = true;
    if (Engine::Get()) {
        isPlaying = Engine::Get()->IsPlaying();
    }

    if (!isPlaying && owner) {
        if (auto sprite = owner->GetComponent<SpriteSheetRenderer>()) {
            if (!m_frames.empty()) {
                // Instantly sync the visual state so the Game View is perfectly accurate
                sprite->SetFrame(m_frames[0]);
                sprite->flipX = m_flipX;
                sprite->flipY = m_flipY;
            }
        }
    }
}

std::string Animation2d::GetName() const {
    return "Animation 2D";
}

// Converts {0, 1, 2} to "0, 1, 2"
std::string Animation2d::GetFramesAsString() const {
    std::stringstream ss;
    for (size_t i = 0; i < m_frames.size(); ++i) {
        ss << m_frames[i];
        if (i < m_frames.size() - 1) ss << ", ";
    }
    return ss.str();
}

// Converts "0, 1, 2" to {0, 1, 2}
void Animation2d::ParseFramesFromString(const std::string& str) {
    m_frames.clear();
    std::stringstream ss(str);
    std::string item;
    
    // Split by comma
    while (std::getline(ss, item, ',')) {
        try {
            m_frames.push_back(std::stoi(item)); // Convert text to int
        } catch (...) {
            // Ignore invalid text inputs (like letters) safely
        }
    }
    
    // Fallback: Ensure there is always at least one frame to avoid crashes
    if (m_frames.empty()) {
        m_frames.push_back(0);
    }
    m_currentIndex = 0; // Reset animation to start
}

#ifndef STANDALONE_MODE
void Animation2d::OnInspector() {
    // Use EditorUI for standard properties
    EditorUI::DragFloat("Speed (s)", &m_speed, 0.01f, this, 0.01f, 5.0f);
    EditorUI::Checkbox("Loop", &m_loop, this);

    // UI Checkboxes for flipping (EditorUI handles Undo/Redo natively)
    EditorUI::Checkbox("Flip X", &m_flipX, this);
    ImGui::SameLine();
    EditorUI::Checkbox("Flip Y", &m_flipY, this);

    // Frame sequence editor (Custom UI)
    std::string framesStr = GetFramesAsString();
    char buffer[256];
    strncpy(buffer, framesStr.c_str(), sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';

    // Static variable to track the state before the user starts typing
    static nlohmann::json initialFramesState;

    // We no longer use ImGuiInputTextFlags_EnterReturnsTrue because we rely on the Deactivated state
    ImGui::InputText("Frames", buffer, sizeof(buffer));

    // UI Lifecycle for Text Input
    if (ImGui::IsItemActivated()) {
        // User clicked inside the text box. Save the current state!
        initialFramesState = Serialize();
    }

    if (ImGui::IsItemDeactivatedAfterEdit()) {
        // User pressed Enter or clicked away. 
        // Apply the new text to the component's internal vector
        ParseFramesFromString(buffer);
        
        // Push the modification to the Undo/Redo stack
        CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, initialFramesState, Serialize()));
    }
    // Note: If the user presses Escape, ImGui::IsItemDeactivatedAfterEdit() is false.
    // The next frame, 'buffer' will simply be repopulated with the old valid data from GetFramesAsString(). Magic!

    ImGui::TextDisabled("Ex: 0, 1, 2, 3");
    
    // Read-only debug info
    if (m_frames.size() > 0 && m_currentIndex < m_frames.size()) {
        ImGui::Text("Current Frame: %d", m_frames[m_currentIndex]);
    }

    ImGui::Separator();

    bool openPickerModal = false;
    
    // Visual frame picker
    // Open the visual picker modal
    if (ImGui::Button(ICON_FA_IMAGE " Visual Frame Picker", ImVec2(-1, 30))) {
        m_pickerBackupFrames = m_frames;  // Backup current frames in case the user cancels
        initialFramesState = Serialize(); // Save state for Undo/Redo if they apply
        openPickerModal = true;
    }

    if (openPickerModal) {
        ImGui::OpenPopup("FramePickerModal");
    }

    // The Modal (Centered popup that blocks interaction with the rest of the UI)
    if (ImGui::BeginPopupModal("FramePickerModal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        SpriteSheetRenderer* sheet = owner->GetComponent<SpriteSheetRenderer>();
        
        // Safety check: Ensure a valid spritesheet is attached
        if (!sheet || sheet->GetTexture().id == 0) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), ICON_FA_TRIANGLE_EXCLAMATION " No valid SpriteSheetRenderer found on this GameObject!");
            ImGui::Spacing();
            if (ImGui::Button("Close", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
        } else {
            // Modal Header
            ImGui::Text("Click frames in order to build your sequence.");
            ImGui::TextColored(ImVec4(0.9f, 0.45f, 0.1f, 1.0f), "Sequence: %s", GetFramesAsString().c_str());
            
            if (ImGui::Button(ICON_FA_TRASH " Clear Sequence")) {
                m_frames.clear();
            }

            ImGui::Separator();
            ImGui::Spacing();

            Texture2D tex = sheet->GetTexture();
            int cols = sheet->GetColumns();
            int rows = sheet->GetRows();

            // Dynamic zoom: Enlarge small pixel art textures for better visibility
            float zoom = 2.0f; 
            if (tex.width * zoom > 600) zoom = 600.0f / tex.width; // Constrain width to 600px
            if (zoom < 1.0f) zoom = 1.0f;

            ImVec2 imageSize(tex.width * zoom, tex.height * zoom);
            
            // Save the ImGui cursor position BEFORE drawing the image
            ImVec2 p = ImGui::GetCursorScreenPos();
            
            // 1. Draw the actual spritesheet image (uintptr_t cast prevents C++ warnings)
            ImGui::Image((ImTextureID)(uintptr_t)tex.id, imageSize);

            // Fetch ImGui's low-level drawing API to draw overlays
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            
            float cellWidth = imageSize.x / cols;
            float cellHeight = imageSize.y / rows;

            // 2. Handle mouse clicks on the image
            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                ImVec2 mousePos = ImGui::GetMousePos();
                
                // Calculate which column and row was clicked
                int col = (mousePos.x - p.x) / cellWidth;
                int row = (mousePos.y - p.y) / cellHeight;
                
                // Safety bounds check
                if (col >= 0 && col < cols && row >= 0 && row < rows) {
                    int clickedFrame = row * cols + col;
                    m_frames.push_back(clickedFrame);
                    m_currentIndex = 0; // Reset animation to start for preview
                }
            }

            // 3. Draw the grid and sequence indicators
            for (int y = 0; y < rows; ++y) {
                for (int x = 0; x < cols; ++x) {
                    int frameIdx = y * cols + x;
                    ImVec2 cellMin(p.x + x * cellWidth, p.y + y * cellHeight);
                    ImVec2 cellMax(cellMin.x + cellWidth, cellMin.y + cellHeight);

                    // Draw the border of each frame (semi-transparent grid)
                    drawList->AddRect(cellMin, cellMax, IM_COL32(255, 255, 255, 50));

                    // Check if this frame is part of our current sequence
                    for (size_t i = 0; i < m_frames.size(); ++i) {
                        if (m_frames[i] == frameIdx) {
                            // Fill the selected cell with semi-transparent Foxvoid orange
                            drawList->AddRectFilled(cellMin, cellMax, IM_COL32(230, 115, 25, 120));
                            
                            // Draw the animation step number in the center of the frame
                            std::string stepStr = std::to_string(i + 1);
                            ImVec2 textSize = ImGui::CalcTextSize(stepStr.c_str());
                            ImVec2 textPos(
                                cellMin.x + (cellWidth - textSize.x) * 0.5f,
                                cellMin.y + (cellHeight - textSize.y) * 0.5f
                            );
                            
                            // Draw a black drop-shadow for readability, then the white text
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

        // Modal Footer
        if (ImGui::Button("Apply", ImVec2(120, 0))) {
            if (m_frames.empty()) m_frames.push_back(0); // Safety fallback
            
            // Commit the modification to the Undo/Redo history
            CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, initialFramesState, Serialize()));
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_frames = m_pickerBackupFrames; // Restore the sequence to its state before opening the modal
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}
#endif

nlohmann::json Animation2d::Serialize() const {
    return {
        {"type", "Animation2d"},
        {"frames", m_frames}, // nlohmann::json handles std::vector automatically!
        {"speed", m_speed},
        {"loop", m_loop},
        {"flipX", m_flipX},
        {"flipY", m_flipY}
    };
}

void Animation2d::Deserialize(const nlohmann::json& j) {
    m_speed = j.value("speed", 0.15f);
    m_loop = j.value("loop", true);

    m_flipX = j.value("flipX", false);
    m_flipY = j.value("flipY", false);
    
    if (j.contains("frames") && j["frames"].is_array()) {
        m_frames.clear();
        for (const auto& frame : j["frames"]) {
            m_frames.push_back(frame.get<int>());
        }
    }
    
    // Fallback
    if (m_frames.empty()) {
        m_frames.push_back(0);
    }
    
    m_currentIndex = 0;
    m_timer = 0.0f;
}
