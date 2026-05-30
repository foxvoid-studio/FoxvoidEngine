#pragma once

#include <pybind11/pybind11.h>
#include <unordered_map>
#include <map>
#include <vector>
#include <functional>
#include <string>

#include "scene/GameObject.hpp"

namespace py = pybind11;

// Define function signatures for getters and adders
using ComponentGetter = std::function<py::object(GameObject&)>;
using ComponentAdder = std::function<py::object(GameObject&, py::args)>;

// Pure C++ Factory Signature
using ComponentFactory = std::function<Component*(GameObject&)>;

class ComponentRegistry {
    public:
        // C++17 inline statics: No need to instantiate them in a .cpp file!
        inline static std::unordered_map<std::string, ComponentGetter> getters;
        inline static std::unordered_map<std::string, ComponentAdder> adders;

        // Factory map and UI list
        inline static std::unordered_map<std::string, ComponentFactory> factories;

        // Map to group component types by category
        // std::map automatically sorts the categories alphabetically
        inline static std::map<std::string, std::vector<std::string>> categorizedTypes;

        template<typename T>
        static void Register(const std::string& typeName, ComponentAdder adderLogic) {
            // Automatically generate the GET logic for this type
            getters[typeName] = [](GameObject& go) -> py::object {
                T* comp = go.GetComponent<T>();
                if (comp) return py::cast(comp, py::return_value_policy::reference);
                return py::none();
            };

            // Store the custom ADD logic provided by the component's module
            adders[typeName] = adderLogic;

            // Register for C++ engine automatically
            RegisterCPP<T>(typeName);
        }

        // Pure C++ registration
        template<typename T>
        static void RegisterCPP(const std::string& typeName, const std::string& category = "General") {
            // Prevent duplicate entries in the UI list if registered multiple times
            if (factories.find(typeName) == factories.end()) {
                categorizedTypes[category].push_back(typeName);
            }

            // Store the factory lambda
            factories[typeName] = [](GameObject& go) -> Component* {
                // Instantiates the component using its default constructor
                return go.AddComponent<T>();
            };
        }
        
        // Returns a flat list of all registered component names
        static std::vector<std::string> GetFlatRegisteredTypes() {
            std::vector<std::string> flatList;

            // Iterate through every category in map
            for (const auto& [category, types] : categorizedTypes) {
                // Add all components from this category into our flat list
                for (const std::string& typeName : types) {
                    flatList.push_back(typeName);
                }
            }

            return flatList;
        }
};
