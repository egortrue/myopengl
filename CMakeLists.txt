cmake_minimum_required(VERSION 3.16)
project(nevk)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
include(ConfigSafeGuards)
include(CTest)
include(Doctest)
include(Warnings)

option(ENABLE_WARNINGS_SETTINGS "Allow target_set_warnings to add flags and defines.
                                 Set this to OFF if you want to provide your own warning parameters." ON)

set(OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Set output directory for executable files
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIRECTORY})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${OUTPUT_DIRECTORY})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${OUTPUT_DIRECTORY})

if (MINGW)
    set(IMGUI_FILES
            external/imgui/src/imgui.cpp
            external/imgui/src/imgui_demo.cpp
            external/imgui/src/imgui_draw.cpp
            external/imgui/src/imgui_widgets.cpp
            external/imgui/src/imgui_impl_glfw.cpp
            external/imgui/src/imgui_impl_vulkan.cpp
            )
endif ()

######## Packages
# Vulkan
find_package(Vulkan REQUIRED)

# Shader Manager
set(SHADERMANAGER_SOURCES
        include/shadermanager/shadermanager.h
        src/shadermanager/shadermanager.cpp
        )
set(SHADERLIB_NAME shadermanager)
add_library(${SHADERLIB_NAME} OBJECT ${SHADERMANAGER_SOURCES})
target_include_directories(${SHADERLIB_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include/)
target_include_directories(${SHADERLIB_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include/shadermanager)
target_include_directories(${SHADERLIB_NAME} PUBLIC external/slang/include)
if (MSVC)
    target_link_libraries(${SHADERLIB_NAME} PUBLIC "${CMAKE_SOURCE_DIR}/external/slang/slang.lib")
elseif (APPLE)
    target_link_libraries(${SHADERLIB_NAME} PUBLIC "${CMAKE_SOURCE_DIR}/external/slang/libslang.dylib")
    target_link_libraries(${SHADERLIB_NAME} PUBLIC "${CMAKE_SOURCE_DIR}/external/slang/libslang-glslang.dylib")
endif ()

# Scene
set(SCENE_SOURCES
        include/scene/scene.h
        include/scene/camera.h
        src/scene/scene.cpp
        src/scene/camera.cpp
        )
set(SCENELIB_NAME scene)
add_library(${SCENELIB_NAME} OBJECT ${SCENE_SOURCES})
target_include_directories(${SCENELIB_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include/)
target_include_directories(${SCENELIB_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include/scene)
target_include_directories(${SCENELIB_NAME} PUBLIC external/glm)

# UI
set(UI_SOURCES
        include/ui/ui.h
        src/ui/ui.cpp

        # Imgui
        external/imgui/include
        external/imgui/src/imgui.cpp
        external/imgui/src/imgui_demo.cpp
        external/imgui/src/imgui_draw.cpp
        external/imgui/src/imgui_widgets.cpp
        external/imgui/src/imgui_tables.cpp
        external/imgui/src/imgui_impl_vulkan.cpp
        external/imgui/src/imgui_impl_glfw.cpp

        external/imguiFD/ImGuiFileDialog.cpp)
set(UILIB_NAME ui)
add_library(${UILIB_NAME} OBJECT ${UI_SOURCES})
target_include_directories(${UILIB_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include/)
target_include_directories(${UILIB_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include/ui)
target_include_directories(${UILIB_NAME} PUBLIC external/imgui/include)
target_include_directories(${UILIB_NAME} PRIVATE external/glfw/include)
target_include_directories(${UILIB_NAME} PUBLIC external/glm)
if (MSVC)
    target_link_libraries(${UILIB_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/external/glfw/glfw3.lib")
endif()
## Vulkan
target_include_directories(${UILIB_NAME} PUBLIC Vulkan::Vulkan)
target_link_libraries(${UILIB_NAME} PUBLIC Vulkan::Vulkan)

# Resource Manager lib
set(RESMNGR_SOURCES
        include/resourcemanager/resourcemanager.h
        src/resourcemanager/resourcemanager.cpp
        )
set(RESMNGR_NAME resourcemanager)
add_library(${RESMNGR_NAME} OBJECT ${RESMNGR_SOURCES})
target_include_directories(${RESMNGR_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include/)
target_include_directories(${RESMNGR_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include/resourcemanager)
## VMA
target_include_directories(${RESMNGR_NAME} PUBLIC external/vma/src)
## Vulkan
target_include_directories(${RESMNGR_NAME} PUBLIC Vulkan::Vulkan)
target_link_libraries(${RESMNGR_NAME} PUBLIC Vulkan::Vulkan)

# Render lib
set(RENDER_SOURCES
        include/render/gbuffer.h
        include/render/render.h
        include/render/renderpass.h
        include/render/gbufferpass.h
        include/render/computepass.h
        include/render/depthpass.h
        include/render/debugUtils.h
        src/render/render.cpp
        src/render/renderpass.cpp
        src/render/gbufferpass.cpp
        src/render/computepass.cpp
        src/render/depthpass.cpp
        src/render/debugUtils.cpp
        )
set(RENDERLIB_NAME render)
add_library(${RENDERLIB_NAME} OBJECT ${RENDER_SOURCES})
target_include_directories(${RENDERLIB_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include/)
target_include_directories(${RENDERLIB_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include/render)

# Model loader lib
set(MODELLIB_SOURCES
        include/modelloader/modelloader.h
        src/modelloader/modelloader.cpp)
set(MODELLIB_NAME modelloader)
add_library(${MODELLIB_NAME} OBJECT ${MODELLIB_SOURCES})
target_include_directories(${MODELLIB_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include/)
target_include_directories(${MODELLIB_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include/modelloader)
target_link_libraries(${MODELLIB_NAME} PUBLIC ${SCENELIB_NAME})
target_include_directories(${MODELLIB_NAME} PUBLIC external/tinygltf)

set(TEXTURELIB_SOURCES
        include/texturemanager/texturemanager.h
        src/texturemanager/texturemanager.cpp)
set(TEXTURELIB_NAME texturemanager)
add_library(${TEXTURELIB_NAME} OBJECT ${TEXTURELIB_SOURCES})
target_include_directories(${TEXTURELIB_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include/)
target_include_directories(${TEXTURELIB_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include/texturemanager)
target_link_libraries(${TEXTURELIB_NAME} PUBLIC ${SCENELIB_NAME} ${MODELLIB_NAME})

## Vulkan
target_include_directories(${RENDERLIB_NAME} PUBLIC Vulkan::Vulkan)
target_link_libraries(${RENDERLIB_NAME} PUBLIC Vulkan::Vulkan)
target_include_directories(${MODELLIB_NAME} PUBLIC Vulkan::Vulkan)
target_link_libraries(${MODELLIB_NAME} PUBLIC Vulkan::Vulkan)

## GLFW
target_include_directories(${RENDERLIB_NAME} PUBLIC external/glfw/include)
if (MSVC)
    target_link_libraries(${RENDERLIB_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/external/glfw/glfw3.lib")
elseif (MINGW)
    target_link_libraries(${RENDERLIB_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/external/glfw/libglfw3.a")
elseif (APPLE)
    find_package(glfw3 3.3 REQUIRED)
    include_directories(${glfw3_INCLUDE_DIRS})
    target_link_libraries(${RENDERLIB_NAME} PUBLIC glfw)
endif ()

## GLM (Mathematics)
target_include_directories(${RENDERLIB_NAME} PUBLIC external/glm)
target_include_directories(${MODELLIB_NAME} PUBLIC external/glm)

# TOL (TinyObjLoader)
target_include_directories(${RENDERLIB_NAME} PUBLIC external/tol)
target_include_directories(${MODELLIB_NAME} PUBLIC external/tol)

# STB (image loader)
target_include_directories(${RENDERLIB_NAME} PUBLIC external/stb)
target_include_directories(${TEXTURELIB_NAME} PUBLIC external/stb)

# imguiFD
target_include_directories(${RENDERLIB_NAME} PUBLIC external/imguiFD)
target_include_directories(${UILIB_NAME} PUBLIC external/imguiFD)

target_link_libraries(${RENDERLIB_NAME} PUBLIC doctest)
target_link_libraries(${RENDERLIB_NAME} PUBLIC ${RESMNGR_NAME})
target_link_libraries(${RENDERLIB_NAME} PUBLIC ${SHADERLIB_NAME})
target_link_libraries(${RENDERLIB_NAME} PUBLIC ${SCENELIB_NAME})
target_link_libraries(${RENDERLIB_NAME} PUBLIC ${MODELLIB_NAME})
target_link_libraries(${RENDERLIB_NAME} PUBLIC ${UILIB_NAME})
target_link_libraries(${RENDERLIB_NAME} PUBLIC ${TEXTURELIB_NAME})
target_set_warnings(${RENDERLIB_NAME} ENABLE ALL AS_ERROR ALL DISABLE Annoying)

# Application
add_executable(${PROJECT_NAME} app/main.cpp)
target_link_libraries(${PROJECT_NAME} PUBLIC ${RESMNGR_NAME} ${SHADERLIB_NAME} ${SCENELIB_NAME} ${RENDERLIB_NAME} ${UILIB_NAME} ${MODELLIB_NAME} ${TEXTURELIB_NAME})  # Link the executable to library (if it uses it).
target_set_warnings(${PROJECT_NAME} ENABLE ALL AS_ERROR ALL DISABLE Annoying) # Set warnings (if needed).


# Set working directory
if (MSVC)
    set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${OUTPUT_DIRECTORY}")
elseif (APPLE)
    set_property(TARGET ${PROJECT_NAME} PROPERTY XCODE_SCHEME_WORKING_DIRECTORY "${OUTPUT_DIRECTORY}")
endif ()

# Create symbolic links in final directory before building
add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink
        "${CMAKE_SOURCE_DIR}/shaders"
        "${OUTPUT_DIRECTORY}/shaders"
        )
add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink
        "${CMAKE_SOURCE_DIR}/misc"
        "${OUTPUT_DIRECTORY}/misc"
        )

# Copy slang dlls to final directory after building
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_SOURCE_DIR}/external/slang/slang.dll"
        "${OUTPUT_DIRECTORY}"
        )
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_SOURCE_DIR}/external/slang/slang-glslang.dll"
        "${OUTPUT_DIRECTORY}"
        )

##########################################
# External 3rd-party libraries

# Slang (Shader manager)
# add_subdirectory(shaders)
# target_link_libraries(${PROJECT_NAME} ShaderManager)

# ImGUI
# target_include_directories(${PROJECT_NAME} PRIVATE external/imgui/include)
# if (MSVC)
#	target_link_libraries(${PROJECT_NAME} "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/imgui.lib")
# endif()

# VMA (Vulkan Memory Allocator)
# target_include_directories(${PROJECT_NAME} PRIVATE external/vma/src)

# GLM (OpenGL Mathematics)
# target_include_directories(${PROJECT_NAME} PRIVATE external/glm)


# TOL (TinyObjLoader)
# target_include_directories(${PROJECT_NAME} PRIVATE external/tol)

# SPDLOG
# target_include_directories(${PROJECT_NAME} PRIVATE external/spdlog/include)

# CXXOPTS
target_include_directories(${PROJECT_NAME} PRIVATE external/cxxopts/include)

option(NEVK_TESTS "enable tests" ON)

if (NOT NEVK_TESTS)
    message(STATUS "tests are disabled")
else ()
    add_subdirectory(tests)
endif ()
