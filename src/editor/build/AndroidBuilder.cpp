#include "AndroidBuilder.hpp"

#include "Build.hpp"
#include "core/ProjectSettings.hpp"

#include <filesystem>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <regex>

bool AndroidBuilder::Configure(const std::string& buildDir, const std::string& engineRoot) {
    Build::LogMessage("Cross-compiling for Android (ARM64) using the NDK...");

    // 1. SEARCH FOR THE NDK ON THE SYSTEM (No longer in vendor/)
    const char* envNdk = std::getenv("ANDROID_NDK_HOME");
    std::string ndkPath;

    if (envNdk) {
        ndkPath = envNdk;
    } else {
        // Typical fallback path if the variable is not set (adapt if necessary)
        ndkPath = "/home/ikigami/Android/Sdk/ndk/25.2.9519653"; 
        Build::LogWarning("ANDROID_NDK_HOME not found in environment. Falling back to: " + ndkPath);
    }

    std::filesystem::path toolchainPath = std::filesystem::path(ndkPath) / "build" / "cmake" / "android.toolchain.cmake";
    
    if (!std::filesystem::exists(toolchainPath)) {
        Build::LogError("Android NDK Toolchain not found at: " + toolchainPath.string() + " ! Please install the NDK via Android Studio and set ANDROID_NDK_HOME.");
        return false;
    }

    std::filesystem::path pyAndroidDir = std::filesystem::path(engineRoot) / "vendor" / "python_android";

    // Automatic dependency resolution
    if (!std::filesystem::exists(pyAndroidDir / "include")) {
        Build::LogWarning("Android Python dependencies are missing. Starting automatic download...");
        std::filesystem::create_directories(pyAndroidDir);
        
        // Downloading the BeeWare Python 3.10 support package for Android
        Build::LogMessage("Downloading Python 3.10 for Android (BeeWare Support Package)...");
        std::string cmdDownload = "wget -q https://github.com/beeware/Python-Android-support/releases/download/3.10-b2/Python-3.10-Android-support.b2.zip -O \"" + (pyAndroidDir / "python_android.zip").string() + "\"";
        if (Build::ExecuteCommandWithOutput(cmdDownload, 0, 5) != 0) {
            Build::LogError("Failed to download Android Python Package.");
            return false;
        }

        Build::LogMessage("Extracting dependencies (This may take a moment)...");
        std::string cmdUnzip = "unzip -q -o \"" + (pyAndroidDir / "python_android.zip").string() + "\" -d \"" + pyAndroidDir.string() + "\"";
        Build::ExecuteCommandWithOutput(cmdUnzip, 5, 8);

        std::filesystem::remove(pyAndroidDir / "python_android.zip");
        Build::LogMessage("Successfully downloaded and extracted Android Python dependencies.");
    }

    std::string cmakeConfigCmd = "cmake -S \"" + engineRoot + "\" -B \"" + buildDir + "\" -DCMAKE_BUILD_TYPE=Release";
    
    // Tell CMake to use the official Android Toolchain included in the NDK
    cmakeConfigCmd += " -DCMAKE_TOOLCHAIN_FILE=\"" + toolchainPath.string() + "\"";
    
    // Define the Target Architecture (Modern 64-bit phones)
    cmakeConfigCmd += " -DANDROID_ABI=arm64-v8a";
    
    // Define the minimum Android Version (API 29 = Android 10)
    cmakeConfigCmd += " -DANDROID_PLATFORM=android-29";
    
    // Tell Raylib we are building for Android
    cmakeConfigCmd += " -DPLATFORM=Android";

    cmakeConfigCmd += " -DPYBIND11_FINDPYTHON=OFF"; 

    return Build::ExecuteCommandWithOutput(cmakeConfigCmd, 0, 10) == 0;
}

bool AndroidBuilder::Compile(const std::string& buildDir) {
    // We compile the standalone engine as a shared library for Android
    std::string cmakeBuildCmd = "cmake --build \"" + buildDir + "\" --target FoxvoidStandalone --config Release";
    return Build::ExecuteCommandWithOutput(cmakeBuildCmd, 10, 90) == 0;
}

bool AndroidBuilder::CopyDependencies(const std::filesystem::path& buildDir, const std::string& engineRoot, ScreenOrientation orientation) {
    Build::LogMessage("Packaging Android APK...");

    // 1. Locate the Android Studio Template
    std::filesystem::path templateDir = std::filesystem::path(engineRoot) / "android" / "FoxvoidAndroidTemplate";
    if (!std::filesystem::exists(templateDir)) {
        Build::LogError("Android template not found at: " + templateDir.string());
        return false;
    }

    std::filesystem::path androidProjectDir = buildDir / "android_project";

    // 2. Clone the template into the build folder
    if (std::filesystem::exists(androidProjectDir)) {
        std::filesystem::remove_all(androidProjectDir);
    }
    std::filesystem::copy(templateDir, androidProjectDir, std::filesystem::copy_options::recursive);

    // 3. Define target directories inside the Android project
    std::filesystem::path jniLibsDir = androidProjectDir / "app" / "src" / "main" / "jniLibs" / "arm64-v8a";
    std::filesystem::path assetsDir = androidProjectDir / "app" / "src" / "main" / "assets";
    std::filesystem::create_directories(jniLibsDir);
    std::filesystem::create_directories(assetsDir);

    // 4. Inject the compiled Engine Library (.so)
    std::filesystem::path engineLib = buildDir / "libFoxvoidStandalone.so";
    if (std::filesystem::exists(engineLib)) {
        std::filesystem::copy_file(engineLib, jniLibsDir / "libFoxvoidStandalone.so", std::filesystem::copy_options::overwrite_existing);
    } else {
        Build::LogError("Engine library (libFoxvoidStandalone.so) not found! Did the compilation fail?");
        return false;
    }

    // 5. Inject the Python Shared Library
    std::filesystem::path pythonLib = std::filesystem::path(engineRoot) / "vendor" / "python_android" / "libs" / "arm64-v8a" / "libpython3.10.so";
    if (std::filesystem::exists(pythonLib)) {
        std::filesystem::copy_file(pythonLib, jniLibsDir / "libpython3.10.so", std::filesystem::copy_options::overwrite_existing);
    } else {
        Build::LogError("Python library (libpython3.10.so) not found!");
        return false;
    }

    // 6. Inject the Python Standard Library (Extracting the BeeWare zip)
    std::filesystem::path stdlibBaseDir = std::filesystem::path(engineRoot) / "vendor" / "python_android" / "src" / "main" / "assets" / "stdlib";
    
    if (std::filesystem::exists(stdlibBaseDir)) {
        // Find the architecture specific zip (arm64-v8a)
        std::filesystem::path zipPath;
        for (const auto& entry : std::filesystem::directory_iterator(stdlibBaseDir)) {
            if (entry.path().extension() == ".zip" && entry.path().string().find("arm64-v8a") != std::string::npos) {
                zipPath = entry.path();
                break;
            }
        }

        if (!zipPath.empty()) {
            std::filesystem::path targetPythonHome = assetsDir / "python_home";
            std::filesystem::create_directories(targetPythonHome);
            
            Build::LogMessage("Unzipping Python Standard Library into Android assets (This takes a few seconds)...");
            // Use unzip to extract the contents directly into assets/python_home
            std::string unzipCmd = "unzip -q -o \"" + zipPath.string() + "\" -d \"" + targetPythonHome.string() + "\"";
            system(unzipCmd.c_str());
        } else {
            Build::LogError("CRITICAL: Could not find Python stdlib zip for arm64-v8a!");
            return false;
        }
    } else {
        Build::LogError("CRITICAL: Python stdlib directory not found at: " + stdlibBaseDir.string());
        return false;
    }

    // 7. Inject Game Assets (Images, scripts, audio...)
    // Assuming game assets are stored in engineRoot/assets.
    std::filesystem::path gameAssetsDir = std::filesystem::path(engineRoot) / "assets";
    if (std::filesystem::exists(gameAssetsDir)) {
        for (const auto& entry : std::filesystem::directory_iterator(gameAssetsDir)) {
            // Protection to avoid recursively copying the template if it's placed inside the assets folder
            if (entry.path().filename() != "android_template") { 
                std::filesystem::copy(entry.path(), assetsDir / entry.path().filename(), std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing);
            }
        }
    }

    // 7.5 Inject the packed data file
    std::filesystem::path dataPak = buildDir / "data.pak";
    if (std::filesystem::exists(dataPak)) {
        std::filesystem::copy_file(dataPak, assetsDir / "data.pak", std::filesystem::copy_options::overwrite_existing);
        Build::LogMessage("Successfully injected data.pak into Android assets.");
    } else {
        Build::LogWarning("data.pak not found! The game will have no assets.");
    }

    // 7.8 Inject the requested screen orientation into AndroidManifest.xml
    std::filesystem::path manifestPath = androidProjectDir / "app" / "src" / "main" / "AndroidManifest.xml";
    if (std::filesystem::exists(manifestPath)) {
        // Read the manifest into memory
        std::ifstream in(manifestPath);
        std::stringstream buffer;
        buffer << in.rdbuf();
        std::string manifestContent = buffer.str();
        in.close();

        std::string projectName = ProjectSettings::GetProjectName();
        // Determine the target string based on the selected enum
        std::string targetOrientation = (orientation == ScreenOrientation::Portrait) ? "sensorPortrait" : "sensorLandscape";

        // Check if an orientation attribute already exists
        std::regex orientationRegex("android:screenOrientation=\"([^\"]*)\"");
        if (std::regex_search(manifestContent, orientationRegex)) {
            // Replace the existing one
            manifestContent = std::regex_replace(manifestContent, orientationRegex, "android:screenOrientation=\"" + targetOrientation + "\"");
        } else {
            // Inject it right under the MainActivity declaration
            std::regex nameRegex("android:name=\"\\.MainActivity\"");
            manifestContent = std::regex_replace(manifestContent, nameRegex, "android:name=\".MainActivity\"\n            android:screenOrientation=\"" + targetOrientation + "\"");
        }

        // Inject App Name (Replaces all occurrences of android:label)
        std::regex labelRegex("android:label=\"([^\"]*)\"");
        manifestContent = std::regex_replace(manifestContent, labelRegex, "android:label=\"" + projectName + "\"");

        // Write the modified manifest back to disk
        std::ofstream out(manifestPath);
        out << manifestContent;
        out.close();

        Build::LogMessage("Configured Android screen orientation: " + targetOrientation);
    } else {
        Build::LogWarning("AndroidManifest.xml not found! Could not set screen orientation.");
    }

    // 8. Compile the APK using Gradle
    Build::LogMessage("Compiling APK with Gradle (This may take a minute)...");
    
    // Ensure the gradlew wrapper has execution permissions on Linux
    std::string chmodCmd = "chmod +x \"" + (androidProjectDir / "gradlew").string() + "\"";
    system(chmodCmd.c_str());

    // We build a 'Debug' APK for now, as Release requires a Keystore setup
    std::string gradleCmd = "cd \"" + androidProjectDir.string() + "\" && ./gradlew assembleDebug";
    if (Build::ExecuteCommandWithOutput(gradleCmd, 90, 100) != 0) {
        Build::LogError("Gradle APK compilation failed. Check the logs above.");
        return false;
    }

    // 9. Extract the final APK to the root of the build folder
    std::filesystem::path apkPath = androidProjectDir / "app" / "build" / "outputs" / "apk" / "debug" / "app-debug.apk";

    // Create a safe APK name (replace spaces with underscores)
    std::string safeApkName = ProjectSettings::GetProjectName();
    std::replace(safeApkName.begin(), safeApkName.end(), ' ', '_');
    std::filesystem::path finalApkPath = buildDir / (safeApkName + ".apk");

    if (std::filesystem::exists(apkPath)) {
        std::filesystem::copy_file(apkPath, finalApkPath, std::filesystem::copy_options::overwrite_existing);
        Build::LogMessage("==============================================");
        Build::LogMessage(" SUCCESS! APK generated at:");
        Build::LogMessage(" " + finalApkPath.string());
        Build::LogMessage("==============================================");
    } else {
        Build::LogError("APK not found after Gradle build!");
        return false;
    }

    return true;
}
