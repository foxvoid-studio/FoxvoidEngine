#include "ScriptEngine.hpp"
#include "ScriptBindings.hpp"
#include <iostream>

PYBIND11_EMBEDDED_MODULE(foxvoid, m) {
    BindCore(m);
    BindInput(m);
    BindMathAndPhysics(m);
    BindGraphics(m);
    BindGUI(m);
    BindAudio(m);
    BindNetwork(m);
}

py::scoped_interpreter* ScriptEngine::s_interpreter = nullptr;

void ScriptEngine::Initialize() {
    // Prevent double initialization
    if (!s_interpreter) {
        try {
            // 1. Start the Python interpreter
            s_interpreter = new py::scoped_interpreter();
            
            // 2. Import the 'sys' module to configure the path
            py::module_ sys = py::module_::import("sys");
            
            // 3. Append our specific scripts folder to sys.path
            // This allows us to just say "import main" to load "assets/scripts/main.py"
            sys.attr("path").attr("append")("assets/scripts");

#ifdef STANDALONE_MODE
            // VFS FULL C++ LOADER
            // We import our own engine module
            py::module_ foxvoid = py::module_::import("foxvoid");
            
            // Instantiate the C++ VFSFinder
            py::object finder = foxvoid.attr("VFSFinder")();
            
            // Insert it at index 0 of sys.meta_path so it has highest priority
            sys.attr("meta_path").attr("insert")(0, finder);
            
            std::cout << "[ScriptEngine] Full C++ VFS Loader injected into Python." << std::endl;
#endif

            std::cout << "[ScriptEngine] Python (pybind11) Initialized." << std::endl;
        } catch (const py::error_already_set& e) {
            std::cerr << "[ScriptEngine] Failed to initialize Python:\n" << e.what() << std::endl;
        }
    }
}

void ScriptEngine::Shutdown() {
    if (s_interpreter) {
        // Deleting the scoped_interpreter safely finalizes Python
        delete s_interpreter;
        s_interpreter = nullptr;
        std::cout << "[ScriptEngine] Python Shutdown complete." << std::endl;
    }
}

void ScriptEngine::AddScriptPath(const std::filesystem::path& path) {
    try {
        py::module_ sys = py::module_::import("sys");
        // Append the absolute path as a string to Python's path list
        sys.attr("path").attr("append")(path.string());
        std::cout << "[ScriptEngine] Added to Python path: " << path.string() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[ScriptEngine] Failed to add script path: " << e.what() << std::endl;
    }
}

void ScriptEngine::LoadModule(const std::string& moduleName) {
    try {
        // Dynamically import the python module
        py::module_ mod = py::module_::import(moduleName.c_str());
        
        std::cout << "[ScriptEngine] Python module loaded successfully: " << moduleName << std::endl;
    } catch (const py::error_already_set& e) {
        // pybind11 provides excellent error traces directly from Python
        std::cerr << "[ScriptEngine] Python Error while loading module '" << moduleName << "':\n" << e.what() << std::endl;
    }
}
