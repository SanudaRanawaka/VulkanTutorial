#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <map>
#include <fstream>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif
#include <GLFW/glfw3.h>

// window size constants
constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;

// standard validation layers bundle
const std::vector<char const*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

// disable validation layers in release builds
#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

class HelloTriangleApplication {
public:
    
    HelloTriangleApplication() {

    }
 
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:

    GLFWwindow* window = nullptr;
    // only want one instance at a time
    vk::raii::Instance instance = nullptr;
    // debug
    vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;
    // window, The window surface needs to be created right after the instance creation
    // because it can actually influence the physical device selection
    vk::raii::SurfaceKHR surface = nullptr;
	// only want one device context at a time
	vk::raii::Context context;
    // one graphics card for now
    vk::raii::PhysicalDevice physicalDevice = nullptr;
    // logical device
    vk::raii::Device device = nullptr;
    //handle to queue
    vk::raii::Queue graphicsQueue = nullptr;
    //handle to presentqueue
    vk::raii::Queue presentQueue = nullptr;
    // Swap chain
    vk::raii::SwapchainKHR swapChain = nullptr;
    std::vector<vk::Image> swapChainImages;
    // will need these swapchain attributes later
    vk::Format swapChainImageFormat = vk::Format::eUndefined;
    vk::Extent2D swapChainExtent;
    // image views
    std::vector<vk::raii::ImageView> swapChainImageViews;
    // pipeline
    vk::raii::PipelineLayout pipelineLayout = nullptr;
    vk::raii::Pipeline graphicsPipeline = nullptr;
    // command buffer
    vk::raii::CommandPool commandPool = nullptr;
    vk::raii::CommandBuffer commandBuffer = nullptr;
    // sync
    vk::raii::Semaphore presentCompleteSemaphore = nullptr;
    vk::raii::Semaphore renderFinishedSemaphore = nullptr;
    vk::raii::Fence drawFence = nullptr;

    
    


    void initVulkan() {
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createGraphicsPipeline();
        createCommandPool();
        createCommandBuffer();
        createSyncObjects();
    }

    void initWindow() {
        // first call has to be glfwinit
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW");
        }
        // tell it not to open an openGL window
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        // disable window resizing for now as its complicated
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        // 4th is more monitor selection, 5th is for opengl only
        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan Window", nullptr, nullptr);
        if (!window) {
            throw std::runtime_error("Failed to create GLFW window");
		}
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            drawFrame();
        }
        device.waitIdle();
    }

    void cleanup() {
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    void createInstance() {
        constexpr vk::ApplicationInfo appInfo{ 
            .pApplicationName = "Hello Triangle",
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName = "No Engine",
            .engineVersion = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion = vk::ApiVersion14
        };

        // Get the required layers
        std::vector<char const*> requiredLayers;
        if (enableValidationLayers) {
            requiredLayers.assign(validationLayers.begin(), validationLayers.end());
        }

        // Check if the required layers are supported by the Vulkan implementation.
        auto layerProperties = context.enumerateInstanceLayerProperties();
        if (std::ranges::any_of(requiredLayers, [&layerProperties](auto const& requiredLayer) {
            return std::ranges::none_of(layerProperties,
                [requiredLayer](auto const& layerProperty)
                { return strcmp(layerProperty.layerName, requiredLayer) == 0; });
            }))
        {
            throw std::runtime_error("One or more required layers are not supported!");
        }

        // get required extensions 
        auto extensions = getRequiredExtensions();
 
        // Check if the required extensions are supported by the Vulkan implementation.
        auto extensionProperties = context.enumerateInstanceExtensionProperties();
        for (auto const& requiredExtension : extensions)
        {
            if (std::ranges::none_of(extensionProperties,
                [requiredExtension](auto const& extensionProperty) { return strcmp(extensionProperty.extensionName, requiredExtension) == 0; }))
            {
                throw std::runtime_error("Required extension not supported: " + std::string(requiredExtension));
            }
        }

        vk::InstanceCreateInfo createInfo{ 
                                           .pApplicationInfo = &appInfo,
                                           .enabledLayerCount = static_cast<uint32_t>(requiredLayers.size()),
                                           .ppEnabledLayerNames = requiredLayers.data(),
                                           .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
                                           .ppEnabledExtensionNames = extensions.data()
        };
        instance = vk::raii::Instance(context, createInfo);
    }

    void createSurface() {
        //GLFW only works with Vulkan C API 
        VkSurfaceKHR _surface;
        // our instance is a pointer but we need the actual variable so dereference
        // this gives a VkResult so if it fails throw an error
        if (glfwCreateWindowSurface(*instance, window, nullptr, &_surface) != VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface!");
        }
        //we wrap the C API class in a C++ class
        surface = vk::raii::SurfaceKHR(instance,_surface);
    }

    void pickPhysicalDevice() {
        // query the number of graphics cards we have
        auto devices = instance.enumeratePhysicalDevices();
        // if none with vulkan support just give up
        if (devices.empty()) {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }

        // Use an ordered map to automatically sort candidates by increasing score
        std::multimap<int, vk::raii::PhysicalDevice> candidates;

        // the ones we have check if they meet requirements
        for (const auto &device : devices) {
            candidates.insert(std::make_pair(getDeviceSuitabilityScore(device), device));
        }

        // Check if the best candidate is suitable at all
        if (candidates.rbegin()->first > 0) {
            physicalDevice = candidates.rbegin()->second;
        }
        else {
            throw std::runtime_error("failed to find a suitable GPU!");
        }

    }

    void createLogicalDevice() {
        std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
        //uint32_t graphicsIndex = findQueueFamilies(physicalDevice);
        
        // get the first index into queueFamilyProperties which supports graphics
        auto graphicsQueueFamilyProperty = std::ranges::find_if(queueFamilyProperties, [](auto const& qfp)
            { return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0); });

        auto graphicsIndex = static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), graphicsQueueFamilyProperty));

        // determine a queueFamilyIndex that supports present
        // first check if the graphicsIndex is good enough
        auto presentIndex = physicalDevice.getSurfaceSupportKHR(graphicsIndex, *surface) ? graphicsIndex : static_cast<uint32_t>(queueFamilyProperties.size());
        if (presentIndex == queueFamilyProperties.size()){
            // the graphicsIndex doesn't support present -> look for another family index that supports both
            // graphics and present
            for (size_t i = 0; i < queueFamilyProperties.size(); i++){
                if ((queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics) &&
                    physicalDevice.getSurfaceSupportKHR(static_cast<uint32_t>(i), *surface)){
                    graphicsIndex = static_cast<uint32_t>(i);
                    presentIndex = graphicsIndex;
                    break;
                }
            }
            if (presentIndex == queueFamilyProperties.size()){
                // there's nothing like a single family index that supports both graphics and present -> look for another
                // family index that supports present
                for (size_t i = 0; i < queueFamilyProperties.size(); i++){
                    if (physicalDevice.getSurfaceSupportKHR(static_cast<uint32_t>(i), *surface)){
                        presentIndex = static_cast<uint32_t>(i);
                        break;
                    }
                }
            }
        }
        if ((graphicsIndex == queueFamilyProperties.size()) || (presentIndex == queueFamilyProperties.size())){
            throw std::runtime_error("Could not find a queue for graphics or present -> terminating");
        }

        // priority between 0.0 and 1.0 need this even if only one queue
        float queuePriority = 0.5f;
        //describes the number of queues we want for a single queue family
        //Right now we’re only interested in a queue with graphics capabilities.
        vk::DeviceQueueCreateInfo deviceQueueCreationInfo{.queueFamilyIndex = graphicsIndex,
            .queueCount = 1, 
            .pQueuePriorities = &queuePriority
        };

        // Create a chain of feature structures, by default vulkan only includes vulkan1.0 features
        // apparently .samplerAnisotropy = true will pass validation layers
        vk::PhysicalDeviceFeatures physdevfeatures = {.samplerAnisotropy = true};
        vk::PhysicalDeviceVulkan13Features vulkanCoreFeatures = {.dynamicRendering = true};
        vulkanCoreFeatures.setSynchronization2(true);
        vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT> featureChain = {
            {.features = physdevfeatures},                               // vk::PhysicalDeviceFeatures2 (empty for now)
            vulkanCoreFeatures ,      // Enable dynamic rendering from Vulkan 1.3
            {.extendedDynamicState = true}   // Enable extended dynamic state from the extension
        };
        // extension we want
        std::vector<const char*> deviceExtensions = {
            //vk::KHRSynchronization2ExtensionName,
            vk::KHRSwapchainExtensionName,
            vk::KHRSpirv14ExtensionName,
            vk::KHRCreateRenderpass2ExtensionName,
            vk::KHRShaderDrawParametersExtensionName,
        };
        // vulkan follows the pnext pointer and can see the connected features from the featureChain
        // use core synch2 instead of khr extension
        vk::PhysicalDeviceSynchronization2Features synch2features{.synchronization2 = VK_TRUE};
        vk::DeviceCreateInfo deviceCreateInfo{.pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &deviceQueueCreationInfo,
            .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
            .ppEnabledExtensionNames = deviceExtensions.data()
        };
        deviceCreateInfo.enabledExtensionCount = deviceExtensions.size();
        deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

        // device we interface with and the usage info
        device = vk::raii::Device(physicalDevice, deviceCreateInfo);
        // queues are created and destroyed along with the device
        graphicsQueue = vk::raii::Queue(device, graphicsIndex, 0);

        presentQueue = vk::raii::Queue(device, presentIndex, 0);
    }

    uint32_t getDeviceSuitabilityScore(const vk::raii::PhysicalDevice &physicalDevice){
        auto deviceProperties = physicalDevice.getProperties();
        auto deviceFeatures = physicalDevice.getFeatures();

        // Application can't function without geometry shaders
        if (!deviceFeatures.geometryShader) {
            return -1;
        }

        uint32_t suitabilityScore = 0;
        // Discrete GPUs have a significant performance advantage
        if (deviceProperties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
            suitabilityScore += 1000;
        }

        // Maximum possible size of textures affects graphics quality
        suitabilityScore += deviceProperties.limits.maxImageDimension2D;

        
        return suitabilityScore;
    }

    uint32_t findQueueFamilies(const vk::raii::PhysicalDevice& physicalDevice) {
        std::vector<vk::QueueFamilyProperties>queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
        auto graphicsQFP = std::find_if(queueFamilyProperties.begin(), queueFamilyProperties.end(), [](vk::QueueFamilyProperties const& qfp) { return qfp.queueFlags & vk::QueueFlagBits::eGraphics; });
        return static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), graphicsQFP));
    }

    void setupDebugMessenger() {
        if (!enableValidationLayers) return;
        //messageSeverity allows you to specify all the types of severities you would like your callback to be called for
        //.We’ve specified all types except for VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT here to receive notifications about possible problems while leaving out verbose general debug info.
        vk::DebugUtilsMessageSeverityFlagsEXT messageFlags{ vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError };
        //Similarly, the messageType field lets you filter which types of messages your callback is notified about. We’ve simply enabled all types here.
        vk::DebugUtilsMessageTypeFlagsEXT messageType{ vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation };
        vk::DebugUtilsMessengerCreateInfoEXT messengerCreateInfo{ .messageSeverity = messageFlags,
                                                         .messageType = messageType,
                                                         .pfnUserCallback=&debugCallback};
        debugMessenger = instance.createDebugUtilsMessengerEXT(messengerCreateInfo);
    }

    std::vector<const char*> getRequiredExtensions() {
        uint32_t glfwExtensionCount = 0;
        // this includes VK_KHR_Surface which we need for window interfacing
        // also the instance specific VK_KHR_win32_surface
        auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
        if (enableValidationLayers) {
            extensions.push_back(vk::EXTDebugUtilsExtensionName);
        }

        return extensions;
    }

    //The VKAPI_ATTR and VKAPI_CALL ensure that the function has the right signature for Vulkan to call it.
    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
        vk::DebugUtilsMessageTypeFlagsEXT type,
        const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* v) {
        std::cerr << "validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;

        return vk::False;
    }

    // choose color space
    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats) {
        // choosing color space as an sRGB format
        for (const auto& availableFormat : availableFormats) {
            if (availableFormat.format == vk::Format::eB8G8R8A8Srgb && availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
                return availableFormat;
            }
        }
        return availableFormats[0];
    }

    // choose swap mode
    vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes) {
        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == vk::PresentModeKHR::eMailbox) {
                return availablePresentMode;
            }
        }
        return vk::PresentModeKHR::eFifo;
    }

    // because display is high DPI, and pixels dont translate to screen coords 1:1
    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities) {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        }
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        return {
            std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
            std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
        };
    }

    void createSwapChain() {
        auto surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(*surface);
        auto swapChainSurfaceFormat = chooseSwapSurfaceFormat(physicalDevice.getSurfaceFormatsKHR(*surface));
        swapChainExtent = chooseSwapExtent(surfaceCapabilities);
        auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
        minImageCount = (surfaceCapabilities.maxImageCount > 0 && minImageCount > surfaceCapabilities.maxImageCount) ? surfaceCapabilities.maxImageCount : minImageCount;
        uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
        if (surfaceCapabilities.maxImageCount > 0 && imageCount > surfaceCapabilities.maxImageCount) {
            imageCount = surfaceCapabilities.maxImageCount;
        }
        vk::SwapchainCreateInfoKHR swapChainCreateInfo{.flags = vk::SwapchainCreateFlagsKHR(),
            .surface = *surface,
            .minImageCount = minImageCount,
            .imageFormat = swapChainSurfaceFormat.format,
            .imageColorSpace = swapChainSurfaceFormat.colorSpace,
            .imageExtent = swapChainExtent,
            .imageArrayLayers = 1,
            .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
            .imageSharingMode = vk::SharingMode::eExclusive,
            .preTransform = surfaceCapabilities.currentTransform,
            .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
            .presentMode = chooseSwapPresentMode(physicalDevice.getSurfacePresentModesKHR(*surface)),
            .clipped = true,
            .oldSwapchain = nullptr
        };
        swapChain = vk::raii::SwapchainKHR(device, swapChainCreateInfo);
        swapChainImages = swapChain.getImages();
        swapChainImageFormat = swapChainSurfaceFormat.format;
        
    }

    void createImageViews() {
        swapChainImageViews.clear();

        vk::ImageViewCreateInfo imageViewCreateInfo{ .viewType = vk::ImageViewType::e2D, .format = swapChainImageFormat,
          .subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 } };

        for (vk::Image img : swapChainImages) {
            imageViewCreateInfo.image = img;
            swapChainImageViews.emplace_back(device, imageViewCreateInfo);
        }
    }

    void createGraphicsPipeline() {
        vk::raii::ShaderModule shaderModule = createShaderModule(readFile("C:/Users/Sanuda/Documents/Sanu/vulkan/HelloTriangle/HelloTriangle/shaders/slang.spv"));
        vk::PipelineShaderStageCreateInfo vertShaderStageInfo{ .stage = vk::ShaderStageFlagBits::eVertex, .module = shaderModule,  .pName = "vertMain" };
        vk::PipelineShaderStageCreateInfo fragShaderStageInfo{ .stage = vk::ShaderStageFlagBits::eFragment, .module = shaderModule, .pName = "fragMain" };
        vk::PipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };
        // vertex stuff
        vk::PipelineVertexInputStateCreateInfo vertexInputInfo = {};
        // creating dynamic states so we can change later without making whole new pipeline
        // the pipeline will ignore and not set these so have to do it manually when drawing
        std::vector dynamicStates = {vk::DynamicState::eViewport,vk::DynamicState::eScissor};
        vk::PipelineDynamicStateCreateInfo dStateInfo = { .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
            .pDynamicStates = dynamicStates.data()
        };
        // setting data structure of reading the vertex data
        vk::PipelineInputAssemblyStateCreateInfo assemblyInfo = {.topology = vk::PrimitiveTopology::eTriangleList};
        // viewport
        vk::PipelineViewportStateCreateInfo viewportInfo = {.viewportCount = 1,
            .scissorCount = 1
        };
        // rasterizer
        vk::PipelineRasterizationStateCreateInfo rasterizerInfo = { .depthClampEnable = false,
            .rasterizerDiscardEnable = false,
            .polygonMode = vk::PolygonMode::eFill,
            .cullMode = vk::CullModeFlagBits::eBack,
            .frontFace = vk::FrontFace::eClockwise,
            .depthBiasEnable = vk::False,
            .depthBiasSlopeFactor = 1.0f,
            .lineWidth = 1.0f
        };
        // multisampling
        vk::PipelineMultisampleStateCreateInfo multisampleInfo = {.rasterizationSamples = vk::SampleCountFlagBits::e1,
            .sampleShadingEnable = vk::False
        };
        // color Blending 
        vk::PipelineColorBlendAttachmentState colorBlendAttachment = { .blendEnable = vk::False,
            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eA | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eG
        };
        vk::PipelineColorBlendStateCreateInfo colorBlendInfo = { .logicOpEnable = vk::False,
            .logicOp = vk::LogicOp::eCopy,
            .attachmentCount = 1,
            .pAttachments = &colorBlendAttachment
        };
        // pipleine layout
        vk::PipelineLayoutCreateInfo pipelineLayoutInfo = {.setLayoutCount = 0,
            .pushConstantRangeCount = 0
        };
        pipelineLayout = vk::raii::PipelineLayout(device,pipelineLayoutInfo);
        // redering info
        vk::PipelineRenderingCreateInfo renderInfo = { .colorAttachmentCount = 1,
            .pColorAttachmentFormats = &swapChainImageFormat
        };
        // creating the graphics pipeline
        // renderPass is nullptr because we are using dynamic render instead of traditional render
        vk::GraphicsPipelineCreateInfo graphicsPipelineInfo = {.pNext = &renderInfo,
            .stageCount = 2,
            .pStages = shaderStages,
            .pVertexInputState = &vertexInputInfo,
            .pInputAssemblyState = &assemblyInfo,
            .pViewportState = &viewportInfo,
            .pRasterizationState = &rasterizerInfo,
            .pMultisampleState = &multisampleInfo,
            .pColorBlendState = &colorBlendInfo,
            .pDynamicState = &dStateInfo,
            .layout = pipelineLayout,
            .renderPass = nullptr
        };
        //graphicsPipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
        //graphicsPipelineInfo.basePipelineIndex = -1; // Optional
        //vk::PipelineCreateFlags f(VK_PIPELINE_CREATE_DERIVATIVE_BIT); // idk if this works
        //graphicsPipelineInfo.flags = f; // idk if this works

        graphicsPipeline = vk::raii::Pipeline(device, nullptr, graphicsPipelineInfo);
    }

    [[nodiscard]] 
    vk::raii::ShaderModule createShaderModule(const std::vector<char>& code) const {
        vk::ShaderModuleCreateInfo createInfo{ .codeSize = code.size() * sizeof(char), .pCode = reinterpret_cast<const uint32_t*>(code.data()) };
        vk::raii::ShaderModule shaderModule{ device, createInfo };
        return shaderModule;
    }

    static std::vector<char> readFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            throw std::runtime_error("failed to open file!");
        }
        // can do this because ios::ate makes us read at end of file and doing that we know the size
        std::vector<char> buffer(file.tellg());
        // seek to beginning
        file.seekg(0, std::ios::beg);
        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        file.close();

        return buffer;
    }

    void createCommandPool() {
        //probably a better way to retrive this
        std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
        auto graphicsQueueFamilyProperty = std::ranges::find_if(queueFamilyProperties, [](auto const& qfp)
            { return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0); });
        auto graphicsIndex = static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), graphicsQueueFamilyProperty));
        
        vk::CommandPoolCreateInfo commandPoolInfo = { .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = graphicsIndex
        };

        commandPool = vk::raii::CommandPool(device, commandPoolInfo);
    }

    void createCommandBuffer() {
        vk::CommandBufferAllocateInfo cmdBufferAllocateInfo = {.commandPool = commandPool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1
        };

        commandBuffer = std::move(vk::raii::CommandBuffers(device, cmdBufferAllocateInfo).front());
    }

    void recordCommandBuffer(uint32_t imageIndex){
        commandBuffer.begin({});
        // Before starting rendering, transition the swapchain image to COLOR_ATTACHMENT_OPTIMAL
        transition_image_layout(
            imageIndex,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eColorAttachmentOptimal,
            {},                                                        // srcAccessMask (no need to wait for previous operations)
            vk::AccessFlagBits2::eColorAttachmentWrite,                // dstAccessMask
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,        // srcStage
            vk::PipelineStageFlagBits2::eColorAttachmentOutput         // dstStage
        );
        vk::ClearValue              clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
        vk::RenderingAttachmentInfo attachmentInfo = {
            .imageView = swapChainImageViews[imageIndex],
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue = clearColor };
        vk::RenderingInfo renderingInfo = {
            .renderArea = {.offset = {0, 0}, .extent = swapChainExtent},
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &attachmentInfo };

        commandBuffer.beginRendering(renderingInfo);
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);
        commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height), 0.0f, 1.0f));
        commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent));
        commandBuffer.draw(3, 1, 0, 0);
        commandBuffer.endRendering();
        // After rendering, transition the swapchain image to PRESENT_SRC
        transition_image_layout(
            imageIndex,
            vk::ImageLayout::eColorAttachmentOptimal,
            vk::ImageLayout::ePresentSrcKHR,
            vk::AccessFlagBits2::eColorAttachmentWrite,                // srcAccessMask
            {},                                                        // dstAccessMask
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,        // srcStage
            vk::PipelineStageFlagBits2::eBottomOfPipe                  // dstStage
        );
        commandBuffer.end();
    }

    void transition_image_layout(uint32_t imageIndex, vk::ImageLayout oldLayout,
        vk::ImageLayout newLayout, vk::AccessFlags2 srcAccessMask, vk::AccessFlags2 dstAccessMask,
        vk::PipelineStageFlags2 srcStageMask, vk::PipelineStageFlags2 dstStageMask){
        vk::ImageMemoryBarrier2 barrier = {
            .srcStageMask = srcStageMask, .srcAccessMask = srcAccessMask,
            .dstStageMask = dstStageMask, .dstAccessMask = dstAccessMask,
            .oldLayout = oldLayout, .newLayout = newLayout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = swapChainImages[imageIndex],
            .subresourceRange = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };
        vk::DependencyInfo dependencyInfo = {
        .dependencyFlags = {},
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier
        };
        commandBuffer.pipelineBarrier2(dependencyInfo);
    }


    //At a high level, rendering a frame in Vulkan consists of a common set of steps:
    // 1. Wait for the previous frame to finish
    // 2. Acquire an image from the swap chain
    // 3. Record a command buffer which draws the scene onto that image
    // 4. Submit the recorded command buffer
    // 5. Present the swap chain image

    void drawFrame() {
        graphicsQueue.waitIdle();
        
        // 1. wait  pointer to fences, wait for all fences is true, max block time is UINT64_MAX
        auto fenceResult = device.waitForFences(*drawFence, vk::True, UINT64_MAX);
        // 2. Acquire an image from the swap chain
        auto [result, imageIndex] = swapChain.acquireNextImage(UINT64_MAX, *presentCompleteSemaphore, nullptr);
        // 3. Record a command buffer which draws the scene onto that image
        recordCommandBuffer(imageIndex);
        device.resetFences(*drawFence);
        // prepare submit
        vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
        const vk::SubmitInfo submitInfo{
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &*presentCompleteSemaphore,
            .pWaitDstStageMask = &waitDestinationStageMask,
            .commandBufferCount = 1,
            .pCommandBuffers = &*commandBuffer,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &*renderFinishedSemaphore 
        };
        // 4. submit
        graphicsQueue.submit(submitInfo, *drawFence);

        const vk::PresentInfoKHR presentInfoKHR{
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &*renderFinishedSemaphore,
            .swapchainCount = 1,
            .pSwapchains = &*swapChain,
            .pImageIndices = &imageIndex 
        };
        // 5. present
        result = presentQueue.presentKHR(presentInfoKHR);
        switch (result)
        {
        case vk::Result::eSuccess:
            break;
        case vk::Result::eSuboptimalKHR:
            std::cout << "vk::Queue::presentKHR returned vk::Result::eSuboptimalKHR !\n";
            break;
        default:
            break;        // an unexpected result is returned!
        }
    }

    void createSyncObjects() {
        presentCompleteSemaphore = vk::raii::Semaphore(device, vk::SemaphoreCreateInfo());
        renderFinishedSemaphore = vk::raii::Semaphore(device, vk::SemaphoreCreateInfo());
        drawFence = vk::raii::Fence(device, { .flags = vk::FenceCreateFlagBits::eSignaled });
    }



};

int main() {
    

    try {
        HelloTriangleApplication app;
        app.run();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}