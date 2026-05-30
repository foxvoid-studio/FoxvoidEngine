#pragma once

#include "ICommand.hpp"
#include "scene/GameObject.hpp"
#include "math/Transform2d.hpp"

struct Transform2dState {
    Vector2 position;
    float rotation;
    Vector2 scale;
};

class Transform2dCommand : public ICommand {
    public:
        Transform2dCommand(GameObject* object, const Transform2dState& oldState, const Transform2dState& newState);

        void Execute() override;
        void Undo() override;

    private:
        GameObject* m_object;
        Transform2dState m_oldState;
        Transform2dState m_newState;
};
