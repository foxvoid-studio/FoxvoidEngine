#pragma once

#include "scene/Component.hpp"
#include "core/utils/UUID.hpp"
#include <string>
#include <pybind11/pybind11.h>
#include <nlohmann/json.hpp>
#include <filesystem>

namespace py = pybind11;

class [[gnu::visibility("default")]] ScriptComponent : public Component {
    public:
        ScriptComponent() = default;
        ~ScriptComponent() override;

        // Legacy constructor for backward compatibility
        ScriptComponent(const std::string& moduleName, const std::string& className);
        
        void Start() override;
        void Update(float deltaTime) override;
        void OnCollision(const Collision2D& collision) override;
        
        // Receive animation events and pass them to python
        void OnAnimationEvent(const std::string& eventName) override;

        void OnGUIClick(const std::string& buttonName) override;

        // Core method to load a script safely via UUID
        void LoadScript(UUID scriptUUID, const std::string& className);

        py::object GetInstance() const;
        std::string GetName() const override;

#ifndef STANDALONE_MODE
        void OnInspector() override;
#endif

        nlohmann::json Serialize() const override;
        void Deserialize(const nlohmann::json& j) override;

    private:
        void HotReload();

        // The UUID linking this component to its .py file on disk
        UUID m_scriptUUID = 0;

        // Cached strings to maintain performance and feed Python's importlib
        std::string m_scriptName;
        std::string m_className;
        std::string m_scriptFilePath;

        py::object m_instance; 
        std::filesystem::file_time_type m_lastWriteTime;
};
