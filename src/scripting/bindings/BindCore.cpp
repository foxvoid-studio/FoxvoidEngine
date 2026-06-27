#include "scripting/ScriptBindings.hpp"
#include <iostream>
#include <pybind11/stl.h>

#include "core/Engine.hpp"
#include "scene/GameObject.hpp"
#include "scene/Component.hpp"
#include "math/Transform2d.hpp"
#include "graphics/renderers/SpriteRenderer.hpp"
#include "graphics/renderers/SpriteSheetRenderer.hpp"
#include "graphics/animation/Animation2d.hpp"
#include "scene/ComponentRegistry.hpp"
#include "core/GameStateManager.hpp"
#include "core/events/EventBus.hpp"
#include "scripting/ScriptableObject.hpp"
#include "scripting/DataManager.hpp"
#include "scene/Scene.hpp"
#include "scripting/VFSLoader.hpp"
#include "scripting/VFSFinder.hpp"
#include "scripting/ScriptComponent.hpp"
#include "scene/PersistentComponent.hpp"

class Debug {
    public:
        void static Log(const std::string& msg) {
            std::cout << "[Python] " << msg << std::endl; 
        }

        void static Error(const std::string& msg) {
            std::cerr << "[Python] " << msg << std::endl;
        }
};

class PyScriptableObject : public ScriptableObject {
    public:
        using ScriptableObject::ScriptableObject;
};

void BindCore(py::module_& m) {
    // VFS Internal Bindings
    py::class_<VFSLoader>(m, "VFSLoader")
        .def(py::init<std::string, std::string>())
        .def("create_module", &VFSLoader::create_module)
        .def("exec_module", &VFSLoader::exec_module);

    py::class_<VFSFinder>(m, "VFSFinder")
        .def(py::init<>())
        .def("find_spec", &VFSFinder::find_spec);

    py::class_<Debug>(m, "Debug")
        .def_static("log", &Debug::Log)
        .def_static("error", &Debug::Error);

    py::class_<Component>(m, "Component")
        .def(py::init<>())
        .def_property_readonly("game_object", [](Component& c) { return c.owner; }, py::return_value_policy::reference)
        .def("start", &Component::Start)
        .def("update", &Component::Update, py::arg("delta_time"))
        .def("on_collision", &Component::OnCollision, py::arg("collision"))
        .def("on_animation_events", &Component::OnAnimationEvent, py::arg("event_name"))
        .def("on_gui_click", &Component::OnGUIClick, py::arg("button_name"));

    py::class_<GameObject>(m, "GameObject")
        .def_readwrite("name", &GameObject::name)
        .def_readwrite("tag", &GameObject::tag)
        .def_readonly("id", &GameObject::id)
        .def_readwrite("is_active", &GameObject::isActive)
        .def("is_active_in_hierarchy", &GameObject::IsActiveInHierarchy)
        .def("set_parent", &GameObject::SetParent, py::arg("new_parent"))
        // We use reference policy so Python doesn't try to take ownership and delete the parent/children
        .def("get_parent", &GameObject::GetParent, py::return_value_policy::reference)
        .def("get_children", &GameObject::GetChildren, py::return_value_policy::reference)
        .def("get_child_by_name", &GameObject::GetChildByName, py::return_value_policy::reference, "Recursively searches for a child GameObject by its name.")
        // We return GameObject* directly. Pybind11 will apply the reference policy automatically
        // and convert nullptr to Python's None safely.
        .def_static("spawn", [](const std::string& name) -> GameObject* {
            std::cout << "[C++] Attempting to spawn: " << name << std::endl;
            
            // Safety check in case the Engine singleton is null
            if (!Engine::Get()) {
                std::cerr << "[C++] FATAL ERROR: Engine::Get() returned nullptr!" << std::endl;
                return nullptr;
            }

            GameObject* newGo = Engine::Get()->GetActiveScene().CreateGameObject(name);

            if (!newGo) {
                std::cerr << "[Python] Failed to spawn: " << name << std::endl;
            }

            return newGo;
        }, py::return_value_policy::reference)
        // Allows Python scripts to spawn a completely pre-configured entity from a JSON file
        .def_static("instantiate", [](const std::string& prefabPath) -> GameObject* {
            // Safety check
            if (!Engine::Get()) {
                std::cerr << "[C++] FATAL ERROR: Engine::Get() returned nullptr!" << std::endl;
                return nullptr;
            }

            // Call the engine's active scene to read the file and build the object
            GameObject* newGo = Engine::Get()->GetActiveScene().Instantiate(prefabPath);

            if (!newGo) {
                std::cerr << "[Python] Failed to instantiate prefab from path: " << prefabPath << std::endl;
            }

            return newGo;
        }, py::return_value_policy::reference)
        // Expose the Destroy method so Python can schedule objects for deletion
        .def("destroy", &GameObject::Destroy)
        .def("get_component", [](GameObject& go, py::object type_obj) -> py::object {
            // Get the string name of the requested class (e.g., "Transform2d" or "PlayerController")
            std::string type_name = py::str(py::getattr(type_obj, "__name__"));
            
            // First, attempt to find a native C++ component in the registry
            auto it = ComponentRegistry::getters.find(type_name);
            if (it != ComponentRegistry::getters.end()) {
                // Execute the registered lambda and return the C++ component to Python
                return it->second(go); 
            }
            
            // If not found in native components, check if it's a Python script
            // Retrieve all ScriptComponents attached to this GameObject
            auto scripts = go.GetComponents<ScriptComponent>();
            
            for (auto* script : scripts) {
                // Retrieve the actual Python instance running inside this ScriptComponent
                py::object py_instance = script->GetInstance(); 
                
                // Check if the instance is valid and matches the requested Python type
                // py::isinstance correctly handles Python inheritance
                if (!py_instance.is_none() && py::isinstance(py_instance, type_obj)) {
                    return py_instance;
                }
            }
            
            // Return None if the component is neither native nor a matching Python script
            return py::none();
        })
        .def("add_component", [](GameObject& go, py::object type_obj, py::args args) -> py::object {
            std::string type_name = py::str(py::getattr(type_obj, "__name__"));
            
            // Search the registry for the adder function
            auto it = ComponentRegistry::adders.find(type_name);
            if (it != ComponentRegistry::adders.end()) {
                return it->second(go, args); // Execute the lambda with args
            }

            std::cerr << "[Python] Unknown component type: " << type_name << std::endl;
            return py::none();
        });

    py::class_<Engine>(m, "SceneManager")
        .def_static("load_scene", [](const std::string& path) {
            if (Engine::Get()) {
                Engine::Get()->LoadScene(path);
            }
            else {
                std::cerr << "[Python] Cannot load scene: Engine is null!" << std::endl;
            }
        });

    py::class_<Scene>(m, "Scene")
        .def_static("find_object_by_name", 
            [](const std::string& name) -> GameObject* {
                if (Engine::Get()) {
                    return Engine::Get()->GetActiveScene().FindObjectByName(name);
                }
                return nullptr;
            },
            py::return_value_policy::reference, py::arg("name")
        )
        .def_static("find_objects_with_tag", 
            [](const std::string& targetTag) -> std::vector<GameObject*> {
                if (Engine::Get()) {
                    return Engine::Get()->GetActiveScene().FindObjectsWithTag(targetTag);
                }
                return {};
            },
            py::return_value_policy::reference
        )
        .def_static("get_all_objects", []() -> std::vector<GameObject*> {
            std::vector<GameObject*> rawObjects;
            
            if (Engine::Get()) {
                // Get the read-only reference to the scene's unique_ptrs
                const auto& uniqueObjects = Engine::Get()->GetActiveScene().GetGameObjects();
                rawObjects.reserve(uniqueObjects.size());
                
                // Convert to raw pointers so Python can reference them safely
                // without interfering with C++ memory management
                for (const auto& obj : uniqueObjects) {
                    rawObjects.push_back(obj.get());
                }
            }
            
            return rawObjects;
        }, py::return_value_policy::reference);

    py::class_<PersistentComponent, Component>(m, "PersistentComponent")
        .def(py::init<>());

    ComponentRegistry::Register<PersistentComponent>("PersistentComponent",
        [](GameObject& go, py::args args) -> py::object {
            auto* p = go.AddComponent<PersistentComponent>();
            return py::cast(p, py::return_value_policy::reference);
        }
    );

    py::class_<GameStateManager>(m, "Globals")
        .def_static("set_int", &GameStateManager::SetInt)
        .def_static("get_int", &GameStateManager::GetInt, py::arg("key"), py::arg("default_val") = 0)
        
        .def_static("set_float", &GameStateManager::SetFloat)
        .def_static("get_float", &GameStateManager::GetFloat, py::arg("key"), py::arg("default_val") = 0.0f)
        
        .def_static("set_bool", &GameStateManager::SetBool)
        .def_static("get_bool", &GameStateManager::GetBool, py::arg("key"), py::arg("default_val") = false)
        
        .def_static("set_string", &GameStateManager::SetString)
        .def_static("get_string", &GameStateManager::GetString, py::arg("key"), py::arg("default_val") = "");

    py::class_<ScriptableObject, PyScriptableObject>(m, "ScriptableObject")
        .def(py::init<>())
        .def_readwrite("asset_id", &ScriptableObject::assetId)
        .def_readwrite("name", &ScriptableObject::name);

    py::class_<DataManager>(m, "DataManager")
        // We bind them as static methods so Python can call DataManager.load_asset(...)
        .def_static("load_asset", &DataManager::LoadAsset, py::arg("filepath"))
        .def_static("save_asset", &DataManager::SaveAsset, py::arg("asset"), py::arg("filepath"))
        .def_static("clear_cache", &DataManager::ClearCache);

    py::class_<EventBus>(m, "EventBus")
        .def_static("subscribe", [](const std::string& eventName, py::function callback) {
            return EventBus::Subscribe(eventName, [callback](const nlohmann::json& payload) {
                // Bridge: C++ nlohmann::json -> Python Dict
                // We stringify the JSON in C++ and use Python's fast json module to decode it into a dict
                std::string jsonStr = payload.dump();
                py::module_ json = py::module_::import("json");
                py::object pyPayload = json.attr("loads")(jsonStr);
                
                callback(pyPayload);
            });
        })
        .def_static("unsubscribe", &EventBus::Unsubscribe)
        .def_static("publish", [](const std::string& eventName, py::object pyPayload) {
            // Bridge: Python Dict -> C++ nlohmann::json
            py::module_ json = py::module_::import("json");
            std::string jsonStr = py::cast<std::string>(json.attr("dumps")(pyPayload));
            
            EventBus::Publish(eventName, nlohmann::json::parse(jsonStr));
        })
        // Overload to allow publishing without a payload
        .def_static("publish_empty", [](const std::string& eventName) {
            EventBus::Publish(eventName);
        });
}
