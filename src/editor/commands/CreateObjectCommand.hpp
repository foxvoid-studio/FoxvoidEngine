#pragma once

#include <memory>
#include "ICommand.hpp"
#include "scene/Scene.hpp"
#include "scene/GameObject.hpp"

class CreateObjectCommand : public ICommand {
    public:
        CreateObjectCommand(Scene& scene, GameObject* newObject);

        void Execute() override;
        void Undo() override;

    private:
        Scene& m_scene;
        GameObject* m_targetObject;
        std::unique_ptr<GameObject> m_savedObject;
        bool m_isFirstExecution;
};
