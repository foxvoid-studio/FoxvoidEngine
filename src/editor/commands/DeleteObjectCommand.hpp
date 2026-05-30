#pragma once

#include <memory>
#include "ICommand.hpp"
#include "scene/Scene.hpp"
#include "scene/GameObject.hpp"

class DeleteObjectCommand : public ICommand {
    public:
        DeleteObjectCommand(Scene& scene, GameObject* object);

        void Execute() override;
        void Undo() override;

    private:
        Scene& m_scene;
        GameObject* m_targetObject;

        // This smart pointer will hold the object while it is "deleted"
        // to prevent it from being permanently destroyed from memory
        std::unique_ptr<GameObject> m_savedObject;
};
