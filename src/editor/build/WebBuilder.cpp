#include "WebBuilder.hpp"

#include "Build.hpp"
#include "core/ProjectSettings.hpp"

#include <filesystem>
#include <cstdlib>
#include <fstream>
#include <sstream>

bool WebBuilder::Configure(const std::string& buildDir, const std::string& engineRoot) {
    Build::LogMessage("Cross-compiling for Web (WebAssembly) using Emscripten...");

    // 1. Locate the Emscripten Toolchain
    std::filesystem::path emsdkPath = std::filesystem::path(engineRoot) / "vendor" / "emsdk";
    if (!std::filesystem::exists(emsdkPath)) {
        Build::LogError("Emscripten SDK not found in vendor/emsdk! Please install it first.");
        return false;
    }

    std::filesystem::path toolchainFile = emsdkPath / "upstream" / "emscripten" / "cmake" / "Modules" / "Platform" / "Emscripten.cmake";
    if (!std::filesystem::exists(toolchainFile)) {
        Build::LogError("Emscripten toolchain file missing at: " + toolchainFile.string());
        return false;
    }

    // 2. Locate our compiled Python Wasm
    std::filesystem::path pythonWasmDir = std::filesystem::path(engineRoot) / "vendor" / "python_wasm";
    if (!std::filesystem::exists(pythonWasmDir)) {
        Build::LogError("Python Wasm libraries not found in vendor/python_wasm! Please compile them first.");
        return false;
    }

    // 3. Build the CMake command
    // Notice we use `emcmake cmake` which wraps the standard cmake command with WebAssembly environment variables
    std::string emcmake = (emsdkPath / "upstream" / "emscripten" / "emcmake").string();
    
    std::string cmakeConfigCmd = emcmake + " cmake -S \"" + engineRoot + "\" -B \"" + buildDir + "\" -DCMAKE_BUILD_TYPE=Release";
    
    // Tell CMake exactly which toolchain to use
    cmakeConfigCmd += " -DCMAKE_TOOLCHAIN_FILE=\"" + toolchainFile.string() + "\"";
    
    // Tell Raylib and our Engine we are building for the Web
    cmakeConfigCmd += " -DPLATFORM=Web";

    // Disable standard Pybind11 discovery, we will inject our Wasm Python manually in the CMakeLists.txt
    cmakeConfigCmd += " -DPYBIND11_FINDPYTHON=OFF"; 

    return Build::ExecuteCommandWithOutput(cmakeConfigCmd, 0, 10) == 0;
}

bool WebBuilder::Compile(const std::string& buildDir) {
    // For Web, we compile the standalone engine directly into an HTML file
    // Emscripten will automatically generate the .wasm, .js, and .html files based on the target name
    std::string cmakeBuildCmd = "cmake --build \"" + buildDir + "\" --target FoxvoidStandalone --config Release";
    return Build::ExecuteCommandWithOutput(cmakeBuildCmd, 10, 80) == 0;
}

std::string WebBuilder::GetExecutableExtension() const {
    // Emscripten outputs an HTML file as the primary entry point
    return ".html";
}

bool WebBuilder::CopyDependencies(const std::filesystem::path& buildDir, const std::string& engineRoot, ScreenOrientation orientation) {
    Build::LogMessage("Packaging Web Build...");

    // 1. Rename the output files to match the Project Name
    std::string projectName = ProjectSettings::GetProjectName();
    std::replace(projectName.begin(), projectName.end(), ' ', '_'); // Clean the name

    // Emscripten generates these three files based on our CMake target name "FoxvoidStandalone"
    std::filesystem::path oldHtml = buildDir / "FoxvoidStandalone.html";
    std::filesystem::path oldJs = buildDir / "FoxvoidStandalone.js";
    std::filesystem::path oldWasm = buildDir / "FoxvoidStandalone.wasm";
    std::filesystem::path oldData = buildDir / "FoxvoidStandalone.data"; // Generated if we use --preload-file

    std::filesystem::path newHtml = buildDir / (projectName + ".html");
    std::filesystem::path newJs = buildDir / (projectName + ".js");
    std::filesystem::path newWasm = buildDir / (projectName + ".wasm");
    std::filesystem::path newData = buildDir / (projectName + ".data");

    bool success = true;

    // Rename HTML
    if (std::filesystem::exists(oldHtml)) {
        // Since the HTML file contains a hardcoded reference to the .js file, we must read and replace it
        std::ifstream in(oldHtml);
        std::stringstream buffer;
        buffer << in.rdbuf();
        std::string htmlContent = buffer.str();
        in.close();

        // Replace "FoxvoidStandalone.js" with "ProjectName.js" inside the HTML script tag
        std::string searchString = "FoxvoidStandalone.js";
        size_t pos = htmlContent.find(searchString);
        if (pos != std::string::npos) {
            htmlContent.replace(pos, searchString.length(), projectName + ".js");
        }

        std::ofstream out(newHtml);
        out << htmlContent;
        out.close();
        
        // Remove the old file
        std::filesystem::remove(oldHtml);
    } else {
        Build::LogError("Web build failed: " + oldHtml.filename().string() + " not found.");
        success = false;
    }

    // Rename JS
    if (std::filesystem::exists(oldJs)) {
        // The JS file contains a hardcoded reference to the .wasm file (and potentially the .data file)
        std::ifstream in(oldJs);
        std::stringstream buffer;
        buffer << in.rdbuf();
        std::string jsContent = buffer.str();
        in.close();

        // Replace references
        size_t pos = 0;
        while ((pos = jsContent.find("FoxvoidStandalone.wasm", pos)) != std::string::npos) {
            jsContent.replace(pos, 22, projectName + ".wasm");
            pos += projectName.length() + 5;
        }

        pos = 0;
        while ((pos = jsContent.find("FoxvoidStandalone.data", pos)) != std::string::npos) {
            jsContent.replace(pos, 22, projectName + ".data");
            pos += projectName.length() + 5;
        }

        std::ofstream out(newJs);
        out << jsContent;
        out.close();
        std::filesystem::remove(oldJs);
    }

    // Rename Wasm
    if (std::filesystem::exists(oldWasm)) {
        std::filesystem::rename(oldWasm, newWasm);
    }

    // Rename Data (Virtual File System)
    if (std::filesystem::exists(oldData)) {
        std::filesystem::rename(oldData, newData);
    }

    if (success) {
        Build::LogMessage("==============================================");
        Build::LogMessage(" SUCCESS! Web Build generated at:");
        Build::LogMessage(" " + newHtml.string());
        Build::LogMessage(" Note: To play, you must serve this folder using a local web server.");
        Build::LogMessage(" Example: python3 -m http.server 8000");
        Build::LogMessage("==============================================");
    }

    return success;
}
