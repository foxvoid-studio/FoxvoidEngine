#pragma once

#include <string>
#include "ICommand.hpp"
#include "scene/GameObject.hpp"

class ChangeNameCommand : public ICommand {
    public:
        ChangeNameCommand(GameObject* object, const std::string& oldName, const std::string& newName);

        void Execute() override;
        void Undo() override;

    private:
        GameObject* m_object;
        std::string m_oldName;
        std::string m_newName;
};
