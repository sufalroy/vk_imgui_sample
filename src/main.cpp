#include <vulkan/vulkan_raii.hpp>

#include <algorithm>
#include <array>
#include <expected>
#include <limits>
#include <optional>
#include <ranges>
#include <span>
#include <string_view>
#include <vector>

#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <print>
#include <unordered_set>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;
constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

constexpr std::array VALIDATION_LAYERS = {"VK_LAYER_KHRONOS_validation"};
constexpr std::array DEVICE_EXTENSIONS = {vk::KHRSwapchainExtensionName};

#ifdef NDEBUG
constexpr bool ENABLE_VALIDATION = false;
#else
constexpr bool ENABLE_VALIDATION = true;
#endif

enum class ErrorCode {
    GlfwInitFailed,
    WindowCreationFailed,
    ValidationLayerNotSupported,
    ExtensionNotSupported,
    VulkanInstanceCreationFailed,
    DebugMessengerCreationFailed,
    SurfaceCreationFailed,
    NoPhysicalDevicesFound,
    NoSuitableGpuFound,
    DeviceExtensionNotSupported,
    NoGraphicsQueueFamily,
    NoPresentQueueFamily,
    DeviceCreationFailed,
    QueueRetrievalFailed,
    SwapchainCreationFailed,
    ImageViewCreationFailed,
    FileReadFailed,
    ShaderModuleCreationFailed,
    PipelineLayoutCreationFailed,
    PipelineCreationFailed,
    CommandPoolCreationFailed,
    CommandBufferAllocationFailed,
    SynchronizationObjectCreationFailed,
    QueueSubmitFailed,
    AcquireImageFailed,
    PresentFailed,
    BufferCreationFailed,
    MemoryTypeNotFound,
    MemoryAllocationFailed,
    DescriptorSetLayoutCreationFailed,
    DescriptorPoolCreationFailed,
    DescriptorSetAllocationFailed,
    ImageCreationFailed,
    TextureLoadFailed,
    UnsupportedLayoutTransition,
    SamplerCreationFailed
};

struct Error {
    ErrorCode code;
    std::string message;
    std::optional<vk::Result> vk_result;

    [[nodiscard]] auto what() const noexcept -> std::string_view {
        return message;
    }
};

template <typename T = void>
using Result = std::expected<T, Error>;

struct QueueFamilyIndices {
    std::optional<uint32_t> graphics;
    std::optional<uint32_t> present;

    [[nodiscard]] constexpr auto is_complete() const noexcept -> bool {
        return graphics.has_value() && present.has_value();
    }

    [[nodiscard]] auto unique_families() const noexcept -> std::vector<uint32_t> {
        std::unordered_set<uint32_t> unique{};
        if (graphics)
            unique.insert(*graphics);
        if (present)
            unique.insert(*present);
        return {unique.begin(), unique.end()};
    }
};

struct SwapChainSupportDetails {
    vk::SurfaceCapabilitiesKHR capabilities{};
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR> present_modes;

    [[nodiscard]] constexpr auto is_adequate() const noexcept -> bool {
        return !formats.empty() && !present_modes.empty();
    }
};

struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;
    glm::vec2 tex_coord;

    [[nodiscard]] static constexpr auto get_binding_description() noexcept -> vk::VertexInputBindingDescription {
        return vk::VertexInputBindingDescription{
            .binding = 0,
            .stride = sizeof(Vertex),
            .inputRate = vk::VertexInputRate::eVertex,
        };
    }

    [[nodiscard]] static constexpr auto
    get_attribute_descriptions() noexcept -> std::array<vk::VertexInputAttributeDescription, 3> {
        return {vk::VertexInputAttributeDescription{
                    .location = 0,
                    .binding = 0,
                    .format = vk::Format::eR32G32Sfloat,
                    .offset = offsetof(Vertex, pos),
                },
                vk::VertexInputAttributeDescription{
                    .location = 1,
                    .binding = 0,
                    .format = vk::Format::eR32G32B32Sfloat,
                    .offset = offsetof(Vertex, color),
                },
                vk::VertexInputAttributeDescription{
                    .location = 2,
                    .binding = 0,
                    .format = vk::Format::eR32G32Sfloat,
                    .offset = offsetof(Vertex, tex_coord),
                }};
    }
};

const std::vector<Vertex> VERTICES = {{.pos = {-0.5F, -0.5F}, .color = {1.0F, 0.0F, 0.0F}, .tex_coord = {1.0F, 0.0F}},
                                      {.pos = {0.5F, -0.5F}, .color = {0.0F, 1.0F, 0.0F}, .tex_coord = {0.0F, 0.0F}},
                                      {.pos = {0.5F, 0.5F}, .color = {0.0F, 0.0F, 1.0F}, .tex_coord = {0.0F, 1.0F}},
                                      {.pos = {-0.5F, 0.5F}, .color = {1.0F, 1.0F, 1.0F}, .tex_coord = {1.0F, 1.0F}}};

const std::vector<uint16_t> INDICES = {0, 1, 2, 2, 3, 0};

struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

struct BufferAllocation {
    vk::raii::Buffer buffer{nullptr};
    vk::raii::DeviceMemory memory{nullptr};
};

struct ImageAllocation {
    vk::raii::Image image{nullptr};
    vk::raii::DeviceMemory memory{nullptr};
};

class VkApplication {
public:
    VkApplication() = default;

    ~VkApplication() noexcept {
        if (window_) {
            glfwDestroyWindow(window_);
        }
        glfwTerminate();
    }

    VkApplication(const VkApplication&) = delete;
    VkApplication& operator=(const VkApplication&) = delete;
    VkApplication(VkApplication&&) = delete;
    VkApplication& operator=(VkApplication&&) = delete;

    auto run() noexcept -> Result<> {
        if (auto result = init_window(); !result) {
            return std::unexpected(result.error());
        }

        if (auto result = init_vulkan(); !result) {
            return std::unexpected(result.error());
        }

        main_loop();
        return {};
    }

private:
    auto init_window() noexcept -> Result<> {
        if (!glfwInit()) {
            return std::unexpected(Error{.code = ErrorCode::GlfwInitFailed, .message = "Failed to initialize GLFW"});
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        window_ = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan Application", nullptr, nullptr);
        if (!window_) {
            glfwTerminate();
            return std::unexpected(
                Error{.code = ErrorCode::WindowCreationFailed, .message = "Failed to create GLFW window"});
        }

        glfwSetWindowUserPointer(window_, this);
        glfwSetFramebufferSizeCallback(window_, glfw_framebuffer_resize_callback);
        glfwSetKeyCallback(window_, glfw_key_callback);

        return {};
    }

    auto init_vulkan() noexcept -> Result<> {
        if (auto result = create_instance(); !result) {
            return std::unexpected(result.error());
        }

        if constexpr (ENABLE_VALIDATION) {
            if (auto result = create_debug_messenger(); !result) {
                return std::unexpected(result.error());
            }
        }

        if (auto result = create_surface(); !result) {
            return std::unexpected(result.error());
        }

        if (auto result = pick_physical_device(); !result) {
            return std::unexpected(result.error());
        }

        if (auto result = create_logical_device(); !result) {
            return std::unexpected(result.error());
        }

        if (auto result = create_swap_chain(); !result) {
            return std::unexpected(result.error());
        }

        if (auto result = create_swapchain_image_views(); !result) {
            return std::unexpected(result.error());
        }

        if (auto result = create_descriptor_set_layout(); !result) {
            return std::unexpected(result.error());
        }

        if (auto result = create_graphics_pipeline(); !result) {
            return std::unexpected(result.error());
        }

        if (auto result = create_command_pool(); !result) {
            return std::unexpected(result.error());
        }

        if (auto result = create_texture_image(); !result) {
            return std::unexpected(result.error());
        }

        if (auto result = create_texture_image_view(); !result) {
            return std::unexpected(result.error());
        }

        if (auto result = create_texture_sampler(); !result) {
            return std::unexpected(result.error());
        }

        if (auto result = create_vertex_buffer(); !result) {
            return std::unexpected(result.error());
        }

        if (auto result = create_index_buffer(); !result) {
            return std::unexpected(result.error());
        }

        if (auto result = create_uniform_buffers(); !result) {
            return std::unexpected(result.error());
        }

        if (auto result = create_descriptor_pool(); !result) {
            return std::unexpected(result.error());
        }

        if (auto result = create_descriptor_sets(); !result) {
            return std::unexpected(result.error());
        }

        if (auto result = create_command_buffers(); !result) {
            return std::unexpected(result.error());
        }

        if (auto result = create_sync_objects(); !result) {
            return std::unexpected(result.error());
        }

        return {};
    }

    auto create_instance() noexcept -> Result<> {
        constexpr vk::ApplicationInfo app_info{
            .pApplicationName = "Vulkan Application",
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName = "No Engine",
            .engineVersion = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion = vk::ApiVersion14,
        };

        auto required_layers = get_required_layers();
        if (auto result = validate_layers(required_layers); !result) {
            return std::unexpected(result.error());
        }

        auto required_extensions = get_required_extensions();
        if (auto result = validate_extensions(required_extensions); !result) {
            return std::unexpected(result.error());
        }

        vk::InstanceCreateInfo create_info{
            .pApplicationInfo = &app_info,
            .enabledLayerCount = static_cast<uint32_t>(required_layers.size()),
            .ppEnabledLayerNames = required_layers.data(),
            .enabledExtensionCount = static_cast<uint32_t>(required_extensions.size()),
            .ppEnabledExtensionNames = required_extensions.data(),
        };

        auto instance_result = context_.createInstance(create_info);
        if (!instance_result.has_value()) {
            return std::unexpected(Error{
                .code = ErrorCode::VulkanInstanceCreationFailed,
                .message = std::format("Failed to create Vulkan instance: {}", vk::to_string(instance_result.error())),
                .vk_result = instance_result.error()});
        }

        instance_ = std::move(*instance_result);
        return {};
    }

    auto create_debug_messenger() noexcept -> Result<> {
        constexpr vk::DebugUtilsMessageSeverityFlagsEXT severity_flags(
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);

        constexpr vk::DebugUtilsMessageTypeFlagsEXT message_type_flags(
            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
            vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);

        vk::DebugUtilsMessengerCreateInfoEXT messenger_create_info{
            .messageSeverity = severity_flags, .messageType = message_type_flags, .pfnUserCallback = &debug_callback};

        auto messenger_result = instance_.createDebugUtilsMessengerEXT(messenger_create_info);
        if (!messenger_result.has_value()) {
            return std::unexpected(Error{
                .code = ErrorCode::DebugMessengerCreationFailed,
                .message = std::format("Failed to create debug messenger: {}", vk::to_string(messenger_result.error())),
                .vk_result = messenger_result.error()});
        }

        debug_messenger_ = std::move(*messenger_result);
        return {};
    }

    auto create_surface() noexcept -> Result<> {
        VkSurfaceKHR surface_handle{};
        if (glfwCreateWindowSurface(*instance_, window_, nullptr, &surface_handle) != VK_SUCCESS) {
            return std::unexpected(
                Error{.code = ErrorCode::SurfaceCreationFailed, .message = "Failed to create window surface"});
        }

        surface_ = vk::raii::SurfaceKHR{instance_, surface_handle};
        return {};
    }

    auto pick_physical_device() noexcept -> Result<> {
        auto devices_result = instance_.enumeratePhysicalDevices();
        if (!devices_result.has_value()) {
            return std::unexpected(Error{.code = ErrorCode::NoPhysicalDevicesFound,
                                         .message = std::format("Failed to enumerate physical devices: {}",
                                                                vk::to_string(devices_result.error())),
                                         .vk_result = devices_result.error()});
        }

        auto& devices = *devices_result;
        if (devices.empty()) {
            return std::unexpected(Error{.code = ErrorCode::NoPhysicalDevicesFound,
                                         .message = "No physical devices with Vulkan support found"});
        }

        for (auto& device : devices) {
            if (!is_device_suitable(device)) {
                continue;
            }

            const auto device_properties = device.getProperties();
            std::println("Device Name: {}", std::string_view(device_properties.deviceName));

            auto indices_result = find_queue_families(device);
            if (!indices_result.has_value() || !indices_result->is_complete()) {
                continue;
            }

            physical_device_ = std::move(device);
            queue_family_indices_ = *indices_result;
            return {};
        }

        return std::unexpected(Error{.code = ErrorCode::NoSuitableGpuFound,
                                     .message = "Failed to find a suitable GPU with required features"});
    }

    auto create_logical_device() noexcept -> Result<> {
        constexpr float queue_priority = 1.0F;
        auto unique_families = queue_family_indices_.unique_families();

        std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
        queue_create_infos.reserve(unique_families.size());

        for (uint32_t family_index : unique_families) {
            queue_create_infos.push_back(
                {.queueFamilyIndex = family_index, .queueCount = 1, .pQueuePriorities = &queue_priority});
        }

        vk::StructureChain<vk::PhysicalDeviceFeatures2,
                           vk::PhysicalDeviceVulkan11Features,
                           vk::PhysicalDeviceVulkan13Features,
                           vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
            feature_chain = {
                {.features = {.samplerAnisotropy = vk::True}},
                {.shaderDrawParameters = vk::True},
                {.synchronization2 = vk::True, .dynamicRendering = vk::True},
                {.extendedDynamicState = vk::True},
            };

        vk::DeviceCreateInfo device_create_info{
            .pNext = &feature_chain.get<vk::PhysicalDeviceFeatures2>(),
            .queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size()),
            .pQueueCreateInfos = queue_create_infos.data(),
            .enabledExtensionCount = static_cast<uint32_t>(DEVICE_EXTENSIONS.size()),
            .ppEnabledExtensionNames = DEVICE_EXTENSIONS.data()};

        auto device_result = physical_device_.createDevice(device_create_info);
        if (!device_result.has_value()) {
            return std::unexpected(Error{
                .code = ErrorCode::DeviceCreationFailed,
                .message = std::format("Failed to create logical device: {}", vk::to_string(device_result.error())),
                .vk_result = device_result.error()});
        }

        device_ = std::move(*device_result);

        auto graphics_queue_result = device_.getQueue(*queue_family_indices_.graphics, 0);
        if (!graphics_queue_result.has_value()) {
            return std::unexpected(Error{.code = ErrorCode::QueueRetrievalFailed,
                                         .message = std::format("Failed to get graphics queue: {}",
                                                                vk::to_string(graphics_queue_result.error())),
                                         .vk_result = graphics_queue_result.error()});
        }

        graphics_queue_ = std::move(*graphics_queue_result);

        auto present_queue_result = device_.getQueue(*queue_family_indices_.present, 0);
        if (!present_queue_result.has_value()) {
            return std::unexpected(Error{
                .code = ErrorCode::QueueRetrievalFailed,
                .message = std::format("Failed to get present queue: {}", vk::to_string(present_queue_result.error())),
                .vk_result = present_queue_result.error()});
        }

        present_queue_ = std::move(*present_queue_result);
        return {};
    }

    auto create_swap_chain() noexcept -> Result<> {
        const auto support = query_swapchain_support(physical_device_);
        const auto surface_format = choose_swap_surface_format(support.formats);
        const auto present_mode = choose_swap_present_mode(support.present_modes);
        const auto extent = choose_swap_extent(support.capabilities);

        uint32_t image_count = support.capabilities.minImageCount + 1;

        if (support.capabilities.maxImageCount > 0) {
            image_count = std::min(image_count, support.capabilities.maxImageCount);
        }

        vk::SwapchainCreateInfoKHR create_info{
            .surface = *surface_,
            .minImageCount = image_count,
            .imageFormat = surface_format.format,
            .imageColorSpace = surface_format.colorSpace,
            .imageExtent = extent,
            .imageArrayLayers = 1,
            .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
            .preTransform = support.capabilities.currentTransform,
            .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
            .presentMode = present_mode,
            .clipped = vk::True,
        };

        const auto indices = queue_family_indices_.unique_families();
        if (queue_family_indices_.graphics != queue_family_indices_.present) {
            create_info.imageSharingMode = vk::SharingMode::eConcurrent;
            create_info.queueFamilyIndexCount = static_cast<uint32_t>(indices.size());
            create_info.pQueueFamilyIndices = indices.data();
        } else {
            create_info.imageSharingMode = vk::SharingMode::eExclusive;
        }

        auto result = device_.createSwapchainKHR(create_info);
        if (!result.has_value()) {
            return std::unexpected(
                Error{.code = ErrorCode::SwapchainCreationFailed,
                      .message = std::format("Failed to create swap chain: {}", vk::to_string(result.error())),
                      .vk_result = result.error()});
        }

        swapchain_ = std::move(*result);
        swapchain_images_ = swapchain_.getImages();
        swapchain_format_ = surface_format.format;
        swapchain_extent_ = extent;

        return {};
    }

    auto cleanup_swapchain() noexcept -> void {
        swapchain_image_views_.clear();
        swapchain_images_.clear();
        swapchain_ = nullptr;

        render_finished_semaphores_.clear();
    }

    auto recreate_swapchain() noexcept -> Result<> {
        int width{}, height{};
        glfwGetFramebufferSize(window_, &width, &height);
        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(window_, &width, &height);
            glfwWaitEvents();
        }

        device_.waitIdle();

        cleanup_swapchain();

        if (auto result = create_swap_chain(); !result) {
            return std::unexpected(result.error());
        }

        if (auto result = create_swapchain_image_views(); !result) {
            return std::unexpected(result.error());
        }

        render_finished_semaphores_.reserve(swapchain_images_.size());
        for (uint32_t i = 0; i < static_cast<uint32_t>(swapchain_images_.size()); ++i) {
            auto result = device_.createSemaphore(vk::SemaphoreCreateInfo{});
            if (!result.has_value()) {
                return std::unexpected(Error{
                    .code = ErrorCode::SynchronizationObjectCreationFailed,
                    .message = std::format(
                        "Failed to create render finished semaphore {}: {}", i, vk::to_string(result.error())),
                    .vk_result = result.error(),
                });
            }
            render_finished_semaphores_.emplace_back(std::move(*result));
        }

        return {};
    }

    [[nodiscard]] auto create_image_view(const vk::Image& image,
                                         vk::Format format) noexcept -> Result<vk::raii::ImageView> {
        vk::ImageViewCreateInfo create_info{
            .image = image,
            .viewType = vk::ImageViewType::e2D,
            .format = format,
            .components =
                {
                    .r = vk::ComponentSwizzle::eIdentity,
                    .g = vk::ComponentSwizzle::eIdentity,
                    .b = vk::ComponentSwizzle::eIdentity,
                    .a = vk::ComponentSwizzle::eIdentity,
                },
            .subresourceRange =
                {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
        };
        auto result = device_.createImageView(create_info);
        if (!result.has_value()) {
            return std::unexpected(
                Error{.code = ErrorCode::ImageViewCreationFailed,
                      .message = std::format("Failed to create image view: {}", vk::to_string(result.error())),
                      .vk_result = result.error()});
        }
        return std::move(*result);
    }

    auto create_swapchain_image_views() noexcept -> Result<> {
        swapchain_image_views_.clear();
        swapchain_image_views_.reserve(swapchain_images_.size());

        for (const auto& image : swapchain_images_) {
            auto view_result = create_image_view(image, swapchain_format_);
            if (!view_result.has_value()) {
                return std::unexpected(view_result.error());
            }

            swapchain_image_views_.emplace_back(std::move(*view_result));
        }

        return {};
    }

    auto create_descriptor_set_layout() noexcept -> Result<> {
        std::array bindings{
            vk::DescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = vk::DescriptorType::eUniformBuffer,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eVertex,
                .pImmutableSamplers = nullptr,

            },
            vk::DescriptorSetLayoutBinding{
                .binding = 1,
                .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eFragment,
                .pImmutableSamplers = nullptr,
            },
        };

        auto result = device_.createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo{
            .bindingCount = static_cast<uint32_t>(bindings.size()),
            .pBindings = bindings.data(),
        });
        if (!result.has_value()) {
            return std::unexpected(Error{
                .code = ErrorCode::DescriptorSetLayoutCreationFailed,
                .message = std::format("Failed to create descriptor set layout: {}", vk::to_string(result.error())),
                .vk_result = result.error(),
            });
        }

        descriptor_set_layout_ = std::move(*result);
        return {};
    }

    auto create_graphics_pipeline() noexcept -> Result<> {
        auto spirv = read_file("/home/dev/Laboratory/cpp_workspace/vk_imgui_sample/assets/shaders/triangle.spv");
        if (!spirv) {
            return std::unexpected(spirv.error());
        }

        auto shader_module = create_shader_module(*spirv);
        if (!shader_module) {
            return std::unexpected(shader_module.error());
        }

        const std::array shader_stages{vk::PipelineShaderStageCreateInfo{
                                           .stage = vk::ShaderStageFlagBits::eVertex,
                                           .module = *shader_module,
                                           .pName = "vertMain",
                                       },
                                       vk::PipelineShaderStageCreateInfo{
                                           .stage = vk::ShaderStageFlagBits::eFragment,
                                           .module = *shader_module,
                                           .pName = "fragMain",
                                       }};

        constexpr auto binding_desc = Vertex::get_binding_description();
        constexpr auto attribute_desc = Vertex::get_attribute_descriptions();

        const vk::PipelineVertexInputStateCreateInfo vertex_input_info{
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = &binding_desc,
            .vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_desc.size()),
            .pVertexAttributeDescriptions = attribute_desc.data(),
        };

        constexpr vk::PipelineInputAssemblyStateCreateInfo input_assembly_info{
            .topology = vk::PrimitiveTopology::eTriangleList, .primitiveRestartEnable = vk::False};

        constexpr vk::PipelineViewportStateCreateInfo viewport_state{
            .viewportCount = 1,
            .scissorCount = 1,
        };

        constexpr vk::PipelineRasterizationStateCreateInfo rasterizer{
            .depthClampEnable = vk::False,
            .rasterizerDiscardEnable = vk::False,
            .polygonMode = vk::PolygonMode::eFill,
            .cullMode = vk::CullModeFlagBits::eBack,
            .frontFace = vk::FrontFace::eCounterClockwise,
            .depthBiasEnable = vk::False,
            .lineWidth = 1.0F,
        };

        constexpr vk::PipelineMultisampleStateCreateInfo multisampling{
            .rasterizationSamples = vk::SampleCountFlagBits::e1, .sampleShadingEnable = vk::False};

        constexpr vk::PipelineColorBlendAttachmentState color_blend_attachment{
            .blendEnable = vk::False,
            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
        };

        const vk::PipelineColorBlendStateCreateInfo color_blending{
            .logicOpEnable = vk::False,
            .logicOp = vk::LogicOp::eCopy,
            .attachmentCount = 1,
            .pAttachments = &color_blend_attachment,
        };

        constexpr std::array dynamic_states{vk::DynamicState::eViewport, vk::DynamicState::eScissor};
        const vk::PipelineDynamicStateCreateInfo dynamic_state_info{
            .dynamicStateCount = static_cast<uint32_t>(dynamic_states.size()),
            .pDynamicStates = dynamic_states.data(),
        };

        const vk::PipelineLayoutCreateInfo layout_info{
            .setLayoutCount = 1,
            .pSetLayouts = &*descriptor_set_layout_,
            .pushConstantRangeCount = 0,
        };

        auto layout_result = device_.createPipelineLayout(layout_info);
        if (!layout_result.has_value()) {
            return std::unexpected(Error{
                .code = ErrorCode::PipelineLayoutCreationFailed,
                .message = std::format("Failed to create pipeline layout: {}", vk::to_string(layout_result.error())),
                .vk_result = layout_result.error(),
            });
        }

        pipeline_layout_ = std::move(*layout_result);

        const vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipeline_create_info{
            {
                .stageCount = static_cast<uint32_t>(shader_stages.size()),
                .pStages = shader_stages.data(),
                .pVertexInputState = &vertex_input_info,
                .pInputAssemblyState = &input_assembly_info,
                .pViewportState = &viewport_state,
                .pRasterizationState = &rasterizer,
                .pMultisampleState = &multisampling,
                .pColorBlendState = &color_blending,
                .pDynamicState = &dynamic_state_info,
                .layout = *pipeline_layout_,
                .renderPass = nullptr,
            },
            {
                .colorAttachmentCount = 1,
                .pColorAttachmentFormats = &swapchain_format_,
            },
        };

        auto pipeline_result =
            device_.createGraphicsPipeline(nullptr, pipeline_create_info.get<vk::GraphicsPipelineCreateInfo>());

        if (!pipeline_result.has_value()) {
            return std::unexpected(Error{
                .code = ErrorCode::PipelineCreationFailed,
                .message =
                    std::format("Failed to create graphics pipeline: {}", vk::to_string(pipeline_result.error())),
                .vk_result = pipeline_result.error(),
            });
        }

        graphics_pipeline_ = std::move(*pipeline_result);

        return {};
    }

    auto create_command_pool() noexcept -> Result<> {
        const vk::CommandPoolCreateInfo pool_info{
            .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = *queue_family_indices_.graphics,
        };
        auto result = device_.createCommandPool(pool_info);
        if (!result.has_value()) {
            return std::unexpected(Error{
                .code = ErrorCode::CommandPoolCreationFailed,
                .message = std::format("Failed to create command pool: {}", vk::to_string(result.error())),
                .vk_result = result.error(),
            });
        }
        command_pool_ = std::move(*result);
        return {};
    }

    [[nodiscard]] auto begin_single_time_commands() noexcept -> Result<vk::raii::CommandBuffer> {
        auto result = device_.allocateCommandBuffers(vk::CommandBufferAllocateInfo{
            .commandPool = *command_pool_,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1,
        });
        if (!result.has_value()) {
            return std::unexpected(Error{
                .code = ErrorCode::CommandBufferAllocationFailed,
                .message =
                    std::format("Failed to allocate single-time command buffer: {}", vk::to_string(result.error())),
                .vk_result = result.error(),
            });
        }
        auto cmd = std::move(result->front());
        cmd.begin(vk::CommandBufferBeginInfo{
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
        });

        return cmd;
    }

    auto end_single_time_commands(vk::raii::CommandBuffer cmd) noexcept -> Result<> {
        cmd.end();
        graphics_queue_.submit(
            vk::SubmitInfo{
                .commandBufferCount = 1,
                .pCommandBuffers = &*cmd,
            },
            nullptr);
        graphics_queue_.waitIdle();
        return {};
    }

    [[nodiscard]] auto create_image(uint32_t width,
                                    uint32_t height,
                                    vk::Format format,
                                    vk::ImageTiling tiling,
                                    vk::ImageUsageFlags usage,
                                    vk::MemoryPropertyFlags properties) noexcept -> Result<ImageAllocation> {
        auto image_result = device_.createImage(vk::ImageCreateInfo{
            .imageType = vk::ImageType::e2D,
            .format = format,
            .extent = vk::Extent3D{.width = width, .height = height, .depth = 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = vk::SampleCountFlagBits::e1,
            .tiling = tiling,
            .usage = usage,
            .sharingMode = vk::SharingMode::eExclusive,
            .initialLayout = vk::ImageLayout::eUndefined,
        });
        if (!image_result.has_value()) {
            return std::unexpected(Error{
                .code = ErrorCode::ImageCreationFailed,
                .message = std::format("Failed to create image: {}", vk::to_string(image_result.error())),
                .vk_result = image_result.error(),
            });
        }

        const auto mem_req = image_result->getMemoryRequirements();
        const auto mem_type = find_memory_type(mem_req.memoryTypeBits, properties);
        if (!mem_type.has_value()) {
            return std::unexpected(Error{
                .code = ErrorCode::MemoryTypeNotFound,
                .message = "Failed to find suitable memory type for image",
            });
        }
        auto memory_result = device_.allocateMemory(vk::MemoryAllocateInfo{
            .allocationSize = mem_req.size,
            .memoryTypeIndex = *mem_type,
        });
        if (!memory_result.has_value()) {
            return std::unexpected(Error{
                .code = ErrorCode::MemoryAllocationFailed,
                .message = std::format("Failed to allocate memory for image: {}", vk::to_string(memory_result.error())),
                .vk_result = memory_result.error(),
            });
        }

        image_result->bindMemory(*memory_result, 0);

        return ImageAllocation{std::move(*image_result), std::move(*memory_result)};
    }

    [[nodiscard]] auto transition_image_layout(const vk::raii::Image& image,
                                               vk::ImageLayout old_layout,
                                               vk::ImageLayout new_layout) noexcept -> Result<> {
        auto cmd_result = begin_single_time_commands();
        if (!cmd_result.has_value()) {
            return std::unexpected(cmd_result.error());
        }

        vk::ImageMemoryBarrier barrier{
            .oldLayout = old_layout,
            .newLayout = new_layout,
            .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
            .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
            .image = *image,
            .subresourceRange =
                {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
        };
        vk::PipelineStageFlags source_stage{};
        vk::PipelineStageFlags destination_stage{};

        if (old_layout == vk::ImageLayout::eUndefined && new_layout == vk::ImageLayout::eTransferDstOptimal) {
            barrier.srcAccessMask = {};
            barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
            source_stage = vk::PipelineStageFlagBits::eTopOfPipe;
            destination_stage = vk::PipelineStageFlagBits::eTransfer;
        } else if (old_layout == vk::ImageLayout::eTransferDstOptimal &&
                   new_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
            barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
            barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
            source_stage = vk::PipelineStageFlagBits::eTransfer;
            destination_stage = vk::PipelineStageFlagBits::eFragmentShader;
        } else {
            return std::unexpected(Error{
                .code = ErrorCode::UnsupportedLayoutTransition,
                .message = std::format(
                    "Unsupported layout transition: {} to {}", vk::to_string(old_layout), vk::to_string(new_layout)),
            });
        }

        cmd_result->pipelineBarrier(source_stage, destination_stage, {}, nullptr, nullptr, barrier);
        end_single_time_commands(std::move(*cmd_result));

        return {};
    }

    auto copy_buffer_to_image(const vk::raii::Buffer& buffer,
                              const vk::raii::Image& image,
                              uint32_t width,
                              uint32_t height) noexcept -> Result<> {
        auto cmd_result = begin_single_time_commands();
        if (!cmd_result.has_value()) {
            return std::unexpected(cmd_result.error());
        }
        vk::BufferImageCopy region{
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource =
                {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .mipLevel = 0,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            .imageOffset = vk::Offset3D{.x = 0, .y = 0, .z = 0},
            .imageExtent = vk::Extent3D{.width = width, .height = height, .depth = 1},
        };
        cmd_result->copyBufferToImage(*buffer, *image, vk::ImageLayout::eTransferDstOptimal, region);
        end_single_time_commands(std::move(*cmd_result));
        return {};
    }

    auto create_texture_image() noexcept -> Result<> {
        int tex_width{}, tex_height{}, tex_channels{};
        stbi_uc* pixels = stbi_load("/home/dev/Laboratory/cpp_workspace/vk_imgui_sample/assets/textures/texture.png",
                                    &tex_width,
                                    &tex_height,
                                    &tex_channels,
                                    STBI_rgb_alpha);
        vk::DeviceSize image_size = static_cast<vk::DeviceSize>(tex_width) * tex_height * 4;

        if (!pixels) {
            return std::unexpected(Error{
                .code = ErrorCode::FileReadFailed,
                .message = std::format("Failed to load texture image: {}", stbi_failure_reason()),
            });
        }

        auto staging =
            create_buffer(image_size,
                          vk::BufferUsageFlagBits::eTransferSrc,
                          vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        if (!staging.has_value()) {
            return std::unexpected(staging.error());
        }

        void* data = staging->memory.mapMemory(0, image_size);
        std::memcpy(data, pixels, image_size);
        staging->memory.unmapMemory();

        stbi_image_free(pixels);

        auto texture = create_image(static_cast<uint32_t>(tex_width),
                                    static_cast<uint32_t>(tex_height),
                                    vk::Format::eR8G8B8A8Srgb,
                                    vk::ImageTiling::eOptimal,
                                    vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
                                    vk::MemoryPropertyFlagBits::eDeviceLocal);
        if (!texture.has_value()) {
            return std::unexpected(texture.error());
        }

        if (auto r = transition_image_layout(
                texture->image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
            !r) {
            return std::unexpected(r.error());
        }

        if (auto r = copy_buffer_to_image(
                staging->buffer, texture->image, static_cast<uint32_t>(tex_width), static_cast<uint32_t>(tex_height));
            !r) {
            return std::unexpected(r.error());
        }

        if (auto r = transition_image_layout(
                texture->image, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
            !r) {
            return std::unexpected(r.error());
        }

        texture_image_ = std::move(texture->image);
        texture_image_memory_ = std::move(texture->memory);

        return {};
    }

    auto create_texture_image_view() noexcept -> Result<> {
        auto view_result = create_image_view(texture_image_, vk::Format::eR8G8B8A8Srgb);
        if (!view_result.has_value()) {
            return std::unexpected(view_result.error());
        }
        texture_image_view_ = std::move(*view_result);
        return {};
    }

    auto create_texture_sampler() noexcept -> Result<> {
        auto properties = physical_device_.getProperties();
        vk::SamplerCreateInfo sampler_info{
            .magFilter = vk::Filter::eLinear,
            .minFilter = vk::Filter::eLinear,
            .mipmapMode = vk::SamplerMipmapMode::eLinear,
            .addressModeU = vk::SamplerAddressMode::eRepeat,
            .addressModeV = vk::SamplerAddressMode::eRepeat,
            .addressModeW = vk::SamplerAddressMode::eRepeat,
            .mipLodBias = 0.0F,
            .anisotropyEnable = vk::True,
            .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
            .compareEnable = vk::False,
            .compareOp = vk::CompareOp::eAlways,
            .minLod = 0.0F,
            .maxLod = 0.0F,
            .borderColor = vk::BorderColor::eIntOpaqueBlack,
            .unnormalizedCoordinates = vk::False,
        };

        auto result = device_.createSampler(sampler_info);
        if (!result.has_value()) {
            return std::unexpected(Error{
                .code = ErrorCode::SamplerCreationFailed,
                .message = std::format("Failed to create texture sampler: {}", vk::to_string(result.error())),
                .vk_result = result.error(),
            });
        }
        texture_sampler_ = std::move(*result);
        return {};
    }

    [[nodiscard]] auto find_memory_type(uint32_t type_filter,
                                        vk::MemoryPropertyFlags properties) const noexcept -> std::optional<uint32_t> {
        auto memory_properties = physical_device_.getMemoryProperties();
        for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i) {
            if ((type_filter & (1U << i)) &&
                (memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] auto create_buffer(vk::DeviceSize size,
                                     vk::BufferUsageFlags usage,
                                     vk::MemoryPropertyFlags properties) noexcept -> Result<BufferAllocation> {
        auto buffer_result = device_.createBuffer(vk::BufferCreateInfo{
            .size = size,
            .usage = usage,
            .sharingMode = vk::SharingMode::eExclusive,
        });
        if (!buffer_result.has_value()) {
            return std::unexpected(Error{
                .code = ErrorCode::BufferCreationFailed,
                .message = std::format("Failed to create buffer: {}", vk::to_string(buffer_result.error())),
                .vk_result = buffer_result.error(),
            });
        }
        const auto mem_req = buffer_result->getMemoryRequirements();
        const auto mem_type = find_memory_type(mem_req.memoryTypeBits, properties);
        if (!mem_type.has_value()) {
            return std::unexpected(Error{
                .code = ErrorCode::MemoryTypeNotFound,
                .message = "Failed to find suitable memory type for buffer",
            });
        }
        auto memory_result = device_.allocateMemory(vk::MemoryAllocateInfo{
            .allocationSize = mem_req.size,
            .memoryTypeIndex = *mem_type,
        });
        if (!memory_result.has_value()) {
            return std::unexpected(Error{
                .code = ErrorCode::MemoryAllocationFailed,
                .message =
                    std::format("Failed to allocate memory for buffer: {}", vk::to_string(memory_result.error())),
                .vk_result = memory_result.error(),
            });
        }

        buffer_result->bindMemory(*memory_result, 0);
        return BufferAllocation{.buffer = std::move(*buffer_result), .memory = std::move(*memory_result)};
    }

    auto
    copy_buffer(const vk::raii::Buffer& src, const vk::raii::Buffer& dst, vk::DeviceSize size) noexcept -> Result<> {
        auto cmd_result = begin_single_time_commands();
        if (!cmd_result.has_value()) {
            return std::unexpected(cmd_result.error());
        }

        cmd_result->copyBuffer(src, dst, vk::BufferCopy{.size = size});

        end_single_time_commands(std::move(*cmd_result));

        return {};
    }

    auto create_vertex_buffer() noexcept -> Result<> {
        vk::DeviceSize buffer_size = sizeof(VERTICES[0]) * VERTICES.size();

        auto staging =
            create_buffer(buffer_size,
                          vk::BufferUsageFlagBits::eTransferSrc,
                          vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        if (!staging.has_value()) {
            return std::unexpected(staging.error());
        }

        void* data = staging->memory.mapMemory(0, buffer_size);
        std::memcpy(data, VERTICES.data(), static_cast<size_t>(buffer_size));
        staging->memory.unmapMemory();

        auto vertex = create_buffer(buffer_size,
                                    vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
                                    vk::MemoryPropertyFlagBits::eDeviceLocal);
        if (!vertex.has_value()) {
            return std::unexpected(vertex.error());
        }

        if (auto r = copy_buffer(staging->buffer, vertex->buffer, buffer_size); !r) {
            return std::unexpected(r.error());
        }

        vertex_buffer_ = std::move(vertex->buffer);
        vertex_buffer_memory_ = std::move(vertex->memory);

        return {};
    }

    auto create_index_buffer() noexcept -> Result<> {
        vk::DeviceSize buffer_size = sizeof(INDICES[0]) * INDICES.size();

        auto staging =
            create_buffer(buffer_size,
                          vk::BufferUsageFlagBits::eTransferSrc,
                          vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        if (!staging.has_value()) {
            return std::unexpected(staging.error());
        }

        void* data = staging->memory.mapMemory(0, buffer_size);
        std::memcpy(data, INDICES.data(), static_cast<size_t>(buffer_size));
        staging->memory.unmapMemory();

        auto index = create_buffer(buffer_size,
                                   vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
                                   vk::MemoryPropertyFlagBits::eDeviceLocal);
        if (!index.has_value()) {
            return std::unexpected(index.error());
        }

        if (auto r = copy_buffer(staging->buffer, index->buffer, buffer_size); !r) {
            return std::unexpected(r.error());
        }

        index_buffer_ = std::move(index->buffer);
        index_buffer_memory_ = std::move(index->memory);

        return {};
    }

    auto create_uniform_buffers() noexcept -> Result<> {
        vk::DeviceSize buffer_size = sizeof(UniformBufferObject);

        uniform_buffers_.reserve(MAX_FRAMES_IN_FLIGHT);
        uniform_buffers_memory_.reserve(MAX_FRAMES_IN_FLIGHT);
        uniform_buffers_mapped_.reserve(MAX_FRAMES_IN_FLIGHT);

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            auto allocation =
                create_buffer(buffer_size,
                              vk::BufferUsageFlagBits::eUniformBuffer,
                              vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

            if (!allocation.has_value()) {
                return std::unexpected(allocation.error());
            }

            void* mapped = allocation->memory.mapMemory(0, buffer_size);

            uniform_buffers_.emplace_back(std::move(allocation->buffer));
            uniform_buffers_memory_.emplace_back(std::move(allocation->memory));
            uniform_buffers_mapped_.emplace_back(mapped);
        }

        return {};
    }

    auto update_uniform_buffer(uint32_t frame_index) noexcept -> void {
        static const auto start_time = std::chrono::high_resolution_clock::now();
        const auto current_time = std::chrono::high_resolution_clock::now();
        const float elapsed = std::chrono::duration<float>(current_time - start_time).count();

        UniformBufferObject ubo{};

        ubo.model = glm::rotate(glm::mat4{1.0F}, elapsed * glm::radians(45.0F), glm::vec3{0.0F, 0.0F, 1.0F});

        ubo.view = glm::lookAt(glm::vec3{2.0F, 2.0F, 2.0F}, glm::vec3{0.0F, 0.0F, 0.0F}, glm::vec3{0.0F, 0.0F, 1.0F});

        ubo.proj =
            glm::perspective(glm::radians(45.0F),
                             static_cast<float>(swapchain_extent_.width) / static_cast<float>(swapchain_extent_.height),
                             0.1F,
                             10.0F);

        ubo.proj[1][1] *= -1;

        std::memcpy(uniform_buffers_mapped_[frame_index], &ubo, sizeof(ubo));
    }

    auto create_descriptor_pool() noexcept -> Result<> {
        std::array pool_sizes{
            vk::DescriptorPoolSize{
                .type = vk::DescriptorType::eUniformBuffer,
                .descriptorCount = MAX_FRAMES_IN_FLIGHT,
            },
            vk::DescriptorPoolSize{
                .type = vk::DescriptorType::eCombinedImageSampler,
                .descriptorCount = MAX_FRAMES_IN_FLIGHT,
            },
        };

        auto result = device_.createDescriptorPool(vk::DescriptorPoolCreateInfo{
            .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            .maxSets = MAX_FRAMES_IN_FLIGHT,
            .poolSizeCount = static_cast<uint32_t>(pool_sizes.size()),
            .pPoolSizes = pool_sizes.data(),
        });
        if (!result.has_value()) {
            return std::unexpected(Error{
                .code = ErrorCode::DescriptorPoolCreationFailed,
                .message = std::format("Failed to create descriptor pool: {}", vk::to_string(result.error())),
                .vk_result = result.error(),
            });
        }

        descriptor_pool_ = std::move(*result);
        return {};
    }

    auto create_descriptor_sets() noexcept -> Result<> {
        const std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *descriptor_set_layout_);

        auto result = device_.allocateDescriptorSets(vk::DescriptorSetAllocateInfo{
            .descriptorPool = *descriptor_pool_,
            .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
            .pSetLayouts = layouts.data(),
        });
        if (!result.has_value()) {
            return std::unexpected(Error{
                .code = ErrorCode::DescriptorSetAllocationFailed,
                .message = std::format("Failed to allocate descriptor sets: {}", vk::to_string(result.error())),
                .vk_result = result.error(),
            });
        }
        descriptor_sets_ = std::move(*result);

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            const vk::DescriptorBufferInfo buffer_info{
                .buffer = *uniform_buffers_[i],
                .offset = 0,
                .range = sizeof(UniformBufferObject),
            };
            const vk::DescriptorImageInfo image_info{
                .sampler = *texture_sampler_,
                .imageView = *texture_image_view_,
                .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
            };

            std::array write_descriptors{
                vk::WriteDescriptorSet{
                    .dstSet = *descriptor_sets_[i],
                    .dstBinding = 0,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = vk::DescriptorType::eUniformBuffer,
                    .pBufferInfo = &buffer_info,
                },
                vk::WriteDescriptorSet{
                    .dstSet = *descriptor_sets_[i],
                    .dstBinding = 1,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                    .pImageInfo = &image_info,
                },
            };

            device_.updateDescriptorSets(write_descriptors, {});
        }

        return {};
    }

    auto create_command_buffers() noexcept -> Result<> {
        const vk::CommandBufferAllocateInfo alloc_info{
            .commandPool = *command_pool_,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = MAX_FRAMES_IN_FLIGHT,
        };

        auto result = device_.allocateCommandBuffers(alloc_info);
        if (!result.has_value()) {
            return std::unexpected(Error{
                .code = ErrorCode::CommandBufferAllocationFailed,
                .message = std::format("Failed to allocate command buffers: {}", vk::to_string(result.error())),
                .vk_result = result.error(),
            });
        }

        command_buffers_ = std::move(*result);
        return {};
    }

    auto transition_image_layout(const vk::raii::CommandBuffer& cmd,
                                 uint32_t image_index,
                                 vk::ImageLayout old_layout,
                                 vk::ImageLayout new_layout,
                                 vk::AccessFlags2 src_access,
                                 vk::AccessFlags2 dst_access,
                                 vk::PipelineStageFlags2 src_stage,
                                 vk::PipelineStageFlags2 dst_stage) const noexcept -> void {
        const vk::ImageMemoryBarrier2 barrier{
            .srcStageMask = src_stage,
            .srcAccessMask = src_access,
            .dstStageMask = dst_stage,
            .dstAccessMask = dst_access,
            .oldLayout = old_layout,
            .newLayout = new_layout,
            .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
            .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
            .image = swapchain_images_[image_index],
            .subresourceRange =
                {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
        };

        cmd.pipelineBarrier2(vk::DependencyInfo{
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &barrier,
        });
    }

    auto record_command_buffer(const vk::raii::CommandBuffer& cmd, uint32_t image_index) noexcept -> Result<> {
        cmd.begin(vk::CommandBufferBeginInfo{});

        transition_image_layout(cmd,
                                image_index,
                                vk::ImageLayout::eUndefined,
                                vk::ImageLayout::eColorAttachmentOptimal,
                                {},
                                vk::AccessFlagBits2::eColorAttachmentWrite,
                                vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                                vk::PipelineStageFlagBits2::eColorAttachmentOutput);

        constexpr vk::ClearValue clear_color{vk::ClearColorValue{0.0F, 0.0F, 0.0F, 1.0F}};

        const vk::RenderingAttachmentInfo color_attachment{
            .imageView = *swapchain_image_views_[image_index],
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue = clear_color,
        };

        cmd.beginRendering(vk::RenderingInfo{
            .renderArea = {.offset = {.x = 0, .y = 0}, .extent = swapchain_extent_},
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &color_attachment,
        });

        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphics_pipeline_);
        cmd.setViewport(0,
                        vk::Viewport{
                            .x = 0.0F,
                            .y = 0.0F,
                            .width = static_cast<float>(swapchain_extent_.width),
                            .height = static_cast<float>(swapchain_extent_.height),
                            .minDepth = 0.0F,
                            .maxDepth = 1.0F,
                        });

        cmd.setScissor(0, vk::Rect2D{.offset = {0, 0}, .extent = swapchain_extent_});

        cmd.bindVertexBuffers(0, {*vertex_buffer_}, {0ULL});
        cmd.bindIndexBuffer(*index_buffer_, 0, vk::IndexType::eUint16);

        cmd.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics, *pipeline_layout_, 0, {*descriptor_sets_[current_frame_]}, nullptr);

        cmd.drawIndexed(static_cast<uint32_t>(INDICES.size()), 1, 0, 0, 0);

        cmd.endRendering();

        transition_image_layout(cmd,
                                image_index,
                                vk::ImageLayout::eColorAttachmentOptimal,
                                vk::ImageLayout::ePresentSrcKHR,
                                vk::AccessFlagBits2::eColorAttachmentWrite,
                                {},
                                vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                                vk::PipelineStageFlagBits2::eBottomOfPipe);

        cmd.end();
        return {};
    }

    auto create_sync_objects() noexcept -> Result<> {
        const auto make_semaphores =
            [&](std::vector<vk::raii::Semaphore>& out, uint32_t count, std::string_view name) -> Result<> {
            out.reserve(count);
            for (uint32_t i = 0; i < count; ++i) {
                auto result = device_.createSemaphore(vk::SemaphoreCreateInfo{});
                if (!result.has_value()) {
                    return std::unexpected(Error{
                        .code = ErrorCode::SynchronizationObjectCreationFailed,
                        .message =
                            std::format("Failed to create {} semaphore {}: {}", name, i, vk::to_string(result.error())),
                        .vk_result = result.error(),
                    });
                }
                out.emplace_back(std::move(*result));
            }
            return {};
        };

        if (auto result = make_semaphores(image_available_semaphores_, MAX_FRAMES_IN_FLIGHT, "image_available");
            !result) {
            return std::unexpected(result.error());
        }

        if (auto result = make_semaphores(
                render_finished_semaphores_, static_cast<uint32_t>(swapchain_images_.size()), "render_finished");
            !result) {
            return std::unexpected(result.error());
        }

        in_flight_fences_.reserve(MAX_FRAMES_IN_FLIGHT);
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            auto result = device_.createFence(vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled});
            if (!result.has_value()) {
                return std::unexpected(Error{
                    .code = ErrorCode::SynchronizationObjectCreationFailed,
                    .message = std::format("Failed to create in-flight fence {}: {}", i, vk::to_string(result.error())),
                    .vk_result = result.error(),
                });
            }
            in_flight_fences_.emplace_back(std::move(*result));
        }

        return {};
    }

    auto draw_frame() noexcept -> Result<> {
        auto& cmd = command_buffers_[current_frame_];
        auto& fence = in_flight_fences_[current_frame_];

        const auto wait_r = device_.waitForFences(*fence, vk::True, std::numeric_limits<uint64_t>::max());
        if (wait_r != vk::Result::eSuccess && wait_r != vk::Result::eTimeout) {
            return std::unexpected(Error{
                .code = ErrorCode::QueueSubmitFailed,
                .message = std::format("waitForFences failed: {}", vk::to_string(wait_r)),
                .vk_result = wait_r,
            });
        }

        const auto [acquire_r, image_index] = swapchain_.acquireNextImage(
            std::numeric_limits<uint64_t>::max(), *image_available_semaphores_[current_frame_], nullptr);

        if (acquire_r == vk::Result::eErrorOutOfDateKHR) {
            if (auto r = recreate_swapchain(); !r) {
                return std::unexpected(r.error());
            }
            return {};
        }

        if (acquire_r != vk::Result::eSuccess && acquire_r != vk::Result::eSuboptimalKHR) {
            return std::unexpected(Error{
                .code = ErrorCode::AcquireImageFailed,
                .message = std::format("acquireNextImage failed: {}", vk::to_string(acquire_r)),
                .vk_result = acquire_r,
            });
        }

        cmd.reset();
        if (auto r = record_command_buffer(cmd, image_index); !r) {
            return std::unexpected(r.error());
        }

        update_uniform_buffer(current_frame_);

        device_.resetFences(*fence);

        constexpr vk::PipelineStageFlags wait_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;

        graphics_queue_.submit(
            vk::SubmitInfo{
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &*image_available_semaphores_[current_frame_],
                .pWaitDstStageMask = &wait_stage,
                .commandBufferCount = 1,
                .pCommandBuffers = &*cmd,
                .signalSemaphoreCount = 1,
                .pSignalSemaphores = &*render_finished_semaphores_[image_index],
            },
            *fence);

        const auto present_r = present_queue_.presentKHR(vk::PresentInfoKHR{
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &*render_finished_semaphores_[image_index],
            .swapchainCount = 1,
            .pSwapchains = &*swapchain_,
            .pImageIndices = &image_index,
        });

        const bool needs_recreation = present_r == vk::Result::eErrorOutOfDateKHR ||
                                      present_r == vk::Result::eSuboptimalKHR || framebuffer_resized_;
        if (needs_recreation) {
            framebuffer_resized_ = false;
            if (auto r = recreate_swapchain(); !r) {
                return std::unexpected(r.error());
            }
        } else if (present_r != vk::Result::eSuccess) {
            return std::unexpected(Error{
                .code = ErrorCode::PresentFailed,
                .message = std::format("presentKHR failed: {}", vk::to_string(present_r)),
                .vk_result = present_r,
            });
        }

        current_frame_ = (current_frame_ + 1) % MAX_FRAMES_IN_FLIGHT;
        return {};
    }

    [[nodiscard]] auto
    create_shader_module(const std::vector<std::byte>& code) noexcept -> Result<vk::raii::ShaderModule> {
        vk::ShaderModuleCreateInfo create_info{
            .codeSize = code.size(),
            .pCode = reinterpret_cast<const uint32_t*>(code.data()),
        };
        auto result = device_.createShaderModule(create_info);
        if (!result.has_value()) {
            return std::unexpected(
                Error{.code = ErrorCode::ShaderModuleCreationFailed,
                      .message = std::format("Failed to create shader module: {}", vk::to_string(result.error())),
                      .vk_result = result.error()});
        }

        return std::move(*result);
    }

    [[nodiscard]] static auto read_file(const std::filesystem::path& path) noexcept -> Result<std::vector<std::byte>> {
        std::error_code ec;

        const auto file_size = std::filesystem::file_size(path, ec);
        if (ec) {
            return std::unexpected(
                Error{.code = ErrorCode::FileReadFailed,
                      .message = std::format("Failed to get file size for {}: {}", path.string(), ec.message())});
        }
        if (file_size == 0) {
            return std::unexpected(
                Error{.code = ErrorCode::FileReadFailed, .message = std::format("File is empty: {}", path.string())});
        }

        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            return std::unexpected(Error{.code = ErrorCode::FileReadFailed,
                                         .message = std::format("Failed to open file: {}", path.string())});
        }

        file.rdbuf()->pubsetbuf(nullptr, 0);

        std::vector<std::byte> buffer(file_size);
        file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(file_size));

        if (file.gcount() != static_cast<std::streamsize>(file_size)) {
            return std::unexpected(Error{
                .code = ErrorCode::FileReadFailed,
                .message = std::format(
                    "Short read on '{}': expected {} bytes, got {}", path.string(), file_size, file.gcount()),
            });
        }

        return buffer;
    }

    [[nodiscard]] auto is_device_suitable(const vk::raii::PhysicalDevice& physical_device) const noexcept -> bool {
        const auto device_properties = physical_device.getProperties();

        if (device_properties.apiVersion < VK_API_VERSION_1_3 ||
            device_properties.deviceType != vk::PhysicalDeviceType::eDiscreteGpu) {
            return false;
        }

        const auto available_extensions = physical_device.enumerateDeviceExtensionProperties();
        const auto supports_extension = [&](std::string_view required_name) {
            return std::ranges::any_of(available_extensions,
                                       [&](const auto& ext) { return required_name == ext.extensionName; });
        };

        if (!std::ranges::all_of(DEVICE_EXTENSIONS, supports_extension)) {
            return false;
        }

        if (!query_swapchain_support(physical_device).is_adequate()) {
            return false;
        }

        const auto feature_chain = physical_device.getFeatures2<vk::PhysicalDeviceFeatures2,
                                                                vk::PhysicalDeviceVulkan11Features,
                                                                vk::PhysicalDeviceVulkan13Features,
                                                                vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();

        const auto& core_features = feature_chain.get<vk::PhysicalDeviceFeatures2>().features;
        const auto& vk11_features = feature_chain.get<vk::PhysicalDeviceVulkan11Features>();
        const auto& vk13_features = feature_chain.get<vk::PhysicalDeviceVulkan13Features>();
        const auto& dynamic_features = feature_chain.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();

        return core_features.samplerAnisotropy && vk11_features.shaderDrawParameters &&
               vk13_features.synchronization2 && vk13_features.dynamicRendering &&
               dynamic_features.extendedDynamicState;
    }

    [[nodiscard]] auto
    query_swapchain_support(const vk::raii::PhysicalDevice& device) const noexcept -> SwapChainSupportDetails {
        return SwapChainSupportDetails{
            .capabilities = device.getSurfaceCapabilitiesKHR(*surface_),
            .formats = device.getSurfaceFormatsKHR(*surface_),
            .present_modes = device.getSurfacePresentModesKHR(*surface_),
        };
    }

    [[nodiscard]] static auto
    choose_swap_surface_format(std::span<const vk::SurfaceFormatKHR> formats) noexcept -> vk::SurfaceFormatKHR {
        const auto it = std::ranges::find_if(formats, [](const auto& f) {
            return f.format == vk::Format::eB8G8R8A8Srgb && f.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
        });
        return it != formats.end() ? *it : formats.front();
    }

    [[nodiscard]] static auto
    choose_swap_present_mode(std::span<const vk::PresentModeKHR> present_modes) noexcept -> vk::PresentModeKHR {
        const auto it = std::ranges::find(present_modes, vk::PresentModeKHR::eMailbox);
        return it != present_modes.end() ? *it : vk::PresentModeKHR::eFifo;
    }

    [[nodiscard]] auto
    choose_swap_extent(const vk::SurfaceCapabilitiesKHR& capabilities) const noexcept -> vk::Extent2D {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        }
        int width{}, height{};
        glfwGetFramebufferSize(window_, &width, &height);

        return {
            std::clamp(
                static_cast<uint32_t>(width), capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
            std::clamp(
                static_cast<uint32_t>(height), capabilities.minImageExtent.height, capabilities.maxImageExtent.height),
        };
    }

    [[nodiscard]] auto
    find_queue_families(const vk::raii::PhysicalDevice& device) const noexcept -> Result<QueueFamilyIndices> {
        QueueFamilyIndices indices{};
        const auto queue_families = device.getQueueFamilyProperties();

        for (uint32_t i = 0; const auto& family : queue_families) {
            if ((family.queueFlags & vk::QueueFlagBits::eGraphics) != vk::QueueFlags{}) {
                indices.graphics = i;
            }

            if (device.getSurfaceSupportKHR(i, *surface_) == vk::True) {
                indices.present = i;
            }

            if (indices.is_complete()) {
                return indices;
            }
            ++i;
        }

        if (!indices.graphics.has_value()) {
            return std::unexpected(
                Error{.code = ErrorCode::NoGraphicsQueueFamily, .message = "Failed to find graphics queue family"});
        }

        if (!indices.present.has_value()) {
            return std::unexpected(
                Error{.code = ErrorCode::NoPresentQueueFamily, .message = "Failed to find present queue family"});
        }

        return indices;
    }

    [[nodiscard]] auto get_required_layers() const noexcept -> std::vector<const char*> {
        if constexpr (ENABLE_VALIDATION) {
            return {VALIDATION_LAYERS.begin(), VALIDATION_LAYERS.end()};
        }
        return {};
    }

    [[nodiscard]] auto get_required_extensions() const noexcept -> std::vector<const char*> {
        uint32_t count = 0;
        const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&count);
        std::vector<const char*> extensions(glfw_extensions, glfw_extensions + count);

        if constexpr (ENABLE_VALIDATION) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    }

    auto validate_layers(std::span<const char* const> required_layers) const noexcept -> Result<> {
        if (required_layers.empty()) {
            return {};
        }

        const auto available_layers = context_.enumerateInstanceLayerProperties();
        auto available = available_layers |
                         std::views::transform([](const auto& prop) { return std::string_view(prop.layerName); }) |
                         std::ranges::to<std::unordered_set>();

        auto missing = std::ranges::find_if(
            required_layers, [&available](std::string_view layer) { return !available.contains(layer); });

        if (missing != required_layers.end()) {
            return std::unexpected(
                Error{.code = ErrorCode::ValidationLayerNotSupported,
                      .message = std::format("Required validation layer not supported: {}", *missing)});
        }

        return {};
    }

    auto validate_extensions(std::span<const char* const> required_extensions) const noexcept -> Result<> {
        const auto available_extensions = context_.enumerateInstanceExtensionProperties();
        auto available = available_extensions |
                         std::views::transform([](const auto& ext) { return std::string_view(ext.extensionName); }) |
                         std::ranges::to<std::unordered_set>();

        auto missing = std::ranges::find_if(required_extensions,
                                            [&available](std::string_view ext) { return !available.contains(ext); });

        if (missing != required_extensions.end()) {
            return std::unexpected(Error{.code = ErrorCode::ExtensionNotSupported,
                                         .message = std::format("Required extension not supported: {}", *missing)});
        }

        return {};
    }

    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debug_callback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
                                                           vk::DebugUtilsMessageTypeFlagsEXT /* type */,
                                                           const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                           void* /* user_data */) {
        std::println(stderr, "Validation layer [{}]: {}", vk::to_string(severity), pCallbackData->pMessage);
        return vk::False;
    }

    static void glfw_framebuffer_resize_callback(GLFWwindow* window, int /* width */, int /* height */) {
        auto app = reinterpret_cast<VkApplication*>(glfwGetWindowUserPointer(window));
        app->framebuffer_resized_ = true;
    }

    static void glfw_key_callback(GLFWwindow* window, int key, int /* scancode */, int action, int /* mods */) {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    }

    void main_loop() noexcept {
        while (!glfwWindowShouldClose(window_)) {
            glfwPollEvents();

            if (auto result = draw_frame(); !result) {
                std::println(stderr, "Error during draw frame: {}", result.error().what());
                if (result.error().vk_result) {
                    std::println(stderr, "Vulkan Result: {}", vk::to_string(*result.error().vk_result));
                }
                break;
            }
        }

        device_.waitIdle();
    }

    GLFWwindow* window_{nullptr};

    vk::raii::Context context_;
    vk::raii::Instance instance_{nullptr};
    vk::raii::DebugUtilsMessengerEXT debug_messenger_{nullptr};

    vk::raii::SurfaceKHR surface_{nullptr};
    vk::raii::PhysicalDevice physical_device_{nullptr};
    vk::raii::Device device_{nullptr};

    QueueFamilyIndices queue_family_indices_;
    vk::raii::Queue graphics_queue_{nullptr};
    vk::raii::Queue present_queue_{nullptr};

    vk::raii::SwapchainKHR swapchain_{nullptr};
    std::vector<vk::Image> swapchain_images_;
    std::vector<vk::raii::ImageView> swapchain_image_views_;
    vk::Format swapchain_format_ = vk::Format::eUndefined;
    vk::Extent2D swapchain_extent_{};

    vk::raii::DescriptorSetLayout descriptor_set_layout_{nullptr};
    vk::raii::PipelineLayout pipeline_layout_{nullptr};
    vk::raii::Pipeline graphics_pipeline_{nullptr};

    vk::raii::Image texture_image_{nullptr};
    vk::raii::DeviceMemory texture_image_memory_{nullptr};
    vk::raii::ImageView texture_image_view_{nullptr};
    vk::raii::Sampler texture_sampler_{nullptr};

    vk::raii::Buffer vertex_buffer_{nullptr};
    vk::raii::DeviceMemory vertex_buffer_memory_{nullptr};
    vk::raii::Buffer index_buffer_{nullptr};
    vk::raii::DeviceMemory index_buffer_memory_{nullptr};

    std::vector<vk::raii::Buffer> uniform_buffers_;
    std::vector<vk::raii::DeviceMemory> uniform_buffers_memory_;
    std::vector<void*> uniform_buffers_mapped_;

    vk::raii::DescriptorPool descriptor_pool_{nullptr};
    std::vector<vk::raii::DescriptorSet> descriptor_sets_;

    vk::raii::CommandPool command_pool_{nullptr};
    std::vector<vk::raii::CommandBuffer> command_buffers_;

    std::vector<vk::raii::Semaphore> image_available_semaphores_;
    std::vector<vk::raii::Semaphore> render_finished_semaphores_;
    std::vector<vk::raii::Fence> in_flight_fences_;

    uint32_t current_frame_{0};
    bool framebuffer_resized_{false};
};

auto main() -> int {
    VkApplication app{};

    const char* msg = "Vulkan Application - Press ESC to exit";
    std::println("{}", std::string_view(msg));

    if (auto result = app.run(); !result) {
        std::println(stderr, "Error: {}", result.error().what());
        if (result.error().vk_result) {
            std::println(stderr, "Vulkan Result: {}", vk::to_string(*result.error().vk_result));
        }
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}