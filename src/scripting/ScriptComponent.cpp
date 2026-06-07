#include "ScriptComponent.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <map>
#include <vector>
#include <algorithm>

#include "core/assets/AssetRegistry.hpp"
#include "scene/GameObject.hpp"
#include "scene/ComponentRegistry.hpp"

#ifndef STANDALONE_MODE
#include "editor/EditorUI.hpp"
#include "editor/commands/CommandHistory.hpp"
#include "editor/commands/ModifyComponentCommand.hpp"
#endif

ScriptComponent::ScriptComponent(const std::string& moduleName, const std::string& className)
    : m_scriptName(moduleName), m_className(className)
{
    // Legacy support: Try to deduce the path and get the UUID
    std::string assumedPath = "assets/scripts/" + moduleName + ".py";
    UUID uuid = AssetRegistry::GetUUIDForPath(assumedPath);
    LoadScript(uuid, className);
}

ScriptComponent::~ScriptComponent() {
    if (m_instance) {
        try {
            Component* nativeComponent = m_instance.cast<Component*>();
            if (nativeComponent) {
                nativeComponent->owner = nullptr;
            }
            
            m_instance = py::none(); 
        } catch (...) {
            
        }
    }
}

void ScriptComponent::LoadScript(UUID scriptUUID, const std::string& className) {
    m_scriptUUID = scriptUUID;
    m_className = className;

    // On ne s'arrête plus si className est vide, on a juste besoin de l'UUID valide !
    if (m_scriptUUID == 0) return;

    // Resolve the UUID to the physical file path
    std::filesystem::path path = AssetRegistry::GetPathForUUID(m_scriptUUID);
    if (path.empty()) {
        std::cerr << "[ScriptComponent] Error: Could not resolve script UUID " << (uint64_t)m_scriptUUID << std::endl;
        return;
    }

    // Extract the module name (filename without extension) for Python import
    m_scriptName = path.stem().string();
    m_scriptFilePath = path.string();

    if (m_className.empty()) {
        m_className = m_scriptName;
        std::ifstream file(m_scriptFilePath);
        if (file.is_open()) {
            std::string line;
            while (std::getline(file, line)) {
                size_t classPos = line.find("class ");
                if (classPos != std::string::npos) {
                    size_t start = classPos + 6;
                    size_t end = line.find_first_of("(: \r\n", start);
                    if (end != std::string::npos) {
                        m_className = line.substr(start, end - start);
                        break; 
                    }
                }
            }
            file.close();
        }
    }

    try {
        py::module_ mod = py::module_::import(m_scriptName.c_str());
        py::object cls = mod.attr(m_className.c_str());
        
        m_instance = cls();
        
        // Immediately bind the owner pointer so Python knows its GameObject
        Component* nativeComponent = m_instance.cast<Component*>();
        if (nativeComponent && this->owner) {
            nativeComponent->owner = this->owner;
        }

        if (std::filesystem::exists(m_scriptFilePath)) {
            m_lastWriteTime = std::filesystem::last_write_time(m_scriptFilePath);
        }

    } catch (const py::error_already_set& e) {
        std::cerr << "[ScriptComponent] LoadScript Error (" << m_scriptName << "):\n" << e.what() << std::endl;
    }
}

void ScriptComponent::Start() {
    if (!m_instance) return;

    try {
        Component* nativeComponent = m_instance.cast<Component*>();

        if (nativeComponent) {
            // We bypass Python entirely and set the C++ pointer directly.
            // When Python scripts call 'self.game_object', it will read this exact pointer!
            nativeComponent->owner = this->owner;
        }

        // Call the start method in Python
        if (py::hasattr(m_instance, "start")) {
            m_instance.attr("start")();
        }
    } catch (const py::error_already_set& e) {
        std::cerr << "[ScriptComponent] Start Error:\n" << e.what() << std::endl;
    }
}

void ScriptComponent::Update(float deltaTime) {
    // Hot reloading check
    try {
        // Ensure the path is not empty before checking the filesystem
        if (!m_scriptFilePath.empty() && std::filesystem::exists(m_scriptFilePath)) {
            auto currentWriteTime = std::filesystem::last_write_time(m_scriptFilePath);
            
            // If the file on disk is newer than our cached version
            if (currentWriteTime > m_lastWriteTime) {
                m_lastWriteTime = currentWriteTime;
                HotReload();
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "[ScriptComponent] File system error: " << e.what() << std::endl;
    }

    // Standard update execution
    try {
        if (m_instance && py::hasattr(m_instance, "update")) {
            m_instance.attr("update")(deltaTime);
        }
    } catch (const py::error_already_set& e) {
        std::cerr << "[ScriptComponent] Update Error:\n" << e.what() << std::endl;
    }
}

void ScriptComponent::OnCollision(const Collision2D& collision) {
    if (!m_instance) return;

    try {
        // Look for 'on_collision' in the Python script
        if (py::hasattr(m_instance, "on_collision")) {
            // Call the python method passing the Collision2D struct
            m_instance.attr("on_collision")(collision);
        }
    } catch (const py::error_already_set& e) {
        std::cerr << "[ScriptComponent] OnCollision Error:\n" << e.what() << std::endl;
    }
}

void ScriptComponent::OnAnimationEvent(const std::string& eventName) {
    if (!m_instance) return;

    try {
        // Look for 'on_animation_event' in the Python script
        if (py::hasattr(m_instance, "on_animation_event")) {
            // Call the python method passing the event name string
            m_instance.attr("on_animation_event")(eventName);
        }
    } catch (const py::error_already_set& e) {
        std::cerr << "[ScriptComponent] OnAnimationEvent Error:\n" << e.what() << std::endl;
    }
}

void ScriptComponent::OnGUIClick(const std::string& buttonName) {
    // Ensute the Python instance is alive and has the method
    if (m_instance && py::hasattr(m_instance, "on_gui_click")) {
        try {
            // Call the Python method: def on_gui_click(self, button_name: str):
            m_instance.attr("on_gui_click")(buttonName);
        } catch (py::error_already_set& e) {
            std::cerr << "[ScriptComponent] Python error in on_gui_click (" << owner->name << "):\n" 
                      << e.what() << std::endl;
        }
    }
}

void ScriptComponent::HotReload() {
    std::cout << "[ScriptEngine] File changed. Hot reloading: " << m_scriptName << "..." << std::endl;

    try {
        // Import Python's built-in importlib module
        py::module_ importlib = py::module_::import("importlib");
        py::module_ sys = py::module_::import("sys");
        py::dict modules = sys.attr("modules");

        // Check if our module is actually loaded in Python's cache
        if (modules.contains(m_scriptName)) {
            py::object currentModule = modules[m_scriptName.c_str()];
            
            // Force Python to re-read and re-compile the .py file
            importlib.attr("reload")(currentModule);
            
            // Fetch the newly compiled class
            py::object newClass = currentModule.attr(m_className.c_str());
            
            // We dynamically swap the class of our existing Python instance.
            // This replaces the methods (like update) but keeps all existing variables 
            // (like speed, health, transform references) perfectly intact!
            m_instance.attr("__class__") = newClass;

            // Changing the class doesn't re-run __init__. If the user added 'self.new_var = 10', 
            // it won't exist in m_instance. We create a temporary dummy instance to find new variables.
            py::object dummyInstance = newClass();
            py::dict dummyDict = dummyInstance.attr("__dict__");
            py::dict currentDict = m_instance.attr("__dict__");

            for (auto item : dummyDict) {
                // If the dummy has a variable that our running instance does NOT have, inject it!
                if (!currentDict.contains(item.first)) {
                    currentDict[item.first] = item.second;
                }
            }

            // Collect keys to remove to avoid iterator invalidation
            std::vector<py::object> keysToRemove;
            for (auto item : currentDict) {
                if (!dummyDict.contains(item.first)) {
                    keysToRemove.push_back(py::reinterpret_borrow<py::object>(item.first));
                }
            }

            // Safely remove deprecated variables
            for (const auto& key : keysToRemove) {
                currentDict.attr("pop")(key);
            }
            
            std::cout << "[ScriptEngine] Hot reload successful!" << std::endl;
        }
    } catch (const py::error_already_set& e) {
        // If you make a syntax error in your Python script and save it, 
        // the engine will catch it here without crashing, print the error, 
        // and keep running the old version of the script until you fix the typo!
        std::cerr << "[ScriptEngine] Failed to hot reload (Syntax Error?):\n" << e.what() << std::endl;
    }
}

py::object ScriptComponent::GetInstance() const {
    return m_instance;
}

std::string ScriptComponent::GetName() const {
    // Show the python class name in the Inspector (e.g., "PlayerController (Script)")
    return m_className + " (Script)";
}

#ifndef STANDALONE_MODE
void ScriptComponent::OnInspector() {
    // Script Binding (Only visible if no script is currently loaded)
    if (!m_instance) {
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "No valid Python instance.");
        
        std::string currentPath = m_scriptUUID != 0 ? AssetRegistry::GetPathForUUID(m_scriptUUID).string() : "Drop a .py file here";
        
        char pathBuffer[256];
        char clsBuffer[256];
        strncpy(pathBuffer, currentPath.c_str(), sizeof(pathBuffer));
        strncpy(clsBuffer, m_className.c_str(), sizeof(clsBuffer));

        // Make the path input read-only in the UI. Drag & Drop is the primary way to assign scripts now.
        ImGui::InputText("File", pathBuffer, sizeof(pathBuffer), ImGuiInputTextFlags_ReadOnly);
        
        // Drag and drop target for Python files
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                std::string droppedPath = (const char*)payload->Data;
                std::filesystem::path fsPath(droppedPath);
                
                if (fsPath.extension() == ".py") {
                    nlohmann::json initialState = Serialize();

                    UUID newUUID = AssetRegistry::GetUUIDForPath(fsPath);
                    std::string parsedClassName = fsPath.stem().string(); // Default fallback

                    // Extract class name from the file
                    std::ifstream file(fsPath);
                    if (file.is_open()) {
                        std::string line;
                        while (std::getline(file, line)) {
                            size_t classPos = line.find("class ");
                            if (classPos != std::string::npos) {
                                size_t start = classPos + 6;
                                size_t end = line.find_first_of("(: \r\n", start);
                                if (end != std::string::npos) {
                                    parsedClassName = line.substr(start, end - start);
                                    break; 
                                }
                            }
                        }
                        file.close();
                    }

                    LoadScript(newUUID, parsedClassName);
                    CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, initialState, Serialize()));
                }
            }
            ImGui::EndDragDropTarget();
        }

        // Allow manual class name edits (useful if multiple classes exist in one file)
        if (ImGui::InputText("Class", clsBuffer, sizeof(clsBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
            if (m_scriptUUID != 0 && std::string(clsBuffer) != m_className) {
                nlohmann::json initialState = Serialize();
                LoadScript(m_scriptUUID, clsBuffer);
                CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, initialState, Serialize()));
            }
        }

        ImGui::Separator();
        return; // Stop drawing here if no instance is loaded
    }

    // Hot Reload Check for Editor Mode (Only runs if a script is loaded)
    try {
        if (!m_scriptFilePath.empty() && std::filesystem::exists(m_scriptFilePath)) {
            auto currentWriteTime = std::filesystem::last_write_time(m_scriptFilePath);
            if (currentWriteTime > m_lastWriteTime) {
                m_lastWriteTime = currentWriteTime;
                HotReload();
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "[ScriptComponent] Inspector File system error: " << e.what() << std::endl;
    }

    // EXTRACTION : Required Components (with OR logic)
    struct RequiredComp {
        std::string className;
        std::string moduleName;
    };

    struct RequiredCompGroup {
        std::vector<RequiredComp> options;
    };

    std::vector<RequiredCompGroup> requiredComponents;
        
    if (py::hasattr(m_instance, "__require_components__")) {
        try {
            py::list pyRequired = m_instance.attr("__require_components__");
            for (auto item : pyRequired) {
                RequiredCompGroup group;

                // Check if the current item is a sub-list (OR condition)
                if (py::isinstance<py::list>(item)) {
                    py::list subList = item.cast<py::list>();
                    for (auto subItem : subList) {
                        RequiredComp req;
                        req.className = py::str(subItem.attr("__name__"));
                        if (py::hasattr(subItem, "__module__")) {
                            req.moduleName = py::str(subItem.attr("__module__"));
                        }
                        group.options.push_back(req);
                    }
                } 
                // It's a standard single required component (AND condition)
                else {
                    RequiredComp req;
                    req.className = py::str(item.attr("__name__"));
                    if (py::hasattr(item, "__module__")) {
                        req.moduleName = py::str(item.attr("__module__"));
                    }
                    group.options.push_back(req);
                }

                requiredComponents.push_back(group);
            }
        } catch (const std::exception& e) {
            std::cerr << "[Editor] Error reading __require_components__: " << e.what() << std::endl;
        }
    }

    // Render : Missing Components Warning
    if (!requiredComponents.empty() && this->owner != nullptr) {
        bool hasMissing = false;

        for (const auto& group : requiredComponents) {
            bool isGroupSatisfied = false;
                
            // 1. Check if ANY of the options in the group is attached
            for (const auto& req : group.options) {
                bool isNative = ComponentRegistry::factories.find(req.className) != ComponentRegistry::factories.end();

                if (isNative) {
                    auto it = ComponentRegistry::getters.find(req.className);
                    if (it != ComponentRegistry::getters.end()) {
                        py::object comp = it->second(*this->owner);
                        if (!comp.is_none()) {
                            isGroupSatisfied = true;
                            break; // Stop checking this group, requirement met!
                        }
                    }
                } else {
                    auto scripts = this->owner->GetComponents<ScriptComponent>();
                    for (auto* script : scripts) {
                        if (script->m_className == req.className) {
                            isGroupSatisfied = true;
                            break; // Stop checking this group, requirement met!
                        }
                    }
                }
            }

            // 2. If the requirement is NOT met, draw the UI
            if (!isGroupSatisfied) {
                if (!hasMissing) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                    ImGui::TextWrapped("Missing Required Components:");
                    ImGui::PopStyleColor();
                    hasMissing = true;
                }

                // If multiple options (OR logic), add a small label to guide the user
                if (group.options.size() > 1) {
                    ImGui::TextDisabled("Choose one of the following Colliders:");
                }

                // Draw a button for each possible resolution
                for (const auto& req : group.options) {
                    std::string buttonLabel = "Add " + req.className;
                    if (ImGui::Button(buttonLabel.c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                        
                        bool isNative = ComponentRegistry::factories.find(req.className) != ComponentRegistry::factories.end();
                        if (isNative) {
                            ComponentRegistry::factories[req.className](*this->owner);
                            std::cout << "[Editor] Auto-added Native Component: " << req.className << std::endl;
                        } else {
                            this->owner->AddComponent<ScriptComponent>(req.moduleName, req.className);
                            std::cout << "[Editor] Auto-added Script Component: " << req.className << std::endl;
                        }

                        // CRITICAL: Exit immediately to prevent Iterator Invalidation 
                        // as discussed in our previous session!
                        return; 
                    }
                }
            }
        }

        if (hasMissing) {
            ImGui::Separator();
        }
    }

    // Dynamic Variables Inspection
    try {
        py::dict attributes = m_instance.attr("__dict__");
        static nlohmann::json initialDynamicState;

        std::map<std::string, std::string> tooltips;
        if (py::hasattr(m_instance, "__tooltips__")) {
            try {
                py::dict pyTooltips = m_instance.attr("__tooltips__");
                for (auto item : pyTooltips) {
                    tooltips[py::str(item.first)] = py::str(item.second);
                }
            } catch (const std::exception& e) {
                std::cerr << "[Editor] Error reading __tooltips__:" << e.what() << std::endl;
            }
        }

        std::map<std::string, std::pair<float, float>> ranges;
        if (py::hasattr(m_instance, "__ranges__")) {
            try {
                py::dict pyRanges = m_instance.attr("__ranges__");
                for (auto item : pyRanges) {
                    py::tuple limits = py::cast<py::tuple>(item.second);
                    if (py::len(limits) == 2) {
                        ranges[py::str(item.first)] = {
                            limits[0].cast<float>(),
                            limits[1].cast<float>()
                        };
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "[Editor] Error reading __ranges__: " << e.what() << std::endl;
            }
        }

        std::vector<std::string> multilineVars;
        if (py::hasattr(m_instance, "__multiline__")) {
            try {
                py::list pyMultiline = m_instance.attr("__multiline__");
                for (auto item : pyMultiline) {
                    multilineVars.push_back(py::str(item));
                }
            } catch (const std::exception& e) {
                std::cerr << "[Editor] Error reading __multiline__: " << e.what() << std::endl;
            }
        }

        std::vector<std::string> readonlyVars;
        if (py::hasattr(m_instance, "__readonly__")) {
            try {
                py::list pyReadonly = m_instance.attr("__readonly__");
                for (auto item : pyReadonly) {
                    readonlyVars.push_back(py::str(item));
                }
            } catch (const std::exception& e) {
                std::cerr << "[Editor] Error reading __readonly__: " << e.what() << std::endl;
            }
        }

        std::vector<std::string> actionMethods;
        if (py::hasattr(m_instance, "__actions__")) {
            try {
                py::list pyActions = m_instance.attr("__actions__");
                for (auto item : pyActions) {
                    actionMethods.push_back(py::str(item));
                }
            } catch (const std::exception& e) {
                std::cerr << "[Editor] Error reading __actions__: " << e.what() << std::endl;
            }
        }

        // Helper Lambda to draw a single property
        // This avoids duplicating the drawing code for categorized and uncategorized variables
        auto DrawPythonVariable = [&](const std::string& key, py::object value) {
            if (py::isinstance<Component>(value)) return;

            // Check if the current variable is flagged as read-only
            bool isReadonly = std::find(readonlyVars.begin(), readonlyVars.end(), key) != readonlyVars.end();
            
            // Disable interactions and gray out the widget if it's read-only
            if (isReadonly) {
                ImGui::BeginDisabled();
            }

            // Fetch the Python class of the variable
            py::object cls = value.attr("__class__");

            // 1. Check for Enums FIRST!
            // (Crucial: StrEnum matches py::str, so we must intercept it before the string check)
            if (py::hasattr(cls, "__members__")) {
                std::string currentName = py::str(value.attr("name"));
                
                // Draw a Dropdown menu
                if (ImGui::BeginCombo(key.c_str(), currentName.c_str())) {
                    
                    // Retrieve all available options from the Python Enum class
                    py::dict members = cls.attr("__members__");
                    
                    for (auto item : members) {
                        std::string memberName = py::str(item.first);
                        bool isSelected = (currentName == memberName);
                        
                        // If the user selects a new option
                        if (ImGui::Selectable(memberName.c_str(), isSelected)) {
                            // Capture the state BEFORE assigning the new value for Undo/Redo
                            nlohmann::json stateBefore = Serialize();
                            
                            // Assign the actual Enum instance back to Python (e.g., Direction.UP)
                            m_instance.attr(key.c_str()) = item.second; 
                            
                            CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, stateBefore, Serialize()));
                        }
                        
                        // Keep the current item visually focused when opening the dropdown
                        if (isSelected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }
            }
            // 2. Fallback to standard types
            else if (py::isinstance<py::float_>(value)) {
                float v = value.cast<float>();

                float min = 0.0f, max = 0.0f;
                bool hasRange = ranges.contains(key);
                if (hasRange) {
                    min = ranges.at(key).first;
                    max = ranges.at(key).second;
                }

                if (EditorUI::DragFloat(key.c_str(), &v, 0.1f, this, min, max)) {
                    if (hasRange) v = std::clamp(v, min, max);
                    
                    m_instance.attr(key.c_str()) = v;
                }
            }
            else if (py::isinstance<py::bool_>(value)) {
                bool v = value.cast<bool>();
                if (EditorUI::Checkbox(key.c_str(), &v, this)) {
                    m_instance.attr(key.c_str()) = v;
                }
            }
            else if (py::isinstance<py::int_>(value)) {
                int v = value.cast<int>();
                
                int min = 0, max = 0;
                bool hasRange = ranges.contains(key);
                if (hasRange) {
                    min = static_cast<int>(ranges.at(key).first);
                    max = static_cast<int>(ranges.at(key).second);
                }

                if (EditorUI::DragInt(key.c_str(), &v, 1.0f, this)) {
                    if (hasRange) v = std::clamp(v, min, max);
                    
                    m_instance.attr(key.c_str()) = v;
                }
            }
            else if (py::isinstance<py::str>(value)) {
                std::string v = value.cast<std::string>();
                
                // A generous buffer size for long dialogues or quest descriptions
                char buffer[2048];
                strncpy(buffer, v.c_str(), sizeof(buffer));
                buffer[sizeof(buffer) - 1] = '\0';
                
                // Check if the current variable is flagged as a multiline string
                bool isMultiline = std::find(multilineVars.begin(), multilineVars.end(), key) != multilineVars.end();

                // Draw the appropriate ImGui widget
                if (isMultiline) {
                    // ImVec2(0, height) -> 0 makes it take the full available width, 
                    // and we set a fixed height (e.g., ~4 lines of text)
                    ImGui::InputTextMultiline(key.c_str(), buffer, sizeof(buffer), ImVec2(0, ImGui::GetTextLineHeight() * 4.2f));
                } else {
                    ImGui::InputText(key.c_str(), buffer, sizeof(buffer));
                }

                // Handle Undo/Redo history and Python value assignment
                if (ImGui::IsItemActivated()) {
                    initialDynamicState = Serialize();
                }
                if (ImGui::IsItemActive()) {
                    m_instance.attr(key.c_str()) = std::string(buffer);
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(this, initialDynamicState, Serialize()));
                }
            }
            else {
                std::string typeName = py::str(value.get_type().attr("__name__"));
                ImGui::TextDisabled("%s : [%s]", key.c_str(), typeName.c_str());
            }

            if (tooltips.contains(key) && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::BeginTooltip();
                    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
                    ImGui::TextUnformatted(tooltips.at(key).c_str());
                    ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }

            // Re-enable interactions for the next UI elements
            if (isReadonly) {
                ImGui::EndDisabled();
            }
        };

        // Categories Logic
        std::map<std::string, std::vector<std::string>> categories;
        std::vector<std::string> categorizedVars;

        // Attempt to read the __categories__ dictionary if defined in the Python class
        if (py::hasattr(m_instance, "__categories__")) {
            try {
                py::dict pyCategories = m_instance.attr("__categories__");
                for (auto item : pyCategories) {
                    std::string catName = py::str(item.first);
                    py::list varsList = py::cast<py::list>(item.second);
                    
                    for (auto var : varsList) {
                        std::string varName = py::str(var);
                        categories[catName].push_back(varName);
                        categorizedVars.push_back(varName); // Track everything that has a category
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "[Editor] Error reading __categories__: " << e.what() << std::endl;
            }
        }

        // Draw the categorized variables first, inside collapsible TreeNodes
        for (const auto& [catName, vars] : categories) {
            // Make the category open by default
            if (ImGui::TreeNodeEx(catName.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                for (const auto& varName : vars) {
                    // Safety check: ensure the variable actually exists in the instance's __dict__
                    if (attributes.contains(varName.c_str())) {
                        py::object varValue = py::reinterpret_borrow<py::object>(attributes[varName.c_str()]);
                        DrawPythonVariable(varName, varValue);
                    } else {
                        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Warning: '%s' not found", varName.c_str());
                    }
                }
                ImGui::TreePop(); // Close the category folder
            }
        }

        // Uncategorized Logic
        // Check if there are any variables left to draw outside of categories
        bool hasUncategorized = false;
        for (auto item : attributes) {
            std::string key = py::str(item.first);
            if (key.rfind("_", 0) == 0) continue; // Skip private

            // If a valid variable is not in the categorized list, flag it
            if (std::find(categorizedVars.begin(), categorizedVars.end(), key) == categorizedVars.end()) {
                hasUncategorized = true;
                break;
            }
        }

        // Draw a separator only if we have both categories AND uncategorized variables
        if (hasUncategorized && !categories.empty()) {
            ImGui::Separator();
            ImGui::TextDisabled("Uncategorized");
        }

        // Draw the remaining variables
        for (auto item : attributes) {
            std::string key = py::str(item.first);
            
            // Skip private variables (starting with '_')
            if (key.rfind("_", 0) == 0) continue;

            // Only draw if it wasn't already drawn in a category
            if (std::find(categorizedVars.begin(), categorizedVars.end(), key) == categorizedVars.end()) {
                py::object value = py::reinterpret_borrow<py::object>(item.second);
                DrawPythonVariable(key, value);
            }
        }

        // Draw custom buttons at the bottom of the inspector for specific script methods
        if (!actionMethods.empty()) {
            ImGui::Separator();
            ImGui::TextDisabled("Script Actions");

            // Draw a button for each declared method in the __actions__ list
            for (const auto& methodName : actionMethods) {
                if (ImGui::Button(methodName.c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                    try {
                        // Double check if the method actually exists before invoking it
                        if (py::hasattr(m_instance, methodName.c_str())) {
                            m_instance.attr(methodName.c_str())();
                        } else {
                            std::cerr << "[ScriptComponent] Warning: Method '" << methodName 
                                      << "' declared in __actions__ but not found in script." << std::endl;
                        }
                    } catch (const py::error_already_set& e) {
                        std::cerr << "[ScriptComponent] Action Execution Error (" << methodName << "):\n" 
                                  << e.what() << std::endl;
                    }
                }
            }
        }

    } 
    catch (const py::error_already_set& e) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Python Error:");
        ImGui::TextWrapped("%s", e.what());
    }
}
#endif

nlohmann::json ScriptComponent::Serialize() const {
    nlohmann::json j;
    
    j["type"] = "ScriptComponent";

    // Save the core data needed to find and reload the script
    j["scriptUUID"] = (uint64_t)m_scriptUUID;
    j["className"] = m_className;

    // Automatically save all valid Python variables
    nlohmann::json properties;

    if (m_instance) {
        try {
            py::dict attributes = m_instance.attr("__dict__");

            for (auto item : attributes) {
                std::string key = py::str(item.first);
                
                // Skip private/internal variables
                if (key.rfind("_", 0) == 0) continue;

                py::object value = py::reinterpret_borrow<py::object>(item.second);

                // Skip component references (they are managed by the C++ engine, not the script state)
                if (py::isinstance<Component>(value)) continue;

                // Dynamically check types and save them to the JSON properties object
                py::object cls = value.attr("__class__");

                // Enums must be intercepted first and saved by their string 'name' (e.g., "UP", "IDLE")
                if (py::hasattr(cls, "__members__")) {
                    properties[key] = py::str(value.attr("name")).cast<std::string>();
                }
                else if (py::isinstance<py::bool_>(value)) {
                    properties[key] = value.cast<bool>();
                } 
                else if (py::isinstance<py::float_>(value)) {
                    properties[key] = value.cast<float>();
                } 
                else if (py::isinstance<py::int_>(value)) {
                    properties[key] = value.cast<int>();
                } 
                else if (py::isinstance<py::str>(value)) {
                    properties[key] = value.cast<std::string>();
                }
            }
        } catch (const py::error_already_set& e) {
            std::cerr << "[ScriptComponent] Auto-Serialize Error:\n" << e.what() << std::endl;
        }
    }

    j["properties"] = properties;

    return j;
}

void ScriptComponent::Deserialize(const nlohmann::json& j) {
    // Restore the script identifiers
    if (j.contains("scriptName")) m_scriptName = j["scriptName"].get<std::string>();
    
    // Backward compatibility & UUID loading
    if (j.contains("scriptUUID")) {
        LoadScript(UUID(j["scriptUUID"].get<uint64_t>()), m_className);
    } 
    else if (j.contains("scriptName")) {
        std::string sName = j["scriptName"].get<std::string>();
        std::string assumedPath = "assets/scripts/" + sName + ".py";
        LoadScript(AssetRegistry::GetUUIDForPath(assumedPath), m_className);
    }
    
    // If this component was created via deserialization (empty), we need to instantiate Python now
    if (!m_instance && !m_scriptName.empty() && !m_className.empty()) {
        try {
            py::module_ mod = py::module_::import(m_scriptName.c_str());
            py::object cls = mod.attr(m_className.c_str());
            m_instance = cls();
            
            // IMPORTANT: Immediately bind the owner pointer so Python knows its GameObject
            Component* nativeComponent = m_instance.cast<Component*>();
            if (nativeComponent) {
                nativeComponent->owner = this->owner;
            }

            // Initialize hot reload paths when loading a scene
            m_scriptFilePath = "assets/scripts/" + m_scriptName + ".py";
            if (std::filesystem::exists(m_scriptFilePath)) {
                m_lastWriteTime = std::filesystem::last_write_time(m_scriptFilePath);
            }
        } catch (const py::error_already_set& e) {
            std::cerr << "[ScriptComponent] Deserialize Instantiation Error:\n" << e.what() << std::endl;
            return;
        }
    }

    // Automatically restore all saved variables into the Python instance
    if (j.contains("properties") && m_instance) {
        auto props = j["properties"];
        
        for (auto& el : props.items()) {
            std::string key = el.key();
            try {
                // We must check the CURRENT type of the variable in the active Python script 
                // to know if it's an Enum. This prevents replacing an Enum with a raw string!
                py::object currentValue = m_instance.attr(key.c_str());
                py::object cls = currentValue.attr("__class__");

                // Reconstruct the Python Enum instance safely
                if (py::hasattr(cls, "__members__") && el.value().is_string()) {
                    std::string enumName = el.value().get<std::string>();
                    
                    // This dynamically invokes cls.UP, returning the true Enum instance
                    m_instance.attr(key.c_str()) = cls.attr(enumName.c_str());
                }
                // Determine the standard JSON types
                else if (el.value().is_boolean()) {
                    m_instance.attr(key.c_str()) = el.value().get<bool>();
                } 
                else if (el.value().is_number_float()) {
                    m_instance.attr(key.c_str()) = el.value().get<float>();
                } 
                else if (el.value().is_number_integer()) {
                    m_instance.attr(key.c_str()) = el.value().get<int>();
                } 
                else if (el.value().is_string()) {
                    m_instance.attr(key.c_str()) = el.value().get<std::string>();
                }
            } catch (const py::error_already_set& e) {
                std::cerr << "[ScriptComponent] Failed to restore property '" << key << "':\n" << e.what() << std::endl;
            }
        }
    }
}
