include(cmake/CPM.cmake)

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
set(GLFW_VULKAN_STATIC  OFF CACHE BOOL "" FORCE)

CPMAddPackage(
    NAME              glfw
    GITHUB_REPOSITORY glfw/glfw
    GIT_TAG           3.4
)
if(TARGET glfw)
    set_target_properties(glfw PROPERTIES FOLDER "External")
    if(MSVC)
        target_compile_options(glfw PRIVATE /W0)
    else()
        target_compile_options(glfw PRIVATE -w)
    endif()
endif()

set(GLM_BUILD_TESTS   OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_INSTALL OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING     OFF CACHE BOOL "" FORCE)

CPMAddPackage(
    NAME              glm
    GITHUB_REPOSITORY g-truc/glm
    GIT_TAG           1.0.1
)

CPMAddPackage(
    NAME              imgui
    GITHUB_REPOSITORY ocornut/imgui
    GIT_TAG           docking
    DOWNLOAD_ONLY     YES
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
    if(MSVC)
        target_compile_options(imgui PRIVATE /W0)
    else()
        target_compile_options(imgui PRIVATE -w)
    endif()
    set_target_properties(imgui PROPERTIES
        FOLDER                    "External"
        CXX_STANDARD              17
        POSITION_INDEPENDENT_CODE ON
    )
endif()