# CMakeList.txt : CMake project for vulkan-imgui-app, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_executable (vulkan-imgui-app "vulkan-imgui-app.cpp" "vulkan-imgui-app.h")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET vulkan-imgui-app PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
