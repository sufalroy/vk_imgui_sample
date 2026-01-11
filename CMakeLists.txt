cmake_minimum_required(VERSION 3.27 FATAL_ERROR)

project(VulkanImGuiSample
    VERSION 1.0.0
    DESCRIPTION "Vulkan ImGui Sample Application"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake")

option(ENABLE_STATIC_ANALYZERS "Enable static analyzers" ON)
option(ENABLE_HARDENING "Enable compiler hardening" ON)
option(ENABLE_SANITIZERS "Enable sanitizers" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(ENABLE_MSAN "Enable MemorySanitizer" OFF)
option(ENABLE_IPO "Enable IPO/LTO" ON)
option(ENABLE_PCH "Enable precompiled headers" ON)

if((ENABLE_SANITIZERS AND ENABLE_TSAN) OR (ENABLE_SANITIZERS AND ENABLE_MSAN) OR (ENABLE_TSAN AND ENABLE_MSAN))
    message(FATAL_ERROR "Only one sanitizer type can be enabled at a time")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(cmake/CPM.cmake)
include(cmake/CompilerWarnings.cmake)
include(cmake/Hardening.cmake)
include(cmake/Sanitizers.cmake)
include(cmake/StaticAnalyzers.cmake)

find_package(Vulkan 1.4.309 REQUIRED COMPONENTS glslc)
if(NOT Vulkan_FOUND)
    message(FATAL_ERROR "Vulkan SDK not found")
endif()

CPMAddPackage(
    NAME glfw
    GITHUB_REPOSITORY glfw/glfw
    GIT_TAG 3.4
    OPTIONS
        "GLFW_BUILD_EXAMPLES OFF"
        "GLFW_BUILD_TESTS OFF"
        "GLFW_BUILD_DOCS OFF"
        "GLFW_INSTALL ON"
        "GLFW_VULKAN_STATIC OFF"
        "GLFW_USE_WAYLAND OFF"
)


CPMAddPackage(
    NAME imgui
    GITHUB_REPOSITORY ocornut/imgui
    GIT_TAG docking
    DOWNLOAD_ONLY YES
)

if(imgui_ADDED)
    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
    )
    
    target_include_directories(imgui PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )
    
    target_link_libraries(imgui PUBLIC Vulkan::Vulkan glfw)
    
    target_compile_definitions(imgui PUBLIC
        IMGUI_DISABLE_OBSOLETE_FUNCTIONS
        IMGUI_DISABLE_OBSOLETE_KEYIO
    )
    
    target_compile_options(imgui PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W0>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-w>
    )
    
    set_target_properties(imgui PROPERTIES
        FOLDER "External"
        CXX_STANDARD 20
        POSITION_INDEPENDENT_CODE ON
    )
endif()

if(TARGET glfw)
    set_target_properties(glfw PROPERTIES FOLDER "External")
    target_compile_options(glfw PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W0>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-w>
    )
endif()

add_executable(${PROJECT_NAME} src/main.cpp)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan glfw imgui)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    VK_USE_PLATFORM_WIN32_KHR
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
    VULKAN_HPP_NO_CONSTRUCTORS
    VULKAN_HPP_NO_SPACESHIP_OPERATOR
    $<$<CONFIG:Debug>:DEBUG_BUILD>
    $<$<CONFIG:Debug>:VULKAN_HPP_ASSERT=assert>
    $<$<CONFIG:Release>:NDEBUG>
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    VC_EXTRALEAN
    PROJECT_NAME="${PROJECT_NAME}"
    PROJECT_VERSION="${PROJECT_VERSION}"
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
    WIN32_EXECUTABLE $<CONFIG:Release>
)

if(ENABLE_PCH)
    target_precompile_headers(${PROJECT_NAME} PRIVATE
        <algorithm>
        <array>
        <cstddef>
        <cstdint>
        <expected>
        <functional>
        <limits>
        <memory>
        <optional>
        <ranges>
        <span>
        <stdexcept>
        <string>
        <string_view>
        <vector>
    )
endif()

if(ENABLE_IPO AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
    if(ipo_supported)
        set_target_properties(${PROJECT_NAME} PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
endif()

set_project_warnings(${PROJECT_NAME})
set_project_hardening(${PROJECT_NAME})

if(ENABLE_SANITIZERS OR ENABLE_TSAN OR ENABLE_MSAN)
    enable_sanitizers(${PROJECT_NAME})
endif()

if(ENABLE_STATIC_ANALYZERS)
    enable_static_analyzers(${PROJECT_NAME})
endif()

find_program(CLANG_FORMAT_EXE NAMES clang-format)
if(CLANG_FORMAT_EXE)
    file(GLOB_RECURSE ALL_SOURCE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
    )
    
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXE} -i ${ALL_SOURCE_FILES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    
    add_custom_target(format-check
        COMMAND ${CLANG_FORMAT_EXE} --dry-run --Werror ${ALL_SOURCE_FILES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()

include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})