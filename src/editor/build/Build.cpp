#include "Build.hpp"

#include "core/ProjectSettings.hpp"
#include "core/AssetRegistry.hpp"
#include "core/PakEntry.hpp"

// Include our specific builders
#include "IBuilder.hpp"
#include "WindowsBuilder.hpp"
#include "LinuxBuilder.hpp"
#include "AndroidBuilder.hpp"
#include "WebBuilder.hpp"

#include <iostream>
#include <fstream>
#include <algorithm>

// Initialize static members
std::atomic<bool> Build::s_isBuilding{false};
std::atomic<int> Build::s_buildProgress{0};
std::mutex Build::s_buildMutex;
std::string Build::s_buildStatusMsg = "";
std::vector<std::string> Build::s_buildLogs;

void Build::Start(const std::string& startScene, const std::string& outputDir, const std::filesystem::path& projectRoot, const std::string& engineRoot, TargetOS target, ScreenOrientation orientation) {
    if (s_isBuilding) return; // Prevent multiple builds at the same time

    s_isBuilding = true;
    s_buildProgress = 0;
    
    {
        std::lock_guard<std::mutex> lock(s_buildMutex);
        s_buildStatusMsg = "Initializing Build...";
        s_buildLogs.clear();
    }

    // Spawn the thread and detach it
    std::thread(&Build::RunThread, startScene, outputDir, projectRoot, engineRoot, target).detach();
}

bool Build::IsBuilding() {
    return s_isBuilding.load();
}

int Build::GetProgress() {
    return s_buildProgress.load();
}

std::string Build::GetStatusMessage() {
    std::lock_guard<std::mutex> lock(s_buildMutex);
    return s_buildStatusMsg;
}

std::vector<std::string> Build::GetLogs() {
    std::lock_guard<std::mutex> lock(s_buildMutex);
    return s_buildLogs; // Returns a safe copy for the UI to iterate over
}

void Build::LogMessage(const std::string& msg) {
    std::lock_guard<std::mutex> lock(s_buildMutex);
    s_buildLogs.push_back(msg);
}

void Build::LogWarning(const std::string& msg) {
    std::lock_guard<std::mutex> lock(s_buildMutex);
    s_buildLogs.push_back("[Warning] " + msg);
}

void Build::LogError(const std::string& msg) {
    std::lock_guard<std::mutex> lock(s_buildMutex);
    s_buildLogs.push_back("[Error] " + msg);
}

void Build::RunThread(std::string startSceneStr, std::string outputDirStr, std::filesystem::path projectRoot, std::string engineRoot, TargetOS target) {
    std::filesystem::path buildDir = projectRoot / outputDirStr;
    
    {
        std::lock_guard<std::mutex> lock(s_buildMutex);
        s_buildLogs.clear();
        s_buildStatusMsg = "Step 1/3: Configuring CMake...";
    }
    s_buildProgress = 0;

    // --- STRATEGY PATTERN IN ACTION ---
    // Instantiate the correct builder based on the target OS requested
    std::unique_ptr<IBuilder> builder;
    if (target == TargetOS::Windows) {
        builder = std::make_unique<WindowsBuilder>();
    } else if (target == TargetOS::Android) {
        builder = std::make_unique<AndroidBuilder>();
    } else if (target == TargetOS::Web) {
        builder = std::make_unique<WebBuilder>();
    } else {
        builder = std::make_unique<LinuxBuilder>();
    }

    // Step 1: Configuration (OS Specific, handled by the builder)
    if (!builder->Configure(buildDir.string(), engineRoot)) {
        std::lock_guard<std::mutex> lock(s_buildMutex);
        s_buildStatusMsg = "FAILED during CMake configuration.";
        s_buildProgress = -1;
        s_isBuilding = false;
        return;
    }

    // Step 2: Packing Assets (Universal Step - OS Independent)
    // NOTE: Must happen BEFORE compilation because Emscripten needs data.pak during the linking phase!
    {
        std::lock_guard<std::mutex> lock(s_buildMutex);
        s_buildStatusMsg = "Step 2/3: Packing Assets into data.pak...";
    }
    s_buildProgress = 20;

    try {
        // The packer (Virtual File System)
        std::filesystem::path pakPath = buildDir / "data.pak";
        std::ofstream pakFile(pakPath, std::ios::binary);

        if (pakFile.is_open()) {
            std::vector<std::filesystem::path> filesToPack;

            filesToPack.push_back(projectRoot / "project.json");

            for (const auto& entry : std::filesystem::recursive_directory_iterator(projectRoot / "assets")) {
                if (entry.is_regular_file()) {
                    std::string ext = entry.path().extension().string();
                    std::string name = entry.path().filename().string();
                    
                    if (ext != ".meta" && ext != ".pyc" && name[0] != '.') {
                        filesToPack.push_back(entry.path());
                    }
                }
            }

            uint32_t numFiles = static_cast<uint32_t>(filesToPack.size());
            pakFile.write(reinterpret_cast<const char*>(&numFiles), sizeof(uint32_t));

            std::vector<PakEntry> toc;

            uint64_t tocOffset = pakFile.tellp();
            uint64_t tocSize = numFiles * sizeof(PakEntry);
            pakFile.seekp(tocOffset + tocSize);

            for (const auto& filePath : filesToPack) {
                UUID fileUUID = 0;
                std::string relPath;

                if (filePath.filename() == "project.json") {
                    fileUUID = UUID(1);
                    relPath = "project.json";
                }
                else {
                    fileUUID = AssetRegistry::GetUUIDForPath(filePath);
                    relPath = std::filesystem::relative(filePath, projectRoot).generic_string();
                }
                
                if (fileUUID != 0) {
                    std::ifstream sourceFile(filePath, std::ios::binary | std::ios::ate);
                    if (sourceFile.is_open()) {
                        uint64_t fileSize = sourceFile.tellg();
                        sourceFile.seekg(0);

                        uint64_t currentOffset = pakFile.tellp();

                        // Get relative path for the TOC
                        std::string entryRelPath = std::filesystem::relative(filePath, projectRoot).generic_string();

                        PakEntry entry = {(uint64_t)fileUUID, currentOffset, fileSize, {0}};
                        strncpy(entry.path, entryRelPath.c_str(), sizeof(entry.path) - 1);

                        toc.push_back(entry);

                        std::vector<char> buffer(fileSize);
                        sourceFile.read(buffer.data(), fileSize);
                        pakFile.write(buffer.data(), fileSize);

                        sourceFile.close();
                    }
                }
            }

            pakFile.seekp(tocOffset);
            pakFile.write(reinterpret_cast<const char*>(toc.data()), toc.size() * sizeof(PakEntry));
            pakFile.close();
        } else {
            throw std::runtime_error("Could not create data.pak file.");
        }

        // Step 3: Compilation (OS Specific, handled by the builder)
        {
            std::lock_guard<std::mutex> lock(s_buildMutex);
            s_buildStatusMsg = "Step 3/3: Compiling Game (This will take a moment)...";
        }
        s_buildProgress = 40;
        
        if (!builder->Compile(buildDir.string())) {
            std::lock_guard<std::mutex> lock(s_buildMutex);
            s_buildStatusMsg = "FAILED during compilation.";
            s_buildProgress = -1;
            s_isBuilding = false;
            return;
        }

        s_buildProgress = 95;

        // For Web, the builder handles the renaming of HTML/JS/WASM internally in CopyDependencies.
        // For Android and Desktop, we handle the primary binary renaming here.
        if (target != TargetOS::Web) {
            // Rename the executable based on the Project Name
            std::string projectName = ProjectSettings::GetProjectName();
            std::string safeProjectName = projectName;
            std::replace(safeProjectName.begin(), safeProjectName.end(), ' ', '_');

            // Ask the builder for the correct extension (.exe, .so, etc.)
            std::string extension = builder->GetExecutableExtension();

            // Handle Android naming convention (libprefix)
            std::filesystem::path originalExe;
            if (target == TargetOS::Android) {
                // Android CMake generates libFoxvoidStandalone.so
                originalExe = buildDir / ("libFoxvoidStandalone" + extension);
            } else {
                originalExe = buildDir / ("FoxvoidStandalone" + extension);
            }
            
            // Fallback for CMake multi-config generators that create a Release/ folder
            if (!std::filesystem::exists(originalExe)) {
                std::string folderPrefix = (target == TargetOS::Android) ? "lib" : "";
                originalExe = buildDir / "Release" / (folderPrefix + "FoxvoidStandalone" + extension); 
            }
            
            // Final name: keep 'lib' prefix for Android shared libraries
            std::string finalPrefix = (target == TargetOS::Android) ? "lib" : "";
            std::filesystem::path newExe = buildDir / (finalPrefix + safeProjectName + extension);

            if (std::filesystem::exists(originalExe)) {
                std::filesystem::copy_file(originalExe, newExe, std::filesystem::copy_options::overwrite_existing);
                
                // Ask the builder to handle its specific dependencies (e.g., APK packaging)
                // We call this BEFORE removing the originalExe because AndroidBuilder 
                // might need the original filename to find it.
                builder->CopyDependencies(buildDir, engineRoot);
                
                // Cleanup
                std::filesystem::remove(originalExe);
            } else {
                Build::LogWarning("Could not find the executable to rename. Checked: " + originalExe.string());
                
                // Fallback for Android: Try to copy dependencies even if renaming failed
                if (target == TargetOS::Android) {
                    builder->CopyDependencies(buildDir, engineRoot);
                }
            }
        }
        else {
            // If it's a Web Build, we directly call CopyDependencies because WebBuilder
            // takes care of renaming its own HTML, JS, WASM and DATA files.
            builder->CopyDependencies(buildDir, engineRoot);
        }

        {
            std::lock_guard<std::mutex> lock(s_buildMutex);
            s_buildStatusMsg = "SUCCESS! Game exported to: " + outputDirStr;
            s_buildLogs.push_back("--- BUILD FINISHED SUCCESSFULLY ---");
        }
        s_buildProgress = 100;
        
    } catch(std::filesystem::filesystem_error& e) {
        LogError(std::string("Filesystem Error: ") + e.what());
        std::lock_guard<std::mutex> lock(s_buildMutex);
        s_buildStatusMsg = "Error copying assets.";
        s_buildProgress = -1; 
    }

    s_isBuilding = false;
}

int Build::ExecuteCommandWithOutput(const std::string& cmd, int baseProgress, int maxProgress) {
    std::string fullCmd = cmd + " 2>&1";
    FILE* pipe = popen(fullCmd.c_str(), "r");
    if (!pipe) {
        std::lock_guard<std::mutex> lock(s_buildMutex);
        s_buildLogs.push_back("[Error] Failed to open process pipe.");
        return -1;
    }

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        std::string line(buffer);
        if (!line.empty() && line.back() == '\n') line.pop_back();

        {
            std::lock_guard<std::mutex> lock(s_buildMutex);
            s_buildLogs.push_back(line);
        }

        size_t pctPos = line.find("%");
        if (pctPos != std::string::npos && pctPos > 0) {
            size_t start = pctPos - 1;
            while (start > 0 && std::isdigit(line[start])) start--;
            if (!std::isdigit(line[start])) start++;

            if (start < pctPos) {
                try {
                    int extractedPct = std::stoi(line.substr(start, pctPos - start));
                    float fraction = extractedPct / 100.0f;
                    s_buildProgress = baseProgress + static_cast<int>(fraction * (maxProgress - baseProgress));
                } catch(...) {}
            }
        }
    }

    return pclose(pipe);
}
