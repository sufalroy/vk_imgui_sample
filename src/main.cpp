#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <cstdlib>
#include <print>

int main() {
    std::println("Initializing GLFW..");

    if (glfwInit() != GLFW_TRUE) {
        std::println("Failed to initialize GLFW");
        return EXIT_FAILURE;
    }

    std::println("GLFW version: {}", glfwGetVersionString());

    if (glfwVulkanSupported() != GLFW_TRUE) {
        std::println("Vulkan is not supported on this system");
        glfwTerminate();
        return EXIT_FAILURE;
    }

    std::println("Vulkan is supported");

    std::uint32_t extensionCount = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions(&extensionCount);

    std::println("Required Vulkan extensions: {}", extensionCount);
    for (uint32_t i = 0; i < extensionCount; ++i) {
        std::println(" - {}", extensions[i]);
    }

    uint32_t vulkanVersion = 0;
    if (vkEnumerateInstanceVersion(&vulkanVersion) == VK_SUCCESS) {
        std::uint32_t major = VK_VERSION_MAJOR(vulkanVersion);
        std::uint32_t minor = VK_VERSION_MINOR(vulkanVersion);
        std::uint32_t patch = VK_VERSION_PATCH(vulkanVersion);
        std::println("Vulkan Instance Version: {}.{}.{}", major, minor, patch);
    }

    std::println("Initializing ImGui context...");
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    std::println("ImGui version: {}", ImGui::GetVersion());

    ImGui::DestroyContext();
    glfwTerminate();

    std::println("All dependencies verified successfully");
    return EXIT_SUCCESS;
}