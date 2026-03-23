cmake_minimum_required(VERSION 3.27)

project(VulkanImGuiSample
    VERSION 0.1.0
    DESCRIPTION "Vulkan + ImGui sample application"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_EXPORT_COMPILE_COMMANDS  ON)

set_property(GLOBAL PROPERTY USE_FOLDERS              ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake")
 

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

option(ENABLE_CPP20_MODULE       "Use Vulkan C++20 module instead of headers"      OFF)
option(ENABLE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors"               ON)
option(ENABLE_HARDENING          "Enable security hardening flags"                  ON)
option(ENABLE_SANITIZERS         "Enable AddressSanitizer + UBSanitizer"           ON)
option(ENABLE_TSAN               "Enable ThreadSanitizer (Clang/GCC only)"         OFF)
option(ENABLE_MSAN               "Enable MemorySanitizer (Clang only)"             OFF)
option(ENABLE_STATIC_ANALYZERS   "Enable clang-tidy / cppcheck / MSVC /analyze"   ON)
option(ENABLE_IPO                "Enable interprocedural optimisation (LTO)"        ON)
option(ENABLE_PCH                "Enable precompiled headers"                       ON)
 
if((ENABLE_SANITIZERS AND ENABLE_TSAN)
        OR (ENABLE_SANITIZERS AND ENABLE_MSAN)
        OR (ENABLE_TSAN AND ENABLE_MSAN))
    message(FATAL_ERROR
        "Only one sanitizer group can be enabled at a time "
        "(ENABLE_SANITIZERS / ENABLE_TSAN / ENABLE_MSAN)")
endif()

include(CompilerWarnings)
include(Hardening)
include(Sanitizers)
include(StaticAnalyzers)

if(ENABLE_IPO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT _ipo_ok OUTPUT _ipo_msg)
    if(NOT _ipo_ok)
        message(WARNING "IPO/LTO not supported: ${_ipo_msg}")
        set(ENABLE_IPO OFF)
    else()
        message(STATUS "IPO/LTO: supported")
    endif()
endif()

find_package(Vulkan 1.4 REQUIRED)
 
find_program(SLANGC_EXECUTABLE slangc
    HINTS "$ENV{VULKAN_SDK}/Bin"
    REQUIRED
)
message(STATUS "slangc: ${SLANGC_EXECUTABLE}")

include(Dependencies.cmake)

set(SHADER_SRC "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/triangle.slang")
set(SHADER_SPV "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/triangle.spv")

add_custom_command(
    OUTPUT  "${SHADER_SPV}"
    COMMAND "${SLANGC_EXECUTABLE}"
            "${SHADER_SRC}"
            -target              spirv
            -profile             spirv_1_4
            -emit-spirv-directly
            -fvk-use-entrypoint-name
            -entry vertMain
            -entry fragMain
            -o "${SHADER_SPV}"
    DEPENDS "${SHADER_SRC}"
    COMMENT "slangc: triangle.slang -> triangle.spv"
    VERBATIM
)
add_custom_target(vk_shaders DEPENDS "${SHADER_SPV}")

add_executable(${PROJECT_NAME} src/main.cpp)
 
add_dependencies(${PROJECT_NAME} vk_shaders)
 
target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Vulkan::cppm
    glfw
    imgui
    glm::glm
)
 
target_compile_definitions(${PROJECT_NAME} PRIVATE
    VULKAN_HPP_NO_EXCEPTIONS
    VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS 
    $<$<PLATFORM_ID:Windows>:VK_USE_PLATFORM_WIN32_KHR>
    $<$<PLATFORM_ID:Windows>:NOMINMAX>
    $<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>
    $<$<PLATFORM_ID:Windows>:VC_EXTRALEAN>
    $<$<CONFIG:Debug>:DEBUG_BUILD>
    $<$<CONFIG:Release>:NDEBUG>
    PROJECT_NAME="${PROJECT_NAME}"
    PROJECT_VERSION="${PROJECT_VERSION}"
    $<$<BOOL:${ENABLE_CPP20_MODULE}>:USE_CPP20_MODULES=1>
)
 
if(ENABLE_IPO)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        INTERPROCEDURAL_OPTIMIZATION            TRUE   # Release / RelWithDebInfo
        INTERPROCEDURAL_OPTIMIZATION_DEBUG      FALSE  # Never LTO in Debug
        INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL TRUE
    )
endif()
 
set_project_warnings(${PROJECT_NAME} "${ENABLE_WARNINGS_AS_ERRORS}")
set_project_hardening(${PROJECT_NAME})
enable_sanitizers(${PROJECT_NAME})
enable_static_analyzers(${PROJECT_NAME})
 
set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    WIN32_EXECUTABLE "$<$<CONFIG:Release>:TRUE>"
)
 
find_program(CLANG_FORMAT_EXE NAMES clang-format)
if(CLANG_FORMAT_EXE)
    file(GLOB_RECURSE _format_sources
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
    )
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXE} -i ${_format_sources}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "clang-format: formatting sources"
    )
    add_custom_target(format-check
        COMMAND ${CLANG_FORMAT_EXE} --dry-run --Werror ${_format_sources}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "clang-format: checking formatting"
    )
    set_target_properties(format format-check PROPERTIES FOLDER "Tooling")
    message(STATUS "clang-format: ${CLANG_FORMAT_EXE}")
else()
    message(STATUS "clang-format: not found")
endif()

include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}"
)