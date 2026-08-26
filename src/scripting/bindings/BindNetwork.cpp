#include "scripting/ScriptBindings.hpp"
#include <iostream>
#include <pybind11/stl.h>
#include <pybind11/functional.h> // Required for passing lambdas

#include "cloud/CloudManager.hpp"
#include <network/HttpClient.hpp>

void BindNetwork(py::module_& m) {
    // HTTP client bindings
    py::class_<HttpResponse>(m, "HttpResponse")
        .def_readonly("status_code", &HttpResponse::statusCode)
        .def_readonly("body", &HttpResponse::body);

    py::class_<HttpClient>(m, "HttpClient")
        .def_static("get", [](const std::string& url, py::dict pyHeaders, py::object onSuccess, py::object onError) {
            
            // Convert Python dictionary to C++ std::unordered_map
            std::unordered_map<std::string, std::string> headers;
            for (auto item : pyHeaders) {
                headers[py::cast<std::string>(item.first)] = py::cast<std::string>(item.second);
            }

            HttpClient::Get(url, headers, 
                [onSuccess](const HttpResponse& response) {
                    if (!onSuccess.is_none()) {
                        py::gil_scoped_acquire acquire;
                        onSuccess(response); // Pybind11 automatically converts HttpResponse to Python object
                    }
                },
                [onError](const std::string& err) {
                    if (!onError.is_none()) {
                        py::gil_scoped_acquire acquire;
                        onError(err);
                    } else {
                        std::cerr << "[HttpClient] GET Error: " << err << std::endl;
                    }
                }
            );
        }, py::arg("url"), py::arg("headers") = py::dict(), py::arg("on_success") = py::none(), py::arg("on_error") = py::none())
        
        .def_static("post", [](const std::string& url, py::dict pyHeaders, const std::string& body, py::object onSuccess, py::object onError) {
            
            std::unordered_map<std::string, std::string> headers;
            for (auto item : pyHeaders) {
                headers[py::cast<std::string>(item.first)] = py::cast<std::string>(item.second);
            }

            HttpClient::Post(url, headers, body,
                [onSuccess](const HttpResponse& response) {
                    if (!onSuccess.is_none()) {
                        py::gil_scoped_acquire acquire;
                        onSuccess(response);
                    }
                },
                [onError](const std::string& err) {
                    if (!onError.is_none()) {
                        py::gil_scoped_acquire acquire;
                        onError(err);
                    } else {
                        std::cerr << "[HttpClient] POST Error: " << err << std::endl;
                    }
                }
            );
        }, py::arg("url"), py::arg("headers") = py::dict(), py::arg("body") = "", py::arg("on_success") = py::none(), py::arg("on_error") = py::none());

    // Cloud manager bindings
    py::class_<CloudManager>(m, "CloudManager")
        .def_static("is_authenticated", &CloudManager::IsAuthenticated)
        
        // PULL SAVE
        .def_static("pull_save", [](const std::string& key, py::object onSuccess, py::object onError) {
            CloudManager::PullSave(key, 
                // Success Callback (Runs asynchronously)
                [onSuccess](const nlohmann::json& data) {
                    if (!onSuccess.is_none()) {
                        // CRITICAL: Acquire the Python Global Interpreter Lock before executing Python code
                        // from a background thread (Desktop) or async callback (Wasm).
                        py::gil_scoped_acquire acquire;
                        
                        try {
                            // Convert C++ JSON to string, then parse it natively in Python
                            py::module_ jsonMod = py::module_::import("json");
                            py::object pyDict = jsonMod.attr("loads")(data.dump());
                            
                            // Execute the Python callback function
                            onSuccess(pyDict);
                        } catch (const std::exception& e) {
                            std::cerr << "[Python Bindings] Error parsing Cloud Save data: " << e.what() << std::endl;
                        }
                    }
                }, 
                // Error Callback (Runs asynchronously)
                [onError](const std::string& err) {
                    if (!onError.is_none()) {
                        py::gil_scoped_acquire acquire;
                        onError(err);
                    } else {
                        std::cerr << "[Cloud] Pull Save Error: " << err << std::endl;
                    }
                }
            );
        }, py::arg("key"), py::arg("on_success"), py::arg("on_error") = py::none())
        
        // PUSH SAVE
        .def_static("push_save", [](const std::string& key, py::dict pyData, py::object onSuccess, py::object onError) {
            
            nlohmann::json cppData;
            try {
                // Convert Python dict to JSON string, then parse it natively in C++
                py::module_ jsonMod = py::module_::import("json");
                std::string jsonStr = py::cast<std::string>(jsonMod.attr("dumps")(pyData));
                cppData = nlohmann::json::parse(jsonStr);
            } catch (const std::exception& e) {
                std::cerr << "[Python Bindings] Error converting Python dict to JSON: " << e.what() << std::endl;
                if (!onError.is_none()) {
                    onError(std::string("Serialization error: ") + e.what());
                }
                return;
            }

            CloudManager::PushSave(key, cppData, 
                // Success Callback
                [onSuccess](const nlohmann::json& responseData) {
                    if (!onSuccess.is_none()) {
                        py::gil_scoped_acquire acquire;
                        try {
                            py::module_ jsonMod = py::module_::import("json");
                            py::object pyDict = jsonMod.attr("loads")(responseData.dump());
                            onSuccess(pyDict);
                        } catch (const std::exception& e) {
                            std::cerr << "[Python Bindings] Error parsing Cloud Save response: " << e.what() << std::endl;
                        }
                    }
                },
                // Error Callback
                [onError](const std::string& err) {
                    if (!onError.is_none()) {
                        py::gil_scoped_acquire acquire;
                        onError(err);
                    } else {
                        std::cerr << "[Cloud] Push Save Error: " << err << std::endl;
                    }
                }
            );
        }, py::arg("key"), py::arg("data"), py::arg("on_success") = py::none(), py::arg("on_error") = py::none());
}
