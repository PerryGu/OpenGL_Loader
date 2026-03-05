cmake_minimum_required(VERSION 3.14)

# Set C++ standard BEFORE project() to ensure it's applied correctly
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable C language support for standard headers like float.h
project(OpenGL_loader LANGUAGES CXX C)

# Use FetchContent to download GLFW
include(FetchContent)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.8
)

FetchContent_MakeAvailable(glfw)

# Find OpenGL
find_package(OpenGL QUIET)

# Collect all source files
file(GLOB_RECURSE SRC_FILES
    "src/*.cpp"
    "src/*.c"
)

# Explicitly ensure critical files are included (GLOB_RECURSE should find them, but explicit inclusion ensures they're always compiled)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/io/fbx_rig_analyzer.cpp")
    list(APPEND SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/io/fbx_rig_analyzer.cpp")
endif()

# Explicitly ensure scene.cpp is included (critical for Scene::init)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/io/scene.cpp")
    list(APPEND SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/io/scene.cpp")
endif()

# Explicitly ensure viewport_selection.cpp is included (critical for ViewportSelectionManager)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/viewport_selection.cpp")
    list(APPEND SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/viewport_selection.cpp")
endif()

# Explicitly ensure debug_panel.cpp is included (critical for DebugPanel)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/io/ui/debug_panel.cpp")
    list(APPEND SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/io/ui/debug_panel.cpp")
endif()

# Explicitly ensure logger.cpp is included (critical for Logger singleton)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/utils/logger.cpp")
    list(APPEND SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/utils/logger.cpp")
endif()

# Remove duplicates in case GLOB already found them
list(REMOVE_DUPLICATES SRC_FILES)

file(GLOB IMGUI_SOURCES
    "Dependencies/imgui/*.cpp"
)

file(GLOB OPENFBX_SOURCES
    "Dependencies/openFBX/*.cpp"
    "Dependencies/openFBX/*.c"
)

file(GLOB IMGUIZMO_SOURCES
    "Dependencies/ImGuizmo/*.cpp"
)

# Create executable with all source files
add_executable(OpenGL_loader
    ${SRC_FILES}
    ${IMGUI_SOURCES}
    ${OPENFBX_SOURCES}
    ${IMGUIZMO_SOURCES}
    Dependencies/include/glm/detail/glm.cpp
    Dependencies/lib/gl3w/GL/gl3w.c
    Dependencies/lib/usynergy/uSynergy.c
)

# Configure include directories
target_include_directories(OpenGL_loader PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Dependencies
    ${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/ImGuizmo
    ${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/include
    ${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/openFBX
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/lib/gl3w
    ${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/lib/gl3w/GL
)

# Define preprocessor macros
target_compile_definitions(OpenGL_loader PRIVATE
    IMGUI_IMPL_OPENGL_LOADER_GLAD
)

# Generate compile_commands.json for better IntelliSense support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add GLFW include directory
target_include_directories(OpenGL_loader PRIVATE
    ${glfw_SOURCE_DIR}/include
)

# Add OpenGL include directories if found
if(OpenGL_FOUND)
    target_include_directories(OpenGL_loader PRIVATE
        ${OPENGL_INCLUDE_DIRS}
    )
endif()

# Link libraries
target_link_libraries(OpenGL_loader PRIVATE
    glfw
)

# Link OpenGL
if(OpenGL_FOUND)
    target_link_libraries(OpenGL_loader PRIVATE
        OpenGL::GL
    )
else()
    # Fallback for systems where find_package doesn't work
    if(WIN32)
        target_link_libraries(OpenGL_loader PRIVATE
            opengl32
        )
    else()
        # Linux/Unix fallback
        target_link_libraries(OpenGL_loader PRIVATE
            GL
        )
    endif()
endif()

# Link Assimp if available
# Use v142 for Visual Studio 2019
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/lib/assimp/assimp-vc142-mt.lib")
    target_link_libraries(OpenGL_loader PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/lib/assimp/assimp-vc142-mt.lib"
    )
    target_include_directories(OpenGL_loader PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/include/assimp"
    )
    set(ASSIMP_DLL_NAME "assimp-vc142-mt.dll")
    message(STATUS "Using Assimp v142 (VS 2019)")
else()
    message(WARNING "Assimp v142 library not found! Animation may not work correctly.")
endif()

# Windows-specific libraries
if(WIN32)
    target_link_libraries(OpenGL_loader PRIVATE
        legacy_stdio_definitions
    )
    
    # Hide console window for Release builds (Windows subsystem)
    # Only applies to Release configuration, Debug keeps console for debugging
    if(MSVC)
        # Use generator expression to only apply to Release builds
        set_target_properties(OpenGL_loader PROPERTIES
            LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"
        )
        message(STATUS "Console window will be hidden in Release builds (Windows subsystem)")
    endif()
    
    # Copy required DLLs to output directory
    # Always copy zlib.dll if it exists
    if(EXISTS "${CMAKE_SOURCE_DIR}/zlib.dll")
        add_custom_command(TARGET OpenGL_loader POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/zlib.dll"
                "$<TARGET_FILE_DIR:OpenGL_loader>"
            COMMENT "Copying zlib.dll to output directory"
        )
    endif()
    
    # Copy Assimp DLL if available
    if(DEFINED ASSIMP_DLL_NAME AND EXISTS "${CMAKE_SOURCE_DIR}/${ASSIMP_DLL_NAME}")
        add_custom_command(TARGET OpenGL_loader POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/${ASSIMP_DLL_NAME}"
                "$<TARGET_FILE_DIR:OpenGL_loader>"
            COMMENT "Copying ${ASSIMP_DLL_NAME} to output directory"
        )
    endif()
    
    # Copy Assets folder to output directory
    # CRITICAL: Use copy_directory which will update all files, including shaders
    # This ensures shader changes are always copied to the build directory on every build
    add_custom_command(TARGET OpenGL_loader POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/Assets"
            "$<TARGET_FILE_DIR:OpenGL_loader>/Assets"
        COMMENT "Copying Assets directory (including shaders) to build directory"
    )
endif()
