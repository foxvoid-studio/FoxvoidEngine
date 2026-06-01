#include "Checkbox.hpp"
#include "ui/RectTransform.hpp"
#include "graphics/renderers/ShapeRenderer.hpp"
#include "ui/ImageRenderer.hpp"
#include "scene/GameObject.hpp"
#include "core/Engine.hpp"
#include "input/Mouse.hpp"
#include "core/AssetRegistry.hpp"
#include <filesystem>

#ifndef STANDALONE_MODE
#include "editor/EditorUI.hpp"
#include "editor/commands/CommandHistory.hpp"
#include "editor/commands/ModifyComponentCommand.hpp"
#endif

Checkbox::Checkbox() 
    : isOn(false), useSprite(false), 
      colorOn(GREEN), colorOff(GRAY), 
      spriteOnUUID(0), spriteOffUUID(0), 
      m_lastState(true) // Set to true initially so the first Update forces visual sync to false
{}

void Checkbox::Update(float deltaTime) {
    if (!owner) return;
    
    Rectangle bounds = {0.0f, 0.0f, 0.0f, 0.0f};

    // We exclusively use the modern UI layout system
    if (RectTransform* rectTransform = owner->GetComponent<RectTransform>()) {
        bounds = rectTransform->GetScreenRect();
    } else {
        return; // Checkbox requires a RectTransform to be clickable
    }

    Vector2 mousePos = Mouse::GetPosition();

    // Check interaction
    if (CheckCollisionPointRec(mousePos, bounds)) {
        // Only trigger on the exact frame the button is pressed down
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            isOn = !isOn; // Toggle state
            
            // Broadcast the global GUI event so Python scripts can react
            owner->OnGUIClick(owner->name);
        }
    }

    // Update visuals if state changed
    if (isOn != m_lastState) {
        ApplyVisuals();
        m_lastState = isOn;
    }
}  

void Checkbox::ApplyVisuals() {
    if (!owner) return;

    if (useSprite) {
        if (auto img = owner->GetComponent<ImageRenderer>()) {
            UUID targetSprite = isOn ? spriteOnUUID : spriteOffUUID;
            if (targetSprite != 0) {
                img->SetTexture(targetSprite);
            }
        }
    } else {
        Color targetColor = isOn ? colorOn : colorOff;
        
        // Try ImageRenderer first (for tinting), then ShapeRenderer
        if (auto img = owner->GetComponent<ImageRenderer>()) {
            img->color = targetColor;
        } else if (auto shape = owner->GetComponent<ShapeRenderer>()) {
            shape->color = targetColor;
        }
    }
}

std::string Checkbox::GetName() const {
    return "Checkbox / Toggle";
}

#ifndef STANDALONE_MODE
void Checkbox::OnInspector() {
    static nlohmann::json initialState;
    auto HandleUndoRedo = [&]() {
        if (ImGui::IsItemActivated()) initialState = Serialize();
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, initialState, Serialize()));
        }
    };

    if (EditorUI::Checkbox("Is On (State)", &isOn, this)) {
        m_lastState = !isOn; // Force visual refresh
    }
    
    ImGui::Separator();

    if (EditorUI::Checkbox("Use Sprites instead of Colors", &useSprite, this)) {
        m_lastState = !isOn; // Force visual refresh
    }

    ImGui::Spacing();

    if (useSprite) {
        // --- Sprite Swap UI ---
        if (!owner->GetComponent<ImageRenderer>()) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Warning: Requires ImageRenderer!");
        }

        auto TextureDropField = [this](const char* label, UUID& targetUUID) {
            std::string currentName = targetUUID == 0 ? "None" : AssetRegistry::GetPathForUUID(targetUUID).filename().string();
            ImGui::Button(currentName.c_str(), ImVec2(-1, 0));
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                    std::string droppedPath = (const char*)payload->Data;
                    std::filesystem::path fsPath(droppedPath);
                    std::string ext = fsPath.extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    
                    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg") {
                        nlohmann::json stateBefore = Serialize();
                        targetUUID = AssetRegistry::GetUUIDForPath(droppedPath);
                        CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, stateBefore, Serialize()));
                        m_lastState = !isOn; // Force visual refresh
                    }
                }
                ImGui::EndDragDropTarget();
            }
            ImGui::SameLine();
            ImGui::Text("%s", label);
        };

        ImGui::Text("Drag & Drop textures from Content Browser:");
        TextureDropField("ON Sprite", spriteOnUUID);
        TextureDropField("OFF Sprite", spriteOffUUID);
    } else {
        // --- Color Tint UI ---
        EditorUI::ColorEdit4("ON Color", &colorOn, this);
        EditorUI::ColorEdit4("OFF Color", &colorOff, this);
    }
}
#endif

nlohmann::json Checkbox::Serialize() const {
    return {
        {"type", "Checkbox"},
        {"isOn", isOn},
        {"useSprite", useSprite},
        {"cOn_r", colorOn.r}, {"cOn_g", colorOn.g}, {"cOn_b", colorOn.b}, {"cOn_a", colorOn.a},
        {"cOff_r", colorOff.r}, {"cOff_g", colorOff.g}, {"cOff_b", colorOff.b}, {"cOff_a", colorOff.a},
        {"sprOn", (uint64_t)spriteOnUUID},
        {"sprOff", (uint64_t)spriteOffUUID}
    };
}

void Checkbox::Deserialize(const nlohmann::json& j) {
    isOn = j.value("isOn", false);
    useSprite = j.value("useSprite", false);

    colorOn = { static_cast<unsigned char>(j.value("cOn_r", 0)), static_cast<unsigned char>(j.value("cOn_g", 255)), static_cast<unsigned char>(j.value("cOn_b", 0)), static_cast<unsigned char>(j.value("cOn_a", 255)) };
    colorOff = { static_cast<unsigned char>(j.value("cOff_r", 130)), static_cast<unsigned char>(j.value("cOff_g", 130)), static_cast<unsigned char>(j.value("cOff_b", 130)), static_cast<unsigned char>(j.value("cOff_a", 255)) };

    spriteOnUUID = j.value("sprOn", 0ULL);
    spriteOffUUID = j.value("sprOff", 0ULL);
    
    // Force a visual update right after loading
    m_lastState = !isOn; 
}
