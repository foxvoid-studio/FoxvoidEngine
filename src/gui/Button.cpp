#include "Button.hpp"
#include "physics/Transform2d.hpp"
#include "gui/RectTransform.hpp"
#include "graphics/ShapeRenderer.hpp"
#include "gui/ImageRenderer.hpp"
#include "world/GameObject.hpp"
#include "core/Engine.hpp"
#include "core/Mouse.hpp"
#include "core/AssetRegistry.hpp"
#include <filesystem>

#ifndef STANDALONE_MODE
#include "editor/EditorUI.hpp"
#include "editor/commands/CommandHistory.hpp"
#include "editor/commands/ModifyComponentCommand.hpp"
#endif

Button::Button() 
    : width(100.0f), height(40.0f), isHUD(true), 
      transition(ButtonTransition::ColorTint),
      normalColor(GRAY), hoverColor(LIGHTGRAY), pressedColor(DARKGRAY),
      normalSpriteUUID(0), hoverSpriteUUID(0), pressedSpriteUUID(0),
      m_state(ButtonState::Normal), m_lastState(ButtonState::Pressed),
      m_wasClicked(false) {}

void Button::Update(float deltaTime) {
    if (!owner) return;
    
    m_wasClicked = false;
    Rectangle bounds = {0.0f, 0.0f, 0.0f, 0.0f};
    bool isScreenSpace = isHUD;

    // 1. Calculate Hitbox based on the available transform component
    if (RectTransform* rectTransform = owner->GetComponent<RectTransform>()) {
        
        // MODERN UI: The hitbox perfectly matches the calculated screen rectangle
        bounds = rectTransform->GetScreenRect();
        isScreenSpace = true; // RectTransform is always Screen Space

    } 
    else if (Transform2d* transform = owner->GetComponent<Transform2d>()) {
        
        // LEGACY WORLD: Calculate bounds from the center of the Transform2d
        float scaledWidth = width * transform->scale.x;
        float scaledHeight = height * transform->scale.y;
        auto position = transform->GetGlobalPosition();
        bounds = {
            position.x - (scaledWidth / 2.0f),
            position.y - (scaledHeight / 2.0f),
            scaledWidth,
            scaledHeight
        };

    } else {
        // Cannot click a button that has no spatial representation
        return; 
    }

    // 2. Get Virtual Mouse Position
    Vector2 mousePos = Mouse::GetPosition();
    
    if (!isScreenSpace && Engine::Get()) {
        Camera2D cam = Engine::Get()->GetActiveScene().GetMainCamera2D(GetScreenWidth(), GetScreenHeight());
        mousePos = GetScreenToWorld2D(mousePos, cam);
    }

    // 3. Check Interactions
    if (CheckCollisionPointRec(mousePos, bounds)) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            m_state = ButtonState::Pressed;
        } else if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            m_state = ButtonState::Hovered;
            m_wasClicked = true; 

            owner->OnGUIClick(owner->name);
        } else {
            m_state = ButtonState::Hovered;
        }
    } else {
        m_state = ButtonState::Normal;
    }

    // 4. Update Visuals ONLY if state changed
    if (m_state != m_lastState) {
        ApplyTransition();
        m_lastState = m_state;
    }
}

void Button::Render() {
    if (Engine::Get() && !Engine::Get()->IsPlaying()) {
        ApplyTransition();
    }
}

void Button::ApplyTransition() {
    if (!owner || transition == ButtonTransition::None) return;

    if (transition == ButtonTransition::ColorTint) {
        Color targetColor = normalColor;
        if (m_state == ButtonState::Hovered) targetColor = hoverColor;
        if (m_state == ButtonState::Pressed) targetColor = pressedColor;

        // Try ImageRenderer first, then ShapeRenderer
        if (auto img = owner->GetComponent<ImageRenderer>()) img->color = targetColor;
        else if (auto shape = owner->GetComponent<ShapeRenderer>()) shape->color = targetColor;
    } 
    else if (transition == ButtonTransition::SpriteSwap) {
        UUID targetSprite = normalSpriteUUID;
        if (m_state == ButtonState::Hovered) targetSprite = hoverSpriteUUID;
        if (m_state == ButtonState::Pressed) targetSprite = pressedSpriteUUID;

        if (auto img = owner->GetComponent<ImageRenderer>()) {
            // We only swap if a valid sprite is assigned, otherwise we leave the current one
            if (targetSprite != 0) img->SetTexture(targetSprite);
        }
    }
}

bool Button::IsClicked() const {
    return m_wasClicked;
}

std::string Button::GetName() const {
    return "Button";
}

#ifndef STANDALONE_MODE
void Button::OnInspector() {
    if (owner->GetComponent<RectTransform>()) {
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Hitbox driven by RectTransform");
    } else {
        EditorUI::DragFloat("Hitbox Width", &width, 1.0f, this, 0.0f, 2000.0f);
        EditorUI::DragFloat("Hitbox Height", &height, 1.0f, this, 0.0f, 2000.0f);
        EditorUI::Checkbox("Is HUD", &isHUD, this);
    }
    
    ImGui::Separator();

    // Transition Mode Selection
    int modeIdx = static_cast<int>(transition);
    const char* modes[] = { "None", "Color Tint", "Sprite Swap" };
    if (ImGui::Combo("Transition", &modeIdx, modes, IM_ARRAYSIZE(modes))) {
        nlohmann::json initialState = Serialize();
        transition = static_cast<ButtonTransition>(modeIdx);
        CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, initialState, Serialize()));
        m_lastState = (ButtonState)-1; // Force visual update
    }

    ImGui::Spacing();

    if (transition == ButtonTransition::ColorTint) {
        EditorUI::ColorEdit4("Normal Color", &normalColor, this);
        EditorUI::ColorEdit4("Hover Color", &hoverColor, this);
        EditorUI::ColorEdit4("Pressed Color", &pressedColor, this);
    } 
    else if (transition == ButtonTransition::SpriteSwap) {
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
                        m_lastState = (ButtonState)-1; // Force visual update
                    }
                }
                ImGui::EndDragDropTarget();
            }
            ImGui::SameLine();
            ImGui::Text("%s", label);
        };

        ImGui::Text("Drag & Drop textures from Content Browser:");
        TextureDropField("Normal", normalSpriteUUID);
        TextureDropField("Hover", hoverSpriteUUID);
        TextureDropField("Pressed", pressedSpriteUUID);
    }

    ImGui::Separator();
    ImGui::TextDisabled(m_state == ButtonState::Normal ? "State: Normal" : 
                       (m_state == ButtonState::Hovered ? "State: Hovered" : "State: Pressed"));
}
#endif

nlohmann::json Button::Serialize() const {
    return {
        {"type", "Button"},
        {"width", width}, {"height", height}, {"isHUD", isHUD},
        {"transition", static_cast<int>(transition)},
        {"n_r", normalColor.r}, {"n_g", normalColor.g}, {"n_b", normalColor.b}, {"n_a", normalColor.a},
        {"h_r", hoverColor.r}, {"h_g", hoverColor.g}, {"h_b", hoverColor.b}, {"h_a", hoverColor.a},
        {"p_r", pressedColor.r}, {"p_g", pressedColor.g}, {"p_b", pressedColor.b}, {"p_a", pressedColor.a},
        {"n_spr", (uint64_t)normalSpriteUUID},
        {"h_spr", (uint64_t)hoverSpriteUUID},
        {"p_spr", (uint64_t)pressedSpriteUUID}
    };
}

void Button::Deserialize(const nlohmann::json& j) {
    width = j.value("width", 100.0f);
    height = j.value("height", 40.0f);
    isHUD = j.value("isHUD", true);

    transition = static_cast<ButtonTransition>(j.value("transition", 1));
    
    // Explicitly cast the integers returned by JSON to unsigned char
    // to prevent C++ narrowing conversion errors
    normalColor = { 
        static_cast<unsigned char>(j.value("n_r", 130)), 
        static_cast<unsigned char>(j.value("n_g", 130)), 
        static_cast<unsigned char>(j.value("n_b", 130)), 
        static_cast<unsigned char>(j.value("n_a", 255)) 
    };
    
    hoverColor = { 
        static_cast<unsigned char>(j.value("h_r", 200)), 
        static_cast<unsigned char>(j.value("h_g", 200)), 
        static_cast<unsigned char>(j.value("h_b", 200)), 
        static_cast<unsigned char>(j.value("h_a", 255)) 
    };
    
    pressedColor = { 
        static_cast<unsigned char>(j.value("p_r", 80)), 
        static_cast<unsigned char>(j.value("p_g", 80)), 
        static_cast<unsigned char>(j.value("p_b", 80)), 
        static_cast<unsigned char>(j.value("p_a", 255)) 
    };

    normalSpriteUUID = j.value("n_spr", 0ULL);
    hoverSpriteUUID = j.value("h_spr", 0ULL);
    pressedSpriteUUID = j.value("p_spr", 0ULL);
}
