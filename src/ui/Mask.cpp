#include "Mask.hpp"

#ifndef STANDALONE_MODE
#include "editor/EditorUI.hpp"
#include "editor/commands/CommandHistory.hpp"
#include "editor/commands/ModifyComponentCommand.hpp"
#endif

Mask::Mask() : isActive(true) {}

std::string Mask::GetName() const {
    return "Mask (Clipping)";
}

#ifndef STANDALONE_MODE
void Mask::OnInspector() {
    static nlohmann::json initialState;

    if (ImGui::IsItemActivated()) {
        initialState = Serialize();
    }

    ImGui::TextDisabled("Clips children to RectTransform bounds.");
    ImGui::Separator();

    if (EditorUI::Checkbox("Is Active", &isActive, this)) {
        CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, initialState, Serialize()));
    }
}
#endif

nlohmann::json Mask::Serialize() const {
    return {
        {"type", "Mask"},
        {"isActive", isActive}
    };
}

void Mask::Deserialize(const nlohmann::json& j) {
    isActive = j.value("isActive", true);
}
