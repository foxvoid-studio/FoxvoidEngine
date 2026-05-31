#include "TextRenderer.hpp"
#include "math/Transform2d.hpp"
#include "scene/GameObject.hpp"
#include "core/AssetRegistry.hpp"
#include <iostream>
#include <filesystem>
#include "RectTransform.hpp"
#include <core/AssetManager.hpp>

#ifndef STANDALONE_MODE
#include "editor/EditorUI.hpp"
#include "editor/commands/CommandHistory.hpp"
#include "editor/commands/ModifyComponentCommand.hpp"
#include <imgui.h>
#endif

TextRenderer::TextRenderer()
    : text("New Text"), fontSize(20.0f), spacing(1.0f), color(BLACK), isHUD(true),
      m_isFontLoaded(false), m_customFont{0} {}

// Converts a path from UI/Drag&Drop into a UUID, then loads it
void TextRenderer::SetFontPath(const std::string& path) {
    if (path.empty()) {
        SetFontPath(UUID(0));
        return;
    }
    
    // Resolve UI path to UUID
    UUID assetId = AssetRegistry::GetUUIDForPath(path);
    SetFontPath(assetId);
}

// The core loading logic using UUID
void TextRenderer::SetFontPath(UUID uuid) {
    m_fontUUID = uuid;

    if (m_fontUUID != 0) {
        std::string resolvedPath = AssetRegistry::GetPathForUUID(m_fontUUID).string();
        
        if (!resolvedPath.empty() && std::filesystem::exists(resolvedPath)) {
            // AssetManager handles the loading and caching automatically
            m_customFont = AssetManager::GetFont(resolvedPath);
            m_isFontLoaded = true;
        } else {
            std::cerr << "[TextRenderer] Error: Could not resolve UUID " << (uint64_t)m_fontUUID << std::endl;
            m_isFontLoaded = false;
        }
    } else {
        m_isFontLoaded = false;
    }
}

std::string TextRenderer::GetFontPath() const {
    return m_fontUUID != 0 ? AssetRegistry::GetPathForUUID(m_fontUUID).string() : "";
}

void TextRenderer::Render() {
    // If it's a HUD element, we don't draw it in the world pass
    if (isHUD || !owner) return;

    auto transform = owner->GetComponent<Transform2d>();
    if (!transform) return;

    // Use custom font if loaded, else fallback to Raylib's default
    Font f = m_isFontLoaded ? m_customFont : GetFontDefault();
    DrawTextEx(f, text.c_str(), transform->GetGlobalPosition(), fontSize, spacing, color);
}

void TextRenderer::RenderHUD() {
    // If it's a world element, we don't draw it in the HUD pass
    if (!isHUD || !owner) return;

    Font f = m_isFontLoaded ? m_customFont : GetFontDefault();

    // UI Elements should prefer RectTransform
    if (RectTransform* rectTransform = owner->GetComponent<RectTransform>()) {
        
        // Get the absolute screen coordinates
        Rectangle rec = rectTransform->GetScreenRect();
        
        // The text starts drawing exactly at the top-left corner of the calculated RectTransform
        Vector2 drawPos = { rec.x, rec.y };
        
        DrawTextEx(f, text.c_str(), drawPos, fontSize, spacing, color);
        return; // Stop here!
    }

    // Fallback: Legacy Transform2d support
    Transform2d* transform = owner->GetComponent<Transform2d>();
    if (transform != nullptr) {
        DrawTextEx(f, text.c_str(), transform->GetGlobalPosition(), fontSize, spacing, color);
    }
}

std::string TextRenderer::GetName() const {
    return "Text Renderer";
}

#ifndef STANDALONE_MODE
void TextRenderer::OnInspector() {
    // Text Input with Undo/Redo support
    char buffer[512];
    strncpy(buffer, text.c_str(), sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';

    static nlohmann::json initialState;

    // We use a multi-line input to allow for paragraphs or line breaks (\n)
    ImGui::InputTextMultiline("Text", buffer, sizeof(buffer), ImVec2(-1.0f, ImGui::GetTextLineHeight() * 4));

    if (ImGui::IsItemActivated()) {
        initialState = Serialize();
    }
    
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        text = buffer;
        CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, initialState, Serialize()));
    }

    ImGui::Separator();

    // Dynamically fetch the current path from the registry for the UI
    std::string currentPath = m_fontUUID != 0 ? AssetRegistry::GetPathForUUID(m_fontUUID).string() : "";

    // Font path input & Drag-Drop
    char pathBuffer[256];
    strncpy(pathBuffer, currentPath.c_str(), sizeof(pathBuffer));
    pathBuffer[sizeof(pathBuffer) - 1] = '\0';

    if (ImGui::InputText("Font Path", pathBuffer, sizeof(pathBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
        std::string newPath(pathBuffer);
        if (newPath != currentPath) {
            nlohmann::json stateBefore = Serialize();
            SetFontPath(newPath); // Will translate path to UUID
            CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, stateBefore, Serialize()));
        }
    }

    // Drag & Drop for .ttf files from the Content Browser
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
            std::string droppedPath = (const char*)payload->Data;
            std::filesystem::path fsPath(droppedPath);
            
            if (fsPath.extension() == ".ttf" || fsPath.extension() == ".otf") {
                nlohmann::json stateBeforeDrop = Serialize();
                SetFontPath(droppedPath); // Will translate path to UUID
                CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, stateBeforeDrop, Serialize()));
            }
        }
        ImGui::EndDragDropTarget();
    }

    if (m_fontUUID != 0) {
        ImGui::TextDisabled("UUID: %llu", (uint64_t)m_fontUUID);
    }
    ImGui::TextDisabled(m_isFontLoaded ? "Status: Custom Font Loaded" : "Status: Using Default Font");

    ImGui::Separator();

    // Styling settings using our custom EditorUI
    EditorUI::DragFloat("Font Size", &fontSize, 0.5f, this, 1.0f, 200.0f);
    EditorUI::DragFloat("Letter Spacing", &spacing, 0.1f, this, 0.0f, 20.0f);
    EditorUI::ColorEdit4("Color", &color, this);
    
    ImGui::Separator();
    
    // Rendering Mode
    EditorUI::Checkbox("Is HUD (Screen Space)", &isHUD, this);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("If checked, the text sticks to the screen. If unchecked, it exists in the world.");
    }
}
#endif

nlohmann::json TextRenderer::Serialize() const {
    return {
        {"type", "TextRenderer"},
        {"text", text},
        {"fontUUID", (uint64_t)m_fontUUID}, // Save the UUID instead of the path
        {"fontSize", fontSize},
        {"spacing", spacing},
        {"isHUD", isHUD},
        {"color_r", color.r},
        {"color_g", color.g},
        {"color_b", color.b},
        {"color_a", color.a}
    };
}

void TextRenderer::Deserialize(const nlohmann::json& j) {
    text = j.value("text", "New Text");
    fontSize = j.value("fontSize", 20.0f);
    spacing = j.value("spacing", 1.0f);
    isHUD = j.value("isHUD", true);

    color.r = j.value("color_r", 0);
    color.g = j.value("color_g", 0);
    color.b = j.value("color_b", 0);
    color.a = j.value("color_a", 255);

    // Backward compatibility & UUID loading
    if (j.contains("fontUUID")) {
        SetFontPath(UUID(j["fontUUID"].get<uint64_t>()));
    } 
    else if (j.contains("fontPath")) {
        SetFontPath(j["fontPath"].get<std::string>());
    }
}
