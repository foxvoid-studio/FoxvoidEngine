#include "LinuxBuilder.hpp"
#include "Build.hpp"

bool LinuxBuilder::Configure(const std::string& buildDir, const std::string& engineRoot) {
#if defined(_WIN32)
    // HOST is Windows, TARGET is Linux -> We MUST use WSL to compile
    Build::LogMessage("Host is Windows. Attempting to cross-compile for Linux via WSL2...");
    
    // wslpath converts Windows paths (C:\...) to Linux paths (/mnt/c/...) inside WSL
    // We use single quotes INSIDE the wslpath command, but we MUST ensure the outer 
    // CMake arguments are properly escaped to handle spaces in project names.
    std::string cmakeConfigCmd = "wsl cmake -S \"$(wslpath -u '" + engineRoot + "')\" -B \"$(wslpath -u '" + buildDir + "')\" -DCMAKE_BUILD_TYPE=Release";
#else
    // HOST is Linux, TARGET is Linux -> Native Compilation
    Build::LogMessage("Host is Linux. Using native compilation...");
    
    // FIX: When executing system commands via popen(), paths with spaces must be escaped properly.
    // Adding extra escaped quotes (\\\") ensures the shell passes the string as a single argument to CMake.
    std::string cmakeConfigCmd = "cmake -S \"" + engineRoot + "\" -B \"" + buildDir + "\" -DCMAKE_BUILD_TYPE=Release";
    
    // Alternative robust fix if the above still fails with popen:
    // std::string cmakeConfigCmd = "cmake -S '" + engineRoot + "' -B '" + buildDir + "' -DCMAKE_BUILD_TYPE=Release";
#endif

    return Build::ExecuteCommandWithOutput(cmakeConfigCmd, 0, 10) == 0;
}

bool LinuxBuilder::Compile(const std::string& buildDir) {
#if defined(_WIN32)
    std::string cmakeBuildCmd = "wsl cmake --build \"$(wslpath -u '" + buildDir + "')\" --target FoxvoidStandalone --config Release";
#else
    // FIX: Same here, ensure the build directory is safely wrapped for the shell.
    std::string cmakeBuildCmd = "cmake --build \"" + buildDir + "\" --target FoxvoidStandalone --config Release";
#endif
    return Build::ExecuteCommandWithOutput(cmakeBuildCmd, 10, 90) == 0;
}

bool LinuxBuilder::CopyDependencies(const std::filesystem::path& buildDir, const std::string& engineRoot, ScreenOrientation orientation) {
    // Linux usually resolves dependencies dynamically via package managers.
    // In the future, we could package .so files here if needed.
    return true; 
}
