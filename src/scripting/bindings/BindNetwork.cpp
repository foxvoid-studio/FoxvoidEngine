#include "scripting/ScriptBindings.hpp"
#include <iostream>
#include <memory> 
#include <pybind11/stl.h>
#include <pybind11/functional.h> 

#include "cloud/CloudManager.hpp"
#include <network/HttpClient.hpp>
#include <scripting/ScriptableObject.hpp>
#include <cloud/CloudItem.hpp>

// ==========================================
// ASYNC PYTHON CALLBACK MANAGER (BULLETPROOF)
// ==========================================

// The GIL MUST be locked even in WebAssembly. 
// Otherwise, Pybind11 will corrupt memory when managing object reference counts.
#define SAFE_GIL_ACQUIRE() py::gil_scoped_acquire acquire;

#define EXECUTE_PYTHON_CALLBACK(code_block) \
    try { \
        SAFE_GIL_ACQUIRE() \
        code_block \
    } catch (const py::error_already_set& e) { \
        std::cerr << "[Python Async Error] " << e.what() << std::endl; \
    } catch (const std::exception& e) { \
        std::cerr << "[C++ Async Error] " << e.what() << std::endl; \
    }

struct PyAsyncContext {
    py::object onSuccess;
    py::object onError;
    py::object extra; 

    PyAsyncContext(py::object success, py::object error, py::object ex = py::none()) 
        : onSuccess(success), onError(error), extra(ex) {}

    ~PyAsyncContext() {
        // C++ destructors are implicitly noexcept. 
        // Emscripten destroying C++ lambdas from the JS event loop can cause Pybind11 
        // to throw exceptions if the Python thread state is lost or corrupted.
        // We MUST wrap GIL acquisition and object destruction in a try-catch to prevent std::terminate.
        try {
            SAFE_GIL_ACQUIRE()
            onSuccess = py::object(); 
            onError = py::object();
            extra = py::object();
        } catch (const std::exception& e) {
            std::cerr << "[PyAsyncContext] Safely caught exception during memory cleanup: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "[PyAsyncContext] Safely caught unknown exception during cleanup." << std::endl;
        }
    }
};

// ==========================================
// JSON TO PYTHON CONVERSION HELPER
// ==========================================
// This recursive function converts a C++ nlohmann::json object 
// into its exact Python equivalent (py::dict, py::list, primitives).
// It ensures nested dictionaries (like 'icon' in custom_data) are preserved!
py::object JsonToPyObject(const nlohmann::json& j) {
    if (j.is_null()) {
        return py::none();
    } 
    else if (j.is_boolean()) {
        return py::bool_(j.get<bool>());
    } 
    else if (j.is_number_integer()) {
        return py::int_(j.get<long>()); // Use long for safety with large IDs
    } 
    else if (j.is_number_float()) {
        return py::float_(j.get<double>());
    } 
    else if (j.is_string()) {
        return py::str(j.get<std::string>());
    } 
    else if (j.is_array()) {
        py::list pyList;
        for (const auto& item : j) {
            // Recursively convert each item in the array
            pyList.append(JsonToPyObject(item));
        }
        return pyList;
    } 
    else if (j.is_object()) {
        py::dict pyDict;
        for (const auto& [key, value] : j.items()) {
            // Recursively convert each value in the dictionary
            pyDict[py::str(key)] = JsonToPyObject(value);
        }
        return pyDict;
    }
    
    // Fallback if type is unrecognized (should never happen with standard JSON)
    return py::none();
}

void BindNetwork(py::module_& m) {
    py::class_<HttpResponse>(m, "HttpResponse")
        .def_readonly("status_code", &HttpResponse::statusCode)
        .def_readonly("body", &HttpResponse::body);

    py::class_<HttpClient>(m, "HttpClient")
        .def_static("get", [](const std::string& url, py::dict pyHeaders, py::object onSuccess, py::object onError) {
            try {
                auto ctx = std::make_shared<PyAsyncContext>(onSuccess, onError);
                std::unordered_map<std::string, std::string> headers;
                for (auto item : pyHeaders) {
                    headers[py::cast<std::string>(item.first)] = py::cast<std::string>(item.second);
                }

                HttpClient::Get(url, headers, 
                    [ctx](const HttpResponse& response) {
                        EXECUTE_PYTHON_CALLBACK({
                            if (!ctx->onSuccess.is_none()) { ctx->onSuccess(response); }
                        })
                    },
                    [ctx](const std::string& err) {
                        EXECUTE_PYTHON_CALLBACK({
                            if (!ctx->onError.is_none()) { ctx->onError(err); } 
                            else { std::cerr << "[HttpClient] GET Error: " << err << std::endl; }
                        })
                    }
                );
            } catch (const std::exception& e) { std::cerr << "[HttpClient] Fatal Sync Error in GET: " << e.what() << std::endl; }
        }, py::arg("url"), py::arg("headers") = py::dict(), py::arg("on_success") = py::none(), py::arg("on_error") = py::none())
        
        .def_static("post", [](const std::string& url, py::dict pyHeaders, const std::string& body, py::object onSuccess, py::object onError) {
            try {
                auto ctx = std::make_shared<PyAsyncContext>(onSuccess, onError);
                std::unordered_map<std::string, std::string> headers;
                for (auto item : pyHeaders) {
                    headers[py::cast<std::string>(item.first)] = py::cast<std::string>(item.second);
                }

                HttpClient::Post(url, headers, body,
                    [ctx](const HttpResponse& response) {
                        EXECUTE_PYTHON_CALLBACK({
                            if (!ctx->onSuccess.is_none()) { ctx->onSuccess(response); }
                        })
                    },
                    [ctx](const std::string& err) {
                        EXECUTE_PYTHON_CALLBACK({
                            if (!ctx->onError.is_none()) { ctx->onError(err); } 
                            else { std::cerr << "[HttpClient] POST Error: " << err << std::endl; }
                        })
                    }
                );
            } catch (const std::exception& e) { std::cerr << "[HttpClient] Fatal Sync Error in POST: " << e.what() << std::endl; }
        }, py::arg("url"), py::arg("headers") = py::dict(), py::arg("body") = "", py::arg("on_success") = py::none(), py::arg("on_error") = py::none());

    py::class_<CloudItem, PyCloudItem>(m, "CloudItem", py::dynamic_attr())
        .def(py::init<>())
        .def_readwrite("item_id", &CloudItem::itemId)
        .def_readwrite("name", &CloudItem::name)
        .def_readwrite("quantity", &CloudItem::quantity)
        .def_readwrite("is_active", &CloudItem::isActive)
        .def("on_deserialized", &CloudItem::OnDeserialized);

    py::class_<CloudManager>(m, "CloudManager")
        .def_static("is_authenticated", &CloudManager::IsAuthenticated)
        
        .def_static("pull_save", [](const std::string& key, py::object onSuccess, py::object onError) {
            try {
                auto ctx = std::make_shared<PyAsyncContext>(onSuccess, onError);
                CloudManager::PullSave(key, 
                    [ctx](const nlohmann::json& data) {
                        EXECUTE_PYTHON_CALLBACK({
                            if (!ctx->onSuccess.is_none()) {
                                py::module_ jsonMod = py::module_::import("json");
                                py::object pyDict = jsonMod.attr("loads")(data.dump());
                                ctx->onSuccess(pyDict);
                            }
                        })
                    }, 
                    [ctx](const std::string& err) {
                        EXECUTE_PYTHON_CALLBACK({
                            if (!ctx->onError.is_none()) { ctx->onError(err); } 
                            else { std::cerr << "[Cloud] Pull Save Error: " << err << std::endl; }
                        })
                    }
                );
            } catch (const std::exception& e) { std::cerr << "[Cloud] Fatal Sync Error in pull_save: " << e.what() << std::endl; }
        }, py::arg("key"), py::arg("on_success"), py::arg("on_error") = py::none())
        
        .def_static("push_save", [](const std::string& key, py::dict pyData, py::object onSuccess, py::object onError) {
            try {
                auto ctx = std::make_shared<PyAsyncContext>(onSuccess, onError);
                nlohmann::json cppData;
                try {
                    py::module_ jsonMod = py::module_::import("json");
                    std::string jsonStr = py::cast<std::string>(jsonMod.attr("dumps")(pyData));
                    cppData = nlohmann::json::parse(jsonStr);
                } catch (const std::exception& e) {
                    EXECUTE_PYTHON_CALLBACK({
                        if (!ctx->onError.is_none()) { ctx->onError(std::string("Serialization error: ") + e.what()); }
                    })
                    return;
                }

                CloudManager::PushSave(key, cppData, 
                    [ctx](const nlohmann::json& responseData) {
                        EXECUTE_PYTHON_CALLBACK({
                            if (!ctx->onSuccess.is_none()) {
                                py::module_ jsonMod = py::module_::import("json");
                                py::object pyDict = jsonMod.attr("loads")(responseData.dump());
                                ctx->onSuccess(pyDict);
                            }
                        })
                    },
                    [ctx](const std::string& err) {
                        EXECUTE_PYTHON_CALLBACK({
                            if (!ctx->onError.is_none()) { ctx->onError(err); } 
                            else { std::cerr << "[Cloud] Push Save Error: " << err << std::endl; }
                        })
                    }
                );
            } catch (const std::exception& e) { std::cerr << "[Cloud] Fatal Sync Error in push_save: " << e.what() << std::endl; }
        }, py::arg("key"), py::arg("data"), py::arg("on_success") = py::none(), py::arg("on_error") = py::none())

        .def_static("push_scriptable_object", [](const std::string& key, ScriptableObject* obj, py::object onSuccess, py::object onError) {
            try {
                auto ctx = std::make_shared<PyAsyncContext>(onSuccess, onError);
                if (!obj) {
                    EXECUTE_PYTHON_CALLBACK({
                        if (!ctx->onError.is_none()) { ctx->onError(std::string("Cannot push a null ScriptableObject.")); }
                    })
                    return;
                }

                nlohmann::json cppData = obj->Serialize();

                CloudManager::PushSave(key, cppData, 
                    [ctx](const nlohmann::json& responseData) {
                        EXECUTE_PYTHON_CALLBACK({
                            if (!ctx->onSuccess.is_none()) {
                                py::module_ jsonMod = py::module_::import("json");
                                py::object pyDict = jsonMod.attr("loads")(responseData.dump());
                                ctx->onSuccess(pyDict);
                            }
                        })
                    },
                    [ctx](const std::string& err) {
                        EXECUTE_PYTHON_CALLBACK({
                            if (!ctx->onError.is_none()) { ctx->onError(err); }
                        })
                    }
                );
            } catch (const std::exception& e) { std::cerr << "[Cloud] Fatal Sync Error in push_scriptable_object: " << e.what() << std::endl; }
        }, py::arg("key"), py::arg("obj"), py::arg("on_success") = py::none(), py::arg("on_error") = py::none())
        
        .def_static("pull_scriptable_object", [](const std::string& key, py::object cls, py::object onSuccess, py::object onError) {
            try {
                auto ctx = std::make_shared<PyAsyncContext>(onSuccess, onError, cls);
                
                CloudManager::PullSave(key, 
                    [ctx](const nlohmann::json& data) {
                        EXECUTE_PYTHON_CALLBACK({
                            if (!ctx->onSuccess.is_none()) {
                                py::object pyInstance = ctx->extra()(); 
                                ScriptableObject* cppInstance = pyInstance.cast<ScriptableObject*>();
                                if (cppInstance) {
                                    if (data.contains("data") && data["data"].is_object()) {
                                        cppInstance->Deserialize(data["data"]);
                                    } else {
                                        cppInstance->Deserialize(data);
                                    }
                                }
                                ctx->onSuccess(pyInstance);
                            }
                        })
                    }, 
                    [ctx](const std::string& err) {
                        EXECUTE_PYTHON_CALLBACK({
                            if (!ctx->onError.is_none()) { ctx->onError(err); }
                        })
                    }
                );
            } catch (const std::exception& e) { std::cerr << "[Cloud] Synchronous Exception Caught in pull_scriptable_object: " << e.what() << std::endl; }
        }, py::arg("key"), py::arg("cls"), py::arg("on_success"), py::arg("on_error") = py::none())

        .def_static("pull_into_scriptable_object", [](const std::string& key, ScriptableObject* obj, py::object onSuccess, py::object onError) {
            try {
                auto ctx = std::make_shared<PyAsyncContext>(onSuccess, onError);
                if (!obj) {
                    EXECUTE_PYTHON_CALLBACK({
                        if (!ctx->onError.is_none()) { ctx->onError(std::string("Target ScriptableObject is null.")); }
                    })
                    return;
                }

                CloudManager::PullSave(key, 
                    [obj, ctx](const nlohmann::json& data) {
                        EXECUTE_PYTHON_CALLBACK({
                            if (data.contains("data") && data["data"].is_object()) {
                                obj->Deserialize(data["data"]);
                            } else {
                                obj->Deserialize(data);
                            }

                            if (!ctx->onSuccess.is_none()) {
                                ctx->onSuccess();
                            }
                        })
                    }, 
                    [ctx](const std::string& err) {
                        EXECUTE_PYTHON_CALLBACK({
                            if (!ctx->onError.is_none()) { ctx->onError(err); }
                        })
                    }
                );
            } catch (const std::exception& e) { std::cerr << "[Cloud] Fatal Sync Error in pull_into_scriptable_object: " << e.what() << std::endl; }
        }, py::arg("key"), py::arg("obj"), py::arg("on_success") = py::none(), py::arg("on_error") = py::none())

        .def_static("pull_inventory", [](py::object cls, py::object onSuccess, py::object onError) {
            try {
                auto ctx = std::make_shared<PyAsyncContext>(onSuccess, onError, cls);
                
                CloudManager::PullInventory(
                    [ctx](const nlohmann::json& data) {
                        EXECUTE_PYTHON_CALLBACK({
                            if (!ctx->onSuccess.is_none()) {
                                py::list resultList;
                                
                                if (!data.is_array()) {
                                    std::cerr << "[Cloud] pull_inventory API response is not a valid JSON array." << std::endl;
                                    ctx->onSuccess(resultList); 
                                    return;
                                }

                                py::object itemClass = ctx->extra; 
                                std::string targetCategory = "";
                                
                                // Extract the target category string from the Python class definition
                                if (py::hasattr(itemClass, "category")) {
                                    targetCategory = py::cast<std::string>(py::str(itemClass.attr("category")));
                                }
                                
                                for (const auto& itemData : data) {
                                    if (!itemData.is_object()) continue; 

                                    std::string itemCategory = itemData.value("category", "");
                                    
                                    // If the class defines a category, we only instantiate matching items
                                    if (targetCategory.empty() || itemCategory == targetCategory) {
                                        
                                        // Instantiate the Python class dynamically (e.g. PlaneSkin())
                                        py::object instance = itemClass();
                                        
                                        // Set attributes directly via Python to bypass C++ RTTI casting errors in WASM
                                        instance.attr("item_id") = itemData.value("item_id", 0);
                                        instance.attr("name") = itemData.value("name", "");
                                        instance.attr("quantity") = itemData.value("quantity", 0);
                                        instance.attr("is_active") = itemData.value("is_active", false);
                                        
                                        py::dict customDataDict;
                                        if (itemData.contains("custom_data") && itemData["custom_data"].is_object()) {
                                            // The helper returns a py::object, we cast it to a py::dict
                                            customDataDict = JsonToPyObject(itemData["custom_data"]).cast<py::dict>();
                                        }
                                        
                                        // Assign the fully populated dictionary to the Python instance
                                        instance.attr("custom_data") = customDataDict; 
                                        
                                        // Trigger Python's on_deserialized dynamically so the user 
                                        // can safely extract their specific fields (like icon_id)
                                        if (py::hasattr(instance, "on_deserialized")) {
                                            instance.attr("on_deserialized")();
                                        }
                                        
                                        resultList.append(instance);
                                    }
                                }
                                ctx->onSuccess(resultList);
                            }
                        })
                    }, 
                    [ctx](const std::string& err) {
                        EXECUTE_PYTHON_CALLBACK({
                            if (!ctx->onError.is_none()) { ctx->onError(err); } 
                        })
                    }
                );
            } catch (const std::exception& e) { std::cerr << "[Cloud] Fatal Sync Error in pull_inventory: " << e.what() << std::endl; }
        }, py::arg("cls"), py::arg("on_success"), py::arg("on_error") = py::none())

        .def_static("equip_item", [](int itemId, const std::string& category, bool disableAll, py::object onSuccess, py::object onError) {
            try {
                auto ctx = std::make_shared<PyAsyncContext>(onSuccess, onError);
                
                CloudManager::EquipItem(itemId, category, disableAll,
                    [ctx](const nlohmann::json& responseData) {
                        EXECUTE_PYTHON_CALLBACK({
                            if (!ctx->onSuccess.is_none()) {
                                ctx->onSuccess();
                            }
                        })
                    },
                    [ctx](const std::string& err) {
                        EXECUTE_PYTHON_CALLBACK({
                            if (!ctx->onError.is_none()) { ctx->onError(err); }
                        })
                    }
                );
            } catch (const std::exception& e) { 
                std::cerr << "[Cloud] Fatal Sync Error in equip_item: " << e.what() << std::endl; 
            }
        }, py::arg("item_id"), py::arg("category"), py::arg("disable_all") = true, py::arg("on_success") = py::none(), py::arg("on_error") = py::none());
}
