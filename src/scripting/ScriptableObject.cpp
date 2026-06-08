#include "ScriptableObject.hpp"
#include "scripting/DataManager.hpp"
#include "core/assets/AssetRegistry.hpp" // Indispensable pour la résolution des UUID
#include <iostream>
#include <map>
#include <vector>
#include <algorithm>

#ifndef STANDALONE_MODE
#include <imgui.h>
#endif

#ifndef STANDALONE_MODE
void ScriptableObject::OnInspector() {
    char nameBuffer[256];
    strncpy(nameBuffer, name.c_str(), sizeof(nameBuffer));
    if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
        name = nameBuffer;
    }

    char idBuffer[256];
    strncpy(idBuffer, assetId.c_str(), sizeof(idBuffer));
    if (ImGui::InputText("Asset ID", idBuffer, sizeof(idBuffer))) {
        assetId = idBuffer;
    }

    ImGui::Separator();
    ImGui::TextDisabled("Python Custom Properties");
    ImGui::Spacing();

    try {
        py::object pyInstance = py::cast(this);
        if (!py::hasattr(pyInstance, "__dict__")) return;
        
        py::dict attributes = pyInstance.attr("__dict__");

        // --- Fetch Type Annotations ---
        py::dict annotations;
        if (py::hasattr(pyInstance.attr("__class__"), "__annotations__")) {
            annotations = pyInstance.attr("__class__").attr("__annotations__");
        }

        // --- Metadata Extraction ---
        std::map<std::string, std::string> tooltips;
        if (py::hasattr(pyInstance, "__tooltips__")) {
            try { py::dict pyTooltips = pyInstance.attr("__tooltips__");
                  for (auto item : pyTooltips) tooltips[py::str(item.first)] = py::str(item.second); } catch (...) {}
        }

        std::map<std::string, std::pair<float, float>> ranges;
        if (py::hasattr(pyInstance, "__ranges__")) {
            try { py::dict pyRanges = pyInstance.attr("__ranges__");
                  for (auto item : pyRanges) {
                      py::tuple limits = py::cast<py::tuple>(item.second);
                      if (py::len(limits) == 2) ranges[py::str(item.first)] = { limits[0].cast<float>(), limits[1].cast<float>() };
                  }
            } catch (...) {}
        }

        std::vector<std::string> multilineVars;
        if (py::hasattr(pyInstance, "__multiline__")) {
            try { py::list pyMultiline = pyInstance.attr("__multiline__");
                  for (auto item : pyMultiline) multilineVars.push_back(py::str(item)); } catch (...) {}
        }

        std::vector<std::string> readonlyVars;
        if (py::hasattr(pyInstance, "__readonly__")) {
            try { py::list pyReadonly = pyInstance.attr("__readonly__");
                  for (auto item : pyReadonly) readonlyVars.push_back(py::str(item)); } catch (...) {}
        }

        std::vector<std::string> actionMethods;
        if (py::hasattr(pyInstance, "__actions__")) {
            try { py::list pyActions = pyInstance.attr("__actions__");
                  for (auto item : pyActions) actionMethods.push_back(py::str(item)); } catch (...) {}
        }

        // --- Helper Lambda for Drawing ---
        auto DrawPythonVariable = [&](const std::string& key, py::object value) {
            bool isReadonly = std::find(readonlyVars.begin(), readonlyVars.end(), key) != readonlyVars.end();
            if (isReadonly) ImGui::BeginDisabled();

            py::object cls = value.attr("__class__");
            
            // 1. Analyze if this is an Asset Reference
            bool isAsset = false;
            std::string expectedClass = "";

            if (!value.is_none() && py::hasattr(value, "asset_id")) {
                isAsset = true;
                expectedClass = py::str(cls.attr("__name__"));
            } 
            else if (value.is_none() && annotations.contains(key.c_str())) {
                std::string annStr = py::str(annotations[key.c_str()]);
                
                size_t pipePos = annStr.find('|');
                if (pipePos != std::string::npos) annStr = annStr.substr(0, pipePos);

                size_t startPos = annStr.find('[');
                size_t endPos = annStr.find(']');
                if (startPos != std::string::npos && endPos != std::string::npos) {
                    annStr = annStr.substr(startPos + 1, endPos - startPos - 1);
                }

                annStr.erase(0, annStr.find_first_not_of(" \t"));
                annStr.erase(annStr.find_last_not_of(" \t") + 1);

                size_t dotPos = annStr.find_last_of('.');
                if (dotPos != std::string::npos) annStr = annStr.substr(dotPos + 1);

                expectedClass = annStr;

                if (expectedClass != "int" && expectedClass != "float" && expectedClass != "bool" && expectedClass != "str" && expectedClass != "NoneType") {
                    isAsset = true;
                }
            }

            // 2. Enums
            if (py::hasattr(cls, "__members__")) {
                std::string currentName = py::str(value.attr("name"));
                if (ImGui::BeginCombo(key.c_str(), currentName.c_str())) {
                    py::dict members = cls.attr("__members__");
                    for (auto item : members) {
                        std::string memberName = py::str(item.first);
                        bool isSelected = (currentName == memberName);
                        if (ImGui::Selectable(memberName.c_str(), isSelected)) {
                            pyInstance.attr(key.c_str()) = item.second;
                        }
                        if (isSelected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            }
            // 3. Asset Drag & Drop Zone
            else if (isAsset) {
                std::string displayStr = "None";
                if (!value.is_none() && py::hasattr(value, "name")) {
                    displayStr = py::str(value.attr("name")); 
                }

                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.25f, 0.35f, 1.0f));
                ImGui::InputText((key + " (" + expectedClass + ")").c_str(), displayStr.data(), displayStr.size(), ImGuiInputTextFlags_ReadOnly);
                ImGui::PopStyleColor();

                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                        std::string droppedPath = (const char*)payload->Data;
                        
                        if (droppedPath.find(".asset") != std::string::npos) {
                            try {
                                py::object loadedAsset = DataManager::LoadAsset(droppedPath);
                                
                                if (!loadedAsset.is_none()) {
                                    std::string droppedClass = py::str(loadedAsset.attr("__class__").attr("__name__"));
                                    
                                    if (droppedClass == expectedClass || expectedClass.empty() || expectedClass == "ScriptableObject") {
                                        // Resolution of the UUID through the central registry
                                        UUID assetUUID = AssetRegistry::GetUUIDForPath(droppedPath);
                                        
                                        // Store the UUID directly inside the Python object metadata
                                        loadedAsset.attr("__asset_uuid__") = (uint64_t)assetUUID;
                                        
                                        pyInstance.attr(key.c_str()) = loadedAsset;
                                        std::cout << "[Editor] Linked Asset: " << droppedClass << " (UUID: " << (uint64_t)assetUUID << ")" << std::endl;
                                    } else {
                                        std::cerr << "[Editor] Asset Type mismatch! Expected " << expectedClass << std::endl;
                                    }
                                }
                            } catch (const std::exception& e) {
                                std::cerr << "[Editor] Failed to load dropped asset: " << e.what() << std::endl;
                            }
                        }
                    }
                    ImGui::EndDragDropTarget();
                }
            }
            // 4. Floats
            else if (py::isinstance<py::float_>(value)) {
                float v = value.cast<float>();
                float min = 0.0f, max = 0.0f;
                bool hasRange = ranges.contains(key);
                if (hasRange) { min = ranges.at(key).first; max = ranges.at(key).second; }
                
                if (ImGui::DragFloat(key.c_str(), &v, 0.1f, min, max)) {
                    if (hasRange) v = std::clamp(v, min, max);
                    pyInstance.attr(key.c_str()) = v;
                }
            }
            // 5. Booleans
            else if (py::isinstance<py::bool_>(value)) {
                bool v = value.cast<bool>();
                if (ImGui::Checkbox(key.c_str(), &v)) pyInstance.attr(key.c_str()) = v;
            }
            // 6. Integers
            else if (py::isinstance<py::int_>(value)) {
                int v = value.cast<int>();
                int min = 0, max = 0;
                bool hasRange = ranges.contains(key);
                if (hasRange) { min = static_cast<int>(ranges.at(key).first); max = static_cast<int>(ranges.at(key).second); }

                if (ImGui::DragInt(key.c_str(), &v, 1.0f, min, max)) {
                    if (hasRange) v = std::clamp(v, min, max);
                    pyInstance.attr(key.c_str()) = v;
                }
            }
            // 7. Strings
            else if (py::isinstance<py::str>(value)) {
                std::string v = value.cast<std::string>();
                char buffer[2048];
                strncpy(buffer, v.c_str(), sizeof(buffer));
                buffer[sizeof(buffer) - 1] = '\0';

                bool isMultiline = std::find(multilineVars.begin(), multilineVars.end(), key) != multilineVars.end();

                if (isMultiline) {
                    if (ImGui::InputTextMultiline(key.c_str(), buffer, sizeof(buffer), ImVec2(0, ImGui::GetTextLineHeight() * 4.2f))) {
                        pyInstance.attr(key.c_str()) = std::string(buffer);
                    }
                } else {
                    if (ImGui::InputText(key.c_str(), buffer, sizeof(buffer))) pyInstance.attr(key.c_str()) = std::string(buffer);
                }
            } else {
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

            if (isReadonly) ImGui::EndDisabled();
        };

        // --- Categories Logic ---
        std::map<std::string, std::vector<std::string>> categories;
        std::vector<std::string> categorizedVars;

        if (py::hasattr(pyInstance, "__categories__")) {
            try { py::dict pyCategories = pyInstance.attr("__categories__");
                  for (auto item : pyCategories) {
                      std::string catName = py::str(item.first);
                      py::list varsList = py::cast<py::list>(item.second);
                      for (auto var : varsList) {
                          std::string varName = py::str(var);
                          categories[catName].push_back(varName);
                          categorizedVars.push_back(varName);
                      }
                  }
            } catch (...) {}
        }

        for (const auto& [catName, vars] : categories) {
            if (ImGui::TreeNodeEx(catName.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                for (const auto& varName : vars) {
                    if (attributes.contains(varName.c_str())) {
                        py::object varValue = py::reinterpret_borrow<py::object>(attributes[varName.c_str()]);
                        DrawPythonVariable(varName, varValue);
                    } else {
                        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Warning: '%s' not found", varName.c_str());
                    }
                }
                ImGui::TreePop();
            }
        }

        bool hasUncategorized = false;
        for (auto item : attributes) {
            std::string key = py::str(item.first);
            if (key.rfind("_", 0) == 0 || key == "asset_id" || key == "name") continue;
            if (std::find(categorizedVars.begin(), categorizedVars.end(), key) == categorizedVars.end()) {
                hasUncategorized = true; break;
            }
        }

        if (hasUncategorized && !categories.empty()) { ImGui::Separator(); ImGui::TextDisabled("Uncategorized"); }

        for (auto item : attributes) {
            std::string key = py::str(item.first);
            if (key.rfind("_", 0) == 0 || key == "asset_id" || key == "name") continue;
            if (std::find(categorizedVars.begin(), categorizedVars.end(), key) == categorizedVars.end()) {
                py::object value = py::reinterpret_borrow<py::object>(item.second);
                DrawPythonVariable(key, value);
            }
        }

        if (!actionMethods.empty()) {
            ImGui::Separator(); ImGui::TextDisabled("Asset Actions");
            for (const auto& methodName : actionMethods) {
                if (ImGui::Button(methodName.c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                    try { if (py::hasattr(pyInstance, methodName.c_str())) pyInstance.attr(methodName.c_str())(); }
                    catch (const py::error_already_set& e) { std::cerr << "Action Error:\n" << e.what() << std::endl; }
                }
            }
        }

    } 
    catch (const py::error_already_set& e) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Python Introspection Error:");
        ImGui::TextWrapped("%s", e.what());
    }
}
#endif

nlohmann::json ScriptableObject::Serialize() const {
    nlohmann::json j;
    j["assetId"] = assetId;
    j["name"] = name;
    j["scriptName"] = scriptName;
    j["className"] = className;

    nlohmann::json properties;

    try {
        py::object pyInstance = py::cast(this);
        if (py::hasattr(pyInstance, "__dict__")) {
            py::dict attributes = pyInstance.attr("__dict__");

            for (auto item : attributes) {
                std::string key = py::str(item.first);
                if (key.rfind("_", 0) == 0 || key == "asset_id" || key == "name") continue;

                py::object value = py::reinterpret_borrow<py::object>(item.second);
                
                // --- Save Asset References via UUID ---
                if (!value.is_none() && py::hasattr(value, "asset_id")) {
                    nlohmann::json assetRef;
                    assetRef["__is_asset__"] = true;
                    assetRef["assetId"] = py::str(value.attr("asset_id")).cast<std::string>();
                    
                    if (py::hasattr(value, "__asset_uuid__")) {
                        assetRef["assetUUID"] = value.attr("__asset_uuid__").cast<uint64_t>();
                    } else {
                        assetRef["assetUUID"] = 0; 
                    }
                    
                    properties[key] = assetRef;
                    continue; 
                }

                py::object cls = value.attr("__class__");

                if (py::hasattr(cls, "__members__")) properties[key] = py::str(value.attr("name")).cast<std::string>();
                else if (py::isinstance<py::bool_>(value)) properties[key] = value.cast<bool>();
                else if (py::isinstance<py::float_>(value)) properties[key] = value.cast<float>();
                else if (py::isinstance<py::int_>(value)) properties[key] = value.cast<int>();
                else if (py::isinstance<py::str>(value)) properties[key] = value.cast<std::string>();
            }
        }
    } catch (const py::error_already_set& e) {
        std::cerr << "[ScriptableObject] Auto-Serialize Error:\n" << e.what() << std::endl;
    }

    j["properties"] = properties;
    return j;
}

void ScriptableObject::Deserialize(const nlohmann::json& j) {
    if (j.contains("assetId")) assetId = j["assetId"].get<std::string>();
    if (j.contains("name")) name = j["name"].get<std::string>();
    if (j.contains("scriptName")) scriptName = j["scriptName"].get<std::string>();
    if (j.contains("className")) className = j["className"].get<std::string>();

    if (j.contains("properties")) {
        auto props = j["properties"];
        try {
            py::object pyInstance = py::cast(this);
            
            for (auto& el : props.items()) {
                std::string key = el.key();
                
                // --- Restore Asset References via UUID ---
                if (el.value().is_object() && el.value().contains("__is_asset__")) {
                    uint64_t savedUUID = el.value().value("assetUUID", 0ULL);
                    std::string resolvedPath = "";
                    
                    // Priorité au UUID pour trouver le fichier, peu importe où il a été déplacé
                    if (savedUUID != 0) {
                        resolvedPath = AssetRegistry::GetPathForUUID(savedUUID).string();
                    } 
                    // Fallback de sécurité si le UUID est introuvable ou si le fichier est un ancien format
                    else if (el.value().contains("path")) {
                        resolvedPath = el.value().value("path", "");
                    }

                    if (!resolvedPath.empty()) {
                        try {
                            py::object loadedAsset = DataManager::LoadAsset(resolvedPath);
                            if (!loadedAsset.is_none()) {
                                // On s'assure de conserver le UUID sur l'objet pour la prochaine sauvegarde
                                if (savedUUID == 0) {
                                    savedUUID = AssetRegistry::GetUUIDForPath(resolvedPath);
                                }
                                loadedAsset.attr("__asset_uuid__") = savedUUID;
                                pyInstance.attr(key.c_str()) = loadedAsset;
                            }
                        } catch (...) {
                            std::cerr << "[Editor] Warning: Missing linked asset at " << resolvedPath << std::endl;
                        }
                    }
                    continue;
                }

                // Normal Deserialization
                py::object currentValue = pyInstance.attr(key.c_str());
                py::object cls = currentValue.attr("__class__");

                if (py::hasattr(cls, "__members__") && el.value().is_string()) {
                    std::string enumName = el.value().get<std::string>();
                    pyInstance.attr(key.c_str()) = cls.attr(enumName.c_str());
                }
                else if (el.value().is_boolean()) pyInstance.attr(key.c_str()) = el.value().get<bool>();
                else if (el.value().is_number_float()) pyInstance.attr(key.c_str()) = el.value().get<float>();
                else if (el.value().is_number_integer()) pyInstance.attr(key.c_str()) = el.value().get<int>();
                else if (el.value().is_string()) pyInstance.attr(key.c_str()) = el.value().get<std::string>();
            }
        } catch (const py::error_already_set& e) {
            std::cerr << "[ScriptableObject] Deserialize Error:\n" << e.what() << std::endl;
        }
    }
}
