#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <cstdint>
#include <vector>

struct VkFrame
{
    VkSemaphore imageAvailable{};
    VkSemaphore renderFinished{};
    VkFence inFlight{};
    VkCommandBuffer cmd{};
};

struct VkContext
{
    GLFWwindow *window{};

    VkInstance instance{};
    VkDebugUtilsMessengerEXT debugMessenger{};
    VkSurfaceKHR surface{};

    VkPhysicalDevice phys{};
    VkDevice device{};

    uint32_t graphicsQFamily = UINT32_MAX;
    VkQueue graphicsQ{};
    VkQueue presentQ{};

    VkSwapchainKHR swapchain{};
    VkFormat swapFormat{};
    VkExtent2D swapExtent{};
    std::vector<VkImage> swapImages;
    std::vector<VkImageView> swapViews;

    VkRenderPass renderPass{};
    VkImage depthImage{};
    VkDeviceMemory depthMem{};
    VkImageView depthView{};
    VkFormat depthFormat{};

    std::vector<VkFramebuffer> framebuffers;

    VkCommandPool cmdPool{};
    static constexpr uint32_t kMaxFrames = 2;
    VkFrame frames[kMaxFrames]{};
    uint32_t frameIndex = 0;

    bool framebufferResized = false;

    void init(GLFWwindow *win, bool enableValidation);
    void cleanup();

    // swapchain dependent
    void recreateSwapchain();

    // per-frame
    VkCommandBuffer beginFrame(uint32_t &outImageIndex);
    void endFrame(uint32_t imageIndex);

    // util
    void waitIdle();
};

void vkFramebufferResizeCallback(GLFWwindow *window, int width, int height);
