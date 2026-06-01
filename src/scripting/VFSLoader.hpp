#pragma once

#include "core/assets/AssetRegistry.hpp"
#include <pybind11/pybind11.h>
#include <pybind11/eval.h>
#include <algorithm>

namespace py = pybind11;

// The Loader: Responsible for actually executing the code if found
class VFSLoader {
    std::string m_code;
    std::string m_filename;
public:
    VFSLoader(const std::string& code, const std::string& filename) 
        : m_code(code), m_filename(filename) {}

    // Tell Python to create the standard module object itself
    py::object create_module(py::object /*spec*/) {
        return py::none(); 
    }

    // Python passes the newly created module here for us to populate
    void exec_module(py::object module) {
        // Set the internal filename so error tracebacks point to the correct file
        module.attr("__file__") = m_filename;
        
        // Extract the module's namespace (variables, classes, functions)
        py::dict dict = module.attr("__dict__");
        
        // Execute the raw string of code directly inside the module's namespace!
        py::exec(m_code, dict);
    }
};
