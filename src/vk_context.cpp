#include "vk_context.h"
#include "vk_helpers.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <optional>
#include <stdexcept>

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void*){
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        std::cerr << "[Vulkan] " << data->pMessage << "\n";
    }
    return VK_FALSE;
}

static VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pMessenger){
    auto fn = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (fn) return fn(instance, pCreateInfo, pAllocator, pMessenger);
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

static void DestroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT messenger,
    const VkAllocationCallbacks* pAllocator){
    auto fn = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (fn) fn(instance, messenger, pAllocator);
}

static VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats){
    // To match the original OpenGL look (manual tonemap + gamma in shader),
    // prefer an UNORM swapchain format (no automatic sRGB encode on write).
    // If UNORM isn't available, fall back to sRGB.
    for (auto& f : formats){
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return f;
    }
    for (auto& f : formats){
        if (f.format == VK_FORMAT_R8G8B8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return f;
    }
    for (auto& f : formats){
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return f;
    }
    return formats[0];
}

static VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& modes){
    for (auto m : modes) if (m == VK_PRESENT_MODE_MAILBOX_KHR) return m;
    return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR& caps, GLFWwindow* window){
    if (caps.currentExtent.width != UINT32_MAX) return caps.currentExtent;
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    VkExtent2D e{(uint32_t)w, (uint32_t)h};
    e.width = std::clamp(e.width, caps.minImageExtent.width, caps.maxImageExtent.width);
    e.height = std::clamp(e.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    return e;
}

static VkFormat findDepthFormat(VkPhysicalDevice phys){
    const VkFormat candidates[] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
    };
    for (VkFormat f : candidates){
        VkFormatProperties props{};
        vkGetPhysicalDeviceFormatProperties(phys, f, &props);
        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
            return f;
    }
    throw std::runtime_error("No supported depth format");
}

struct SwapchainSupport {
    VkSurfaceCapabilitiesKHR caps{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> modes;
};

static SwapchainSupport querySwapchainSupport(VkPhysicalDevice phys, VkSurfaceKHR surface){
    SwapchainSupport s;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys, surface, &s.caps);

    uint32_t count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &count, nullptr);
    s.formats.resize(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &count, s.formats.data());

    count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &count, nullptr);
    s.modes.resize(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &count, s.modes.data());

    return s;
}

static bool checkDevice(VkPhysicalDevice dev, VkSurfaceKHR surface, uint32_t& outQFamily){
    uint32_t qCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &qCount, nullptr);
    std::vector<VkQueueFamilyProperties> qProps(qCount);
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &qCount, qProps.data());

    // Require graphics + compute + present in one queue for simplicity
    for (uint32_t i = 0; i < qCount; ++i){
        VkBool32 present = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &present);
        if (present && (qProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && (qProps[i].queueFlags & VK_QUEUE_COMPUTE_BIT)){
            outQFamily = i;
            // check swapchain
            auto sc = querySwapchainSupport(dev, surface);
            if (!sc.formats.empty() && !sc.modes.empty()) return true;
        }
    }
    return false;
}

void vkFramebufferResizeCallback(GLFWwindow* window, int, int){
    auto* ctx = reinterpret_cast<VkContext*>(glfwGetWindowUserPointer(window));
    if (ctx) ctx->framebufferResized = true;
}

void VkContext::init(GLFWwindow* win, bool enableValidation){
    window = win;

    // Instance
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "VulkanOcean";
    app.applicationVersion = VK_MAKE_VERSION(1,0,0);
    app.pEngineName = "";
    app.engineVersion = VK_MAKE_VERSION(1,0,0);
    app.apiVersion = VK_API_VERSION_1_2;

    uint32_t extCount = 0;
    const char** exts = glfwGetRequiredInstanceExtensions(&extCount);
    std::vector<const char*> extensions(exts, exts + extCount);
    if (enableValidation) extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    std::vector<const char*> layers;
    if (enableValidation) layers.push_back("VK_LAYER_KHRONOS_validation");

    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ci.pApplicationInfo = &app;
    ci.enabledExtensionCount = (uint32_t)extensions.size();
    ci.ppEnabledExtensionNames = extensions.data();
    ci.enabledLayerCount = (uint32_t)layers.size();
    ci.ppEnabledLayerNames = layers.data();

    VkDebugUtilsMessengerCreateInfoEXT dbg{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    if (enableValidation){
        dbg.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        dbg.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        dbg.pfnUserCallback = debugCallback;
        ci.pNext = &dbg;
    }

    if (vkCreateInstance(&ci, nullptr, &instance) != VK_SUCCESS)
        throw std::runtime_error("vkCreateInstance failed");

    if (enableValidation){
        if (CreateDebugUtilsMessengerEXT(instance, &dbg, nullptr, &debugMessenger) != VK_SUCCESS)
            std::cerr << "Warning: debug messenger not created\n";
    }

    // Surface
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
        throw std::runtime_error("glfwCreateWindowSurface failed");

    // Pick physical device
    uint32_t devCount = 0;
    vkEnumeratePhysicalDevices(instance, &devCount, nullptr);
    if (devCount == 0) throw std::runtime_error("No Vulkan physical devices found");
    std::vector<VkPhysicalDevice> devices(devCount);
    vkEnumeratePhysicalDevices(instance, &devCount, devices.data());

    for (auto d : devices){
        uint32_t qf = UINT32_MAX;
        if (checkDevice(d, surface, qf)){
            phys = d;
            graphicsQFamily = qf;
            break;
        }
    }
    if (!phys) throw std::runtime_error("No suitable physical device found");

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(phys, &props);
    std::cout << "Using GPU: " << props.deviceName << "\n";

    // Device
    float qPri = 1.0f;
    VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex = graphicsQFamily;
    qci.queueCount = 1;
    qci.pQueuePriorities = &qPri;

    VkPhysicalDeviceFeatures feats{};
    feats.samplerAnisotropy = VK_TRUE;
    feats.fillModeNonSolid = VK_TRUE;

    const char* devExts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    dci.pEnabledFeatures = &feats;
    dci.enabledExtensionCount = 1;
    dci.ppEnabledExtensionNames = devExts;
    if (enableValidation){
        dci.enabledLayerCount = (uint32_t)layers.size();
        dci.ppEnabledLayerNames = layers.data();
    }

    if (vkCreateDevice(phys, &dci, nullptr, &device) != VK_SUCCESS)
        throw std::runtime_error("vkCreateDevice failed");

    vkGetDeviceQueue(device, graphicsQFamily, 0, &graphicsQ);
    presentQ = graphicsQ;

    // Command pool
    VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pci.queueFamilyIndex = graphicsQFamily;
    pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(device, &pci, nullptr, &cmdPool) != VK_SUCCESS)
        throw std::runtime_error("vkCreateCommandPool failed");

    // Allocate per-frame cmd buffers + sync
    VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cai.commandPool = cmdPool;
    cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cai.commandBufferCount = kMaxFrames;
    VkCommandBuffer cbufs[kMaxFrames]{};
    if (vkAllocateCommandBuffers(device, &cai, cbufs) != VK_SUCCESS)
        throw std::runtime_error("vkAllocateCommandBuffers failed");

    for (uint32_t i=0;i<kMaxFrames;i++){
        frames[i].cmd = cbufs[i];

        VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        vkCreateSemaphore(device, &sci, nullptr, &frames[i].imageAvailable);
        vkCreateSemaphore(device, &sci, nullptr, &frames[i].renderFinished);

        VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        vkCreateFence(device, &fci, nullptr, &frames[i].inFlight);
    }

    depthFormat = findDepthFormat(phys);

    // Swapchain + renderpass
    recreateSwapchain();

    // Hook resize callback
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, vkFramebufferResizeCallback);
}

void VkContext::waitIdle(){
    if (device) vkDeviceWaitIdle(device);
}

void VkContext::cleanup(){
    waitIdle();

    for (auto fb : framebuffers) vkDestroyFramebuffer(device, fb, nullptr);
    framebuffers.clear();

    if (depthView) vkDestroyImageView(device, depthView, nullptr);
    if (depthImage) vkDestroyImage(device, depthImage, nullptr);
    if (depthMem) vkFreeMemory(device, depthMem, nullptr);

    if (renderPass) vkDestroyRenderPass(device, renderPass, nullptr);

    for (auto v : swapViews) vkDestroyImageView(device, v, nullptr);
    swapViews.clear();

    if (swapchain) vkDestroySwapchainKHR(device, swapchain, nullptr);

    for (uint32_t i=0;i<kMaxFrames;i++){
        if (frames[i].imageAvailable) vkDestroySemaphore(device, frames[i].imageAvailable, nullptr);
        if (frames[i].renderFinished) vkDestroySemaphore(device, frames[i].renderFinished, nullptr);
        if (frames[i].inFlight) vkDestroyFence(device, frames[i].inFlight, nullptr);
    }

    if (cmdPool) vkDestroyCommandPool(device, cmdPool, nullptr);

    if (device) vkDestroyDevice(device, nullptr);

    if (surface) vkDestroySurfaceKHR(instance, surface, nullptr);

    if (debugMessenger) DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);

    if (instance) vkDestroyInstance(instance, nullptr);

    *this = {};
}

void VkContext::recreateSwapchain(){
    int w=0, h=0;
    glfwGetFramebufferSize(window, &w, &h);
    while (w == 0 || h == 0){
        glfwWaitEvents();
        glfwGetFramebufferSize(window, &w, &h);
    }

    waitIdle();

    // Cleanup old
    for (auto fb : framebuffers) vkDestroyFramebuffer(device, fb, nullptr);
    framebuffers.clear();

    if (depthView) vkDestroyImageView(device, depthView, nullptr);
    if (depthImage) vkDestroyImage(device, depthImage, nullptr);
    if (depthMem) vkFreeMemory(device, depthMem, nullptr);
    depthView = {}; depthImage = {}; depthMem = {};

    for (auto v : swapViews) vkDestroyImageView(device, v, nullptr);
    swapViews.clear();

    if (swapchain) vkDestroySwapchainKHR(device, swapchain, nullptr);
    swapchain = {};

    if (renderPass) { vkDestroyRenderPass(device, renderPass, nullptr); renderPass = {}; }

    // Create swapchain
    auto sc = querySwapchainSupport(phys, surface);
    VkSurfaceFormatKHR surfFmt = chooseSurfaceFormat(sc.formats);
    VkPresentModeKHR present = choosePresentMode(sc.modes);
    VkExtent2D extent = chooseExtent(sc.caps, window);

    uint32_t imageCount = sc.caps.minImageCount + 1;
    if (sc.caps.maxImageCount > 0 && imageCount > sc.caps.maxImageCount)
        imageCount = sc.caps.maxImageCount;

    VkSwapchainCreateInfoKHR sci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    sci.surface = surface;
    sci.minImageCount = imageCount;
    sci.imageFormat = surfFmt.format;
    sci.imageColorSpace = surfFmt.colorSpace;
    sci.imageExtent = extent;
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform = sc.caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode = present;
    sci.clipped = VK_TRUE;
    sci.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(device, &sci, nullptr, &swapchain) != VK_SUCCESS)
        throw std::runtime_error("vkCreateSwapchainKHR failed");

    swapFormat = surfFmt.format;
    swapExtent = extent;

    uint32_t scCount = 0;
    vkGetSwapchainImagesKHR(device, swapchain, &scCount, nullptr);
    swapImages.resize(scCount);
    vkGetSwapchainImagesKHR(device, swapchain, &scCount, swapImages.data());

    swapViews.resize(scCount);
    for (uint32_t i=0;i<scCount;i++){
        swapViews[i] = createImageView(device, swapImages[i], swapFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
    }

    // depth
    VkImageCreateInfo di{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    di.imageType = VK_IMAGE_TYPE_2D;
    di.extent = {swapExtent.width, swapExtent.height, 1};
    di.mipLevels = 1;
    di.arrayLayers = 1;
    di.format = depthFormat;
    di.tiling = VK_IMAGE_TILING_OPTIMAL;
    di.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    di.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    di.samples = VK_SAMPLE_COUNT_1_BIT;
    di.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &di, nullptr, &depthImage) != VK_SUCCESS)
        throw std::runtime_error("vkCreateImage(depth) failed");

    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(device, depthImage, &req);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = findMemoryType(phys, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(device, &ai, nullptr, &depthMem) != VK_SUCCESS)
        throw std::runtime_error("vkAllocateMemory(depth) failed");
    vkBindImageMemory(device, depthImage, depthMem, 0);

    depthView = createImageView(device, depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);

    // render pass
    VkAttachmentDescription color{};
    color.format = swapFormat;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depth{};
    depth.format = depthFormat;
    depth.samples = VK_SAMPLE_COUNT_1_BIT;
    depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription sub{};
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments = &colorRef;
    sub.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.srcAccessMask = 0;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription,2> atts{color, depth};

    VkRenderPassCreateInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rp.attachmentCount = (uint32_t)atts.size();
    rp.pAttachments = atts.data();
    rp.subpassCount = 1;
    rp.pSubpasses = &sub;
    rp.dependencyCount = 1;
    rp.pDependencies = &dep;

    if (vkCreateRenderPass(device, &rp, nullptr, &renderPass) != VK_SUCCESS)
        throw std::runtime_error("vkCreateRenderPass failed");

    // framebuffers
    framebuffers.resize(scCount);
    for (uint32_t i=0;i<scCount;i++){
        VkImageView attachments[] = { swapViews[i], depthView };
        VkFramebufferCreateInfo fbi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fbi.renderPass = renderPass;
        fbi.attachmentCount = 2;
        fbi.pAttachments = attachments;
        fbi.width = swapExtent.width;
        fbi.height = swapExtent.height;
        fbi.layers = 1;
        if (vkCreateFramebuffer(device, &fbi, nullptr, &framebuffers[i]) != VK_SUCCESS)
            throw std::runtime_error("vkCreateFramebuffer failed");
    }

    framebufferResized = false;
}

VkCommandBuffer VkContext::beginFrame(uint32_t& outImageIndex){
    VkFrame& fr = frames[frameIndex];
    vkWaitForFences(device, 1, &fr.inFlight, VK_TRUE, UINT64_MAX);

    VkResult acq = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, fr.imageAvailable, VK_NULL_HANDLE, &outImageIndex);
    if (acq == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return VK_NULL_HANDLE;
    }
    if (acq != VK_SUCCESS && acq != VK_SUBOPTIMAL_KHR)
        throw std::runtime_error("vkAcquireNextImageKHR failed");

    vkResetFences(device, 1, &fr.inFlight);
    vkResetCommandBuffer(fr.cmd, 0);

    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    if (vkBeginCommandBuffer(fr.cmd, &bi) != VK_SUCCESS)
        throw std::runtime_error("vkBeginCommandBuffer failed");

    return fr.cmd;
}

void VkContext::endFrame(uint32_t imageIndex){
    VkFrame& fr = frames[frameIndex];

    if (vkEndCommandBuffer(fr.cmd) != VK_SUCCESS)
        throw std::runtime_error("vkEndCommandBuffer failed");

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = &fr.imageAvailable;
    si.pWaitDstStageMask = &waitStage;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &fr.cmd;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &fr.renderFinished;

    if (vkQueueSubmit(graphicsQ, 1, &si, fr.inFlight) != VK_SUCCESS)
        throw std::runtime_error("vkQueueSubmit failed");

    VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &fr.renderFinished;
    pi.swapchainCount = 1;
    pi.pSwapchains = &swapchain;
    pi.pImageIndices = &imageIndex;

    VkResult pres = vkQueuePresentKHR(presentQ, &pi);
    if (pres == VK_ERROR_OUT_OF_DATE_KHR || pres == VK_SUBOPTIMAL_KHR || framebufferResized){
        recreateSwapchain();
    } else if (pres != VK_SUCCESS){
        throw std::runtime_error("vkQueuePresentKHR failed");
    }

    frameIndex = (frameIndex + 1) % kMaxFrames;
}
