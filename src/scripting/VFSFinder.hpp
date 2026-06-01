#pragma once

#include "core/assets/AssetRegistry.hpp"
#include <pybind11/pybind11.h>
#include <pybind11/eval.h>
#include <algorithm>
#include "VFSLoader.hpp"

namespace py = pybind11;

// The Finder: Responsible for intercepting the 'import' statement
class VFSFinder {
public:
    py::object find_spec(const std::string& fullname, py::object /*path*/, py::object /*target*/) {
        // Convert Python module name (e.g. 'player.controller') to file path ('player/controller.py')
        std::string filepath = fullname;
        std::replace(filepath.begin(), filepath.end(), '.', '/');
        filepath = "assets/scripts/" + filepath + ".py";

        // Check our Virtual File System
        if (AssetRegistry::IsPacked()) {
            std::vector<unsigned char> data = AssetRegistry::GetFileData(filepath);
            
            if (!data.empty()) {
                std::string code(data.begin(), data.end());
                
                // Instantiate our C++ Loader
                VFSLoader loader(code, filepath);
                
                // Import Python's built-in ModuleSpec class
                py::module_ machinery = py::module_::import("importlib.machinery");
                
                // Return a ModuleSpec object telling Python to use our C++ Loader
                return machinery.attr("ModuleSpec")(
                    fullname, 
                    loader, 
                    py::arg("origin") = filepath
                );
            }
        }
        
        // Return None to let Python continue its normal search (standard libraries, etc.)
        return py::none(); 
    }
};
