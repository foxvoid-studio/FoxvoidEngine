#pragma once

#include "ICommand.hpp"
#include "scene/Component.hpp"

class ModifyComponentCommand : public ICommand {
    public:
        ModifyComponentCommand(Component* component, const nlohmann::json& oldState, const nlohmann::json& newState);

        void Execute() override;
        void Undo() override;

    private:
        Component* m_component;
        nlohmann::json m_oldState;
        nlohmann::json m_newState;
};
