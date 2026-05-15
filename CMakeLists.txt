# Minimum CMake version required
cmake_minimum_required(VERSION 3.20)

# Project name and the languages used (C and CXX for C++)
project(FoxvoidEngine VERSION 1.0.0 LANGUAGES C CXX)

# Force modern C++ (C++20) for clean and modern syntax
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# =========================================================================
# DEPENDENCY MANAGEMENT (FetchContent)
# =========================================================================
include(FetchContent)

# Fetch Raylib directly from GitHub
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG        5.5 # Lock to version 5.5 for stability
)
FetchContent_MakeAvailable(raylib)

# Fetch nlohmann/json (The best JSON parser for C++)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(json)

# --- IMGUI & RLIMGUI (Editor UI) ---
# We unconditionally fetch dependencies to avoid CMake variable scope issues, 
# but we will only build the Editor target on Desktop.

# 1. Fetch Dear ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        docking
)
FetchContent_MakeAvailable(imgui)

# 2. Fetch rlImGui
FetchContent_Declare(
    rlImGui
    GIT_REPOSITORY https://github.com/raylib-extras/rlImGui.git
    GIT_TAG        main
)
set(IMGUI_DIR ${imgui_SOURCE_DIR}) 
FetchContent_MakeAvailable(rlImGui)

# 3. Fetch ImGuizmo
FetchContent_Declare(
    imguizmo
    GIT_REPOSITORY https://github.com/CedricGuillemet/ImGuizmo.git
    GIT_TAG        master
)
FetchContent_GetProperties(imguizmo)
if(NOT imguizmo_POPULATED)
    FetchContent_Populate(imguizmo)
endif()

# 4. Fetch ImGuiColorTextEdit
FetchContent_Declare(
    imgui_color_text_edit
    GIT_REPOSITORY https://github.com/BalazsJako/ImGuiColorTextEdit.git
    GIT_TAG        master
)
FetchContent_GetProperties(imgui_color_text_edit)
if(NOT imgui_color_text_edit_POPULATED)
    FetchContent_Populate(imgui_color_text_edit)
endif()

# Fetch Portable File Dialogs
FetchContent_Declare(
    pfd
    GIT_REPOSITORY https://github.com/samhocevar/portable-file-dialogs.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(pfd)

# --- PYTHON & PYBIND11 SETUP ---

if (CMAKE_CROSSCOMPILING AND WIN32)
    message(STATUS "Cross-compiling for Windows: Fetching Windows Python 3.11 from GitHub...")
    
    FetchContent_Declare(
        python_win_deps
        URL "https://github.com/foxvoid-studio/FoxvoidEngine/releases/download/dependencies-v1/python_win.zip"
    )
    FetchContent_MakeAvailable(python_win_deps)
    
    # On pointe maintenant vers le dossier téléchargé par CMake (python_win_deps_SOURCE_DIR)
    set(Python3_INCLUDE_DIRS "${python_win_deps_SOURCE_DIR}/tools/include")
    set(Python3_LIBRARIES "${python_win_deps_SOURCE_DIR}/tools/libs/python311.lib")
    
    set(Python3_FOUND TRUE)
    set(Python3_Development_FOUND TRUE)
    set(Python3_Interpreter_FOUND TRUE)
    set(Python3_Development.Module_FOUND TRUE)
    set(Python3_Development.Embed_FOUND TRUE)
    
    find_program(Python3_EXECUTABLE NAMES python3 python REQUIRED)

    if(NOT TARGET Python3::Python)
        add_library(Python3::Python UNKNOWN IMPORTED)
        set_target_properties(Python3::Python PROPERTIES
            IMPORTED_LOCATION "${Python3_LIBRARIES}"
            INTERFACE_INCLUDE_DIRECTORIES "${Python3_INCLUDE_DIRS}"
        )
    endif()
    if(NOT TARGET Python3::Module)
        add_library(Python3::Module ALIAS Python3::Python)
    endif()
    if(NOT TARGET Python3::Embed)
        add_library(Python3::Embed ALIAS Python3::Python)
    endif()

elseif (ANDROID)
    message(STATUS "Cross-compiling for Android: Using BeeWare Python 3.10")
    
    set(Python3_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/vendor/python_android/include/python3.10")
    set(Python3_LIBRARIES "${CMAKE_SOURCE_DIR}/vendor/python_android/libs/arm64-v8a/libpython3.10.so")
    
    set(Python3_FOUND TRUE)
    set(Python3_Development_FOUND TRUE)
    set(Python3_Interpreter_FOUND TRUE)
    set(Python3_Development.Module_FOUND TRUE)
    set(Python3_Development.Embed_FOUND TRUE)
    
    find_program(Python3_EXECUTABLE NAMES python3 python REQUIRED)

    if(NOT TARGET Python3::Python)
        add_library(Python3::Python UNKNOWN IMPORTED)
        set_target_properties(Python3::Python PROPERTIES
            IMPORTED_LOCATION "${Python3_LIBRARIES}"
            INTERFACE_INCLUDE_DIRECTORIES "${Python3_INCLUDE_DIRS}"
        )
    endif()
    if(NOT TARGET Python3::Module)
        add_library(Python3::Module ALIAS Python3::Python)
    endif()
    if(NOT TARGET Python3::Embed)
        add_library(Python3::Embed ALIAS Python3::Python)
    endif()

elseif (EMSCRIPTEN)
    message(STATUS "Cross-compiling for Web (Wasm): Fetching custom Python 3.11 from GitHub...")
    
    FetchContent_Declare(
        python_wasm_deps
        URL "https://github.com/foxvoid-studio/FoxvoidEngine/releases/download/dependencies-v1/python_wasm.zip"
    )
    FetchContent_MakeAvailable(python_wasm_deps)
    
    # Point directly to our freshly downloaded Wasm Python static library
    set(Python3_INCLUDE_DIRS "${python_wasm_deps_SOURCE_DIR}/include")
    set(Python3_LIBRARIES "${python_wasm_deps_SOURCE_DIR}/lib/libpython3.11.a")
    
    set(Python3_FOUND TRUE)
    set(Python3_Development_FOUND TRUE)
    set(Python3_Interpreter_FOUND TRUE)
    set(Python3_Development.Module_FOUND TRUE)
    set(Python3_Development.Embed_FOUND TRUE)
    
    find_program(Python3_EXECUTABLE NAMES python3 python REQUIRED)

    if(NOT TARGET Python3::Python)
        add_library(Python3::Python UNKNOWN IMPORTED)
        set_target_properties(Python3::Python PROPERTIES
            IMPORTED_LOCATION "${Python3_LIBRARIES}"
            INTERFACE_INCLUDE_DIRECTORIES "${Python3_INCLUDE_DIRS}"
        )
    endif()
    if(NOT TARGET Python3::Module)
        add_library(Python3::Module ALIAS Python3::Python)
    endif()
    if(NOT TARGET Python3::Embed)
        add_library(Python3::Embed ALIAS Python3::Python)
    endif()

else()
    find_package(Python3 COMPONENTS Interpreter Development Development.Embed REQUIRED)
endif()

# 2. Fetch pybind11 directly from GitHub
FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG        v2.11.1
)
FetchContent_MakeAvailable(pybind11)

# =========================================================================
# SOURCE FILES EXPLICIT GATHERING
# =========================================================================

file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS 
    "src/audio/*.cpp"
    "src/core/*.cpp"
    "src/graphics/*.cpp"
    "src/gui/*.cpp"
    "src/physics/*.cpp"
    "src/scripting/*.cpp"
    "src/world/*.cpp"
    "src/main.cpp"
)

# =========================================================================
# EDITOR BUILD TARGET (FoxvoidEngine) - Desktop Only
# =========================================================================
# We DO NOT compile the Editor on Android or WebAssembly
if (NOT ANDROID AND NOT EMSCRIPTEN)
    file(GLOB_RECURSE EDITOR_SOURCES CONFIGURE_DEPENDS "src/editor/*.cpp")

    set(IMGUI_SOURCES
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${rlimgui_SOURCE_DIR}/rlImGui.cpp
        ${imguizmo_SOURCE_DIR}/src/ImGuizmo.cpp
        ${imgui_color_text_edit_SOURCE_DIR}/TextEditor.cpp
    )

    add_executable(FoxvoidEngine ${ENGINE_SOURCES} ${EDITOR_SOURCES} ${IMGUI_SOURCES})

    target_link_libraries(FoxvoidEngine PRIVATE 
        raylib 
        nlohmann_json::nlohmann_json 
        pybind11::embed
    )

    target_include_directories(FoxvoidEngine PRIVATE 
        src
        ${imgui_SOURCE_DIR}
        ${rlimgui_SOURCE_DIR}
        ${imguizmo_SOURCE_DIR}/src
        ${pfd_SOURCE_DIR}
        ${imgui_color_text_edit_SOURCE_DIR}
    )

    target_compile_definitions(FoxvoidEngine PRIVATE NDEBUG)
endif()

# =========================================================================
# STANDALONE BUILD TARGET (FoxvoidStandalone)
# =========================================================================

if (ANDROID)
    set(ANDROID_NATIVE_APP_GLUE "${CMAKE_SOURCE_DIR}/vendor/android_ndk/sources/android/native_app_glue")
    add_library(FoxvoidStandalone SHARED ${ENGINE_SOURCES} "${ANDROID_NATIVE_APP_GLUE}/android_native_app_glue.c")
    target_include_directories(FoxvoidStandalone PRIVATE "${ANDROID_NATIVE_APP_GLUE}")
    target_link_libraries(FoxvoidStandalone PRIVATE android log EGL GLESv2)
else()
    # Desktop & Web
    add_executable(FoxvoidStandalone ${ENGINE_SOURCES})
endif()

target_link_libraries(FoxvoidStandalone PRIVATE 
    raylib 
    nlohmann_json::nlohmann_json 
    pybind11::embed
)

target_include_directories(FoxvoidStandalone PRIVATE src)

if (CMAKE_CROSSCOMPILING AND WIN32)
    target_link_options(FoxvoidStandalone PRIVATE -static -static-libgcc -static-libstdc++)
endif()

# WebAssembly Specific Linker Flags
if (EMSCRIPTEN)
    # Output an HTML file containing the WebGL Canvas context
    set_target_properties(FoxvoidStandalone PROPERTIES SUFFIX ".html")

    target_link_libraries(FoxvoidStandalone PRIVATE 
        "${python_wasm_deps_SOURCE_DIR}/lib/libmpdec.a"
        "${python_wasm_deps_SOURCE_DIR}/lib/libexpat.a"
    )

    target_link_options(FoxvoidStandalone PRIVATE 
        "-sUSE_GLFW=3"
        "-sUSE_ZLIB=1"                           
        "-sASYNCIFY=1"                           
        "-sALLOW_MEMORY_GROWTH=1"                
        "-sFORCE_FILESYSTEM=1"                   
        
        "-sEXPORTED_RUNTIME_METHODS=ccall,cwrap,getValue,setValue,HEAPF32,HEAP8,HEAPU8"
        "-sEXPORTED_FUNCTIONS=_main,_malloc,_free"
        
        "SHELL:-s ERROR_ON_UNDEFINED_SYMBOLS=0"
        "SHELL:-s WARN_ON_UNDEFINED_SYMBOLS=0"
        "LINKER:--allow-undefined"

        "SHELL:--shell-file ${CMAKE_SOURCE_DIR}/web/web_shell.html"
        
        # Inject the Python Standard Library into the VFS at the path /lib/python3.11
        "SHELL:--preload-file ${python_wasm_deps_SOURCE_DIR}/lib/python3.11@/lib/python3.11"
        
        # Inject our Game Assets (data.pak) into the VFS right next to the executable
        "SHELL:--preload-file ${CMAKE_BINARY_DIR}/data.pak@/data.pak"
    )
endif()

# Define a macro so the C++ code knows it's running in Standalone mode
target_compile_definitions(FoxvoidStandalone PRIVATE STANDALONE_MODE)
