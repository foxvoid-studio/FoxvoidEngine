#pragma once

#include <string>
#include "ICommand.hpp"
#include "world/GameObject.hpp"

class ChangeTagCommand : public ICommand {
    public:
        ChangeTagCommand(GameObject* object, const std::string& oldTag, const std::string& newTag);

        void Execute() override;
        void Undo() override;

    private:
        GameObject* m_object;
        std::string m_oldTag;
        std::string m_newTag;
};
