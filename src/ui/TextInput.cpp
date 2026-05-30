#include "TextInput.hpp"
#include "ui/RectTransform.hpp"
#include "scene/GameObject.hpp"
#include "input/Mouse.hpp"
#include "core/AssetManager.hpp"
#include "core/AssetRegistry.hpp"
#include <filesystem>

#ifndef STANDALONE_MODE
#include "editor/EditorUI.hpp"
#include "editor/commands/CommandHistory.hpp"
#include "editor/commands/ModifyComponentCommand.hpp"
#include <imgui.h>
#endif

TextInput::TextInput() 
    : text(""), maxLength(20), fontSize(20), spacing(1.0f),
      textColor(BLACK), bgColor(LIGHTGRAY), focusedBgColor(WHITE),
      m_isFocused(false), m_cursorTimer(0.0f), m_showCursor(false),
      m_fontUUID(0) 
{
    // Fallback to default font initially
    m_font = GetFontDefault();
}

void TextInput::SetFont(const std::string& path) {
    if (path.empty()) {
        SetFont(UUID(0));
        return;
    }
    SetFont(AssetRegistry::GetUUIDForPath(path));
}

void TextInput::SetFont(UUID uuid) {
    m_fontUUID = uuid;
    if (m_fontUUID != 0) {
        std::string resolvedPath = AssetRegistry::GetPathForUUID(m_fontUUID).string();
        if (!resolvedPath.empty()) {
            m_font = AssetManager::GetFont(resolvedPath);
        } else {
            m_font = GetFontDefault(); // Fallback if file is missing
        }
    } else {
        m_font = GetFontDefault();
    }
}

void TextInput::Update(float deltaTime) {
    if (!owner) return;
    
    RectTransform* rectTransform = owner->GetComponent<RectTransform>();
    if (!rectTransform) return;

    Rectangle bounds = rectTransform->GetScreenRect();
    Vector2 mousePos = Mouse::GetPosition();

    // 1. Handle Focus (Clicking inside or outside the text box)
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (CheckCollisionPointRec(mousePos, bounds)) {
            m_isFocused = true;
            m_showCursor = true; // Show cursor immediately upon clicking
            m_cursorTimer = 0.0f;
        } else {
            m_isFocused = false;
        }
    }

    // 2. Handle Keyboard Input (Only if focused)
    if (m_isFocused) {
        // Cursor blink logic (toggle every 0.5 seconds)
        m_cursorTimer += deltaTime;
        if (m_cursorTimer >= 0.5f) {
            m_showCursor = !m_showCursor;
            m_cursorTimer = 0.0f;
        }

        // Handle Backspace (Support holding down the key)
        if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) {
            if (!text.empty()) {
                text.pop_back();
                // Reset cursor blink when typing
                m_showCursor = true; 
                m_cursorTimer = 0.0f;
            }
        }

        // Handle text typing (Raylib queue supports multiple chars per frame)
        int charPressed = GetCharPressed();
        while (charPressed > 0) {
            // Only allow standard printable characters (ASCII 32 to 125)
            if ((charPressed >= 32) && (charPressed <= 125) && (text.length() < maxLength)) {
                text += (char)charPressed;
                m_showCursor = true;
                m_cursorTimer = 0.0f;
            }
            charPressed = GetCharPressed(); // Fetch next char in queue
        }

        // 3. Handle 'Enter' Key - Broadcast an event like a button!
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
            m_isFocused = false; // Remove focus
            owner->OnGUIClick(owner->name); // Tell Python we finished typing!
        }
    }
}  

void TextInput::RenderHUD() {
    if (!owner) return;

    RectTransform* rectTransform = owner->GetComponent<RectTransform>();
    if (!rectTransform) return;

    Rectangle bounds = rectTransform->GetScreenRect();

    // 1. Draw Background Box
    Color currentBgColor = m_isFocused ? focusedBgColor : bgColor;
    DrawRectangleRec(bounds, currentBgColor);
    
    // Draw a subtle border
    DrawRectangleLinesEx(bounds, 2.0f, DARKGRAY);

    // 2. Draw Text (with a small 5px padding on the left)
    float textX = bounds.x + 5.0f;
    float textY = bounds.y + (bounds.height / 2.0f) - ((float)fontSize / 2.0f);
    
    Vector2 textPos = { textX, textY };
    DrawTextEx(m_font, text.c_str(), textPos, (float)fontSize, spacing, textColor);
    
    // 3. Draw Blinking Cursor
    if (m_isFocused && m_showCursor) {
        // Calculate where the text ends so we can place the cursor
        Vector2 textSize = MeasureTextEx(m_font, text.c_str(), (float)fontSize, spacing);
        
        float cursorX = textX + textSize.x + 2.0f;

        // Prevent the cursor from drawing outside the right edge of the box
        if (cursorX < bounds.x + bounds.width - 10.0f) {
            DrawRectangle((int)cursorX, (int)textY, 2, fontSize, textColor);
        }
    }
}

std::string TextInput::GetName() const {
    return "Text Input";
}

#ifndef STANDALONE_MODE
void TextInput::OnInspector() {
    static nlohmann::json initialState;
    auto HandleUndoRedo = [&]() {
        if (ImGui::IsItemActivated()) initialState = Serialize();
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, initialState, Serialize()));
        }
    };

    char buffer[256];
    strncpy(buffer, text.c_str(), sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';

    if (ImGui::InputText("Text", buffer, sizeof(buffer))) {
        text = buffer;
    }
    HandleUndoRedo();
    
    ImGui::DragInt("Max Length", &maxLength, 1, 1, 100);
    HandleUndoRedo();

    ImGui::Separator();

    std::string currentFontName = m_fontUUID == 0 ? "Default Font" : AssetRegistry::GetPathForUUID(m_fontUUID).filename().string();
    ImGui::Button(currentFontName.c_str(), ImVec2(-1, 0));
    
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
            std::string droppedPath = (const char*)payload->Data;
            std::filesystem::path fsPath(droppedPath);
            std::string ext = fsPath.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            
            if (ext == ".ttf" || ext == ".otf") {
                nlohmann::json stateBefore = Serialize();
                SetFont(droppedPath);
                CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, stateBefore, Serialize()));
            }
        }
        ImGui::EndDragDropTarget();
    }
    ImGui::Text("Drag & Drop a .ttf or .otf file above");

    ImGui::Spacing();

    ImGui::DragInt("Font Size", &fontSize, 1, 8, 128);
    HandleUndoRedo();
    
    ImGui::DragFloat("Spacing", &spacing, 0.1f, 0.0f, 20.0f);
    HandleUndoRedo();

    ImGui::Separator();

    EditorUI::ColorEdit4("Text Color", &textColor, this);
    EditorUI::ColorEdit4("Background", &bgColor, this);
    EditorUI::ColorEdit4("Focused Background", &focusedBgColor, this);
}
#endif

nlohmann::json TextInput::Serialize() const {
    return {
        {"type", "TextInput"},
        {"text", text},
        {"maxLength", maxLength},
        {"fontUUID", (uint64_t)m_fontUUID},
        {"fontSize", fontSize},
        {"spacing", spacing},
        {"tx_r", textColor.r}, {"tx_g", textColor.g}, {"tx_b", textColor.b}, {"tx_a", textColor.a},
        {"bg_r", bgColor.r}, {"bg_g", bgColor.g}, {"bg_b", bgColor.b}, {"bg_a", bgColor.a},
        {"fb_r", focusedBgColor.r}, {"fb_g", focusedBgColor.g}, {"fb_b", focusedBgColor.b}, {"fb_a", focusedBgColor.a}
    };
}

void TextInput::Deserialize(const nlohmann::json& j) {
    text = j.value("text", "");
    maxLength = j.value("maxLength", 20);
    
    if (j.contains("fontUUID")) {
        SetFont(UUID(j["fontUUID"].get<uint64_t>()));
    }

    fontSize = j.value("fontSize", 20);
    spacing = j.value("spacing", 1.0f);

    textColor = { static_cast<unsigned char>(j.value("tx_r", 0)), static_cast<unsigned char>(j.value("tx_g", 0)), static_cast<unsigned char>(j.value("tx_b", 0)), static_cast<unsigned char>(j.value("tx_a", 255)) };
    bgColor = { static_cast<unsigned char>(j.value("bg_r", 200)), static_cast<unsigned char>(j.value("bg_g", 200)), static_cast<unsigned char>(j.value("bg_b", 200)), static_cast<unsigned char>(j.value("bg_a", 255)) };
    focusedBgColor = { static_cast<unsigned char>(j.value("fb_r", 255)), static_cast<unsigned char>(j.value("fb_g", 255)), static_cast<unsigned char>(j.value("fb_b", 255)), static_cast<unsigned char>(j.value("fb_a", 255)) };
}
