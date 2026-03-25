#include <vulkan/vulkan_raii.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <expected>
#include <limits>
#include <optional>
#include <ranges>
#include <span>
#include <string_view>
#include <vector>

#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <print>
#include <unordered_set>

namespace {
constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;
constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

constexpr std::array VALIDATION_LAYERS = {"VK_LAYER_KHRONOS_validation"};
constexpr std::array DEVICE_EXTENSIONS = {vk::KHRSwapchainExtensionName};

#ifdef NDEBUG
constexpr bool ENABLE_VALIDATION = false;
#else
constexpr bool ENABLE_VALIDATION = true;
#endif
} // namespace

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
    PresentFailed
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

        if (auto result = create_image_views(); !result) {
            return std::unexpected(result.error());
        }

        if (auto result = create_graphics_pipeline(); !result) {
            return std::unexpected(result.error());
        }

        if (auto result = create_command_pool(); !result) {
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
                {},
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

        if (auto result = create_image_views(); !result) {
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

    auto create_image_views() noexcept -> Result<> {
        swapchain_image_views_.reserve(swapchain_images_.size());

        for (const auto& image : swapchain_images_) {
            vk::ImageViewCreateInfo create_info{
                .image = image,
                .viewType = vk::ImageViewType::e2D,
                .format = swapchain_format_,
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
            swapchain_image_views_.emplace_back(std::move(*result));
        }

        return {};
    }

    auto create_graphics_pipeline() noexcept -> Result<> {
        auto spirv = read_file("C:/Laboratory/cpp-workspace/vk_imgui_sample/assets/shaders/triangle.spv");
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

        constexpr vk::PipelineVertexInputStateCreateInfo vertex_input_info{};

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
            .frontFace = vk::FrontFace::eClockwise,
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

        auto layout_result = device_.createPipelineLayout(vk::PipelineLayoutCreateInfo{});
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
            .renderArea = {.offset = {0, 0}, .extent = swapchain_extent_},
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

        cmd.draw(3, 1, 0, 0);

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

        device_.resetFences(*fence);

        const vk::PipelineStageFlags wait_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;

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

    [[nodiscard]] auto create_shader_module(const std::vector<std::byte>& code) noexcept
        -> Result<vk::raii::ShaderModule> {
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

        const auto& vk11_features = feature_chain.get<vk::PhysicalDeviceVulkan11Features>();
        const auto& vk13_features = feature_chain.get<vk::PhysicalDeviceVulkan13Features>();
        const auto& dynamic_features = feature_chain.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();

        return vk11_features.shaderDrawParameters && vk13_features.synchronization2 && vk13_features.dynamicRendering &&
               dynamic_features.extendedDynamicState;
    }

    [[nodiscard]] auto query_swapchain_support(const vk::raii::PhysicalDevice& device) const noexcept
        -> SwapChainSupportDetails {
        return SwapChainSupportDetails{
            .capabilities = device.getSurfaceCapabilitiesKHR(*surface_),
            .formats = device.getSurfaceFormatsKHR(*surface_),
            .present_modes = device.getSurfacePresentModesKHR(*surface_),
        };
    }

    [[nodiscard]] static auto choose_swap_surface_format(std::span<const vk::SurfaceFormatKHR> formats) noexcept
        -> vk::SurfaceFormatKHR {
        const auto it = std::ranges::find_if(formats, [](const auto& f) {
            return f.format == vk::Format::eB8G8R8A8Srgb && f.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
        });
        return it != formats.end() ? *it : formats.front();
    }

    [[nodiscard]] static auto choose_swap_present_mode(std::span<const vk::PresentModeKHR> present_modes) noexcept
        -> vk::PresentModeKHR {
        const auto it = std::ranges::find(present_modes, vk::PresentModeKHR::eMailbox);
        return it != present_modes.end() ? *it : vk::PresentModeKHR::eFifo;
    }

    [[nodiscard]] auto choose_swap_extent(const vk::SurfaceCapabilitiesKHR& capabilities) const noexcept
        -> vk::Extent2D {
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

    [[nodiscard]] auto find_queue_families(const vk::raii::PhysicalDevice& device) const noexcept
        -> Result<QueueFamilyIndices> {
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

    vk::raii::PipelineLayout pipeline_layout_{nullptr};
    vk::raii::Pipeline graphics_pipeline_{nullptr};

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