#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>
#include <vector>

struct AllocatedBuffer
{
    VkBuffer buffer{};
    VkDeviceMemory memory{};
    VkDeviceSize size{};
};

struct AllocatedImage
{
    VkImage image{};
    VkDeviceMemory memory{};
    VkImageView view{};
    uint32_t width{};
    uint32_t height{};
    uint32_t mipLevels{};
    uint32_t layers{1};
    VkFormat format{};
};

std::vector<uint8_t> readFileBinary(const std::string &path);
VkShaderModule createShaderModule(VkDevice device, const std::vector<uint8_t> &code);

uint32_t findMemoryType(VkPhysicalDevice phys, uint32_t typeFilter, VkMemoryPropertyFlags props);

AllocatedBuffer createBuffer(
    VkPhysicalDevice phys,
    VkDevice device,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags props);

void destroyBuffer(VkDevice device, AllocatedBuffer &b);

AllocatedImage createImage2D(
    VkPhysicalDevice phys,
    VkDevice device,
    uint32_t w, uint32_t h,
    uint32_t mipLevels,
    VkFormat format,
    VkImageUsageFlags usage,
    VkImageAspectFlags aspect,
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT,
    VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL,
    VkImageCreateFlags flags = 0,
    uint32_t layers = 1);

void destroyImage(VkDevice device, AllocatedImage &img);

VkImageView createImageView(
    VkDevice device,
    VkImage image,
    VkFormat format,
    VkImageAspectFlags aspect,
    uint32_t mipLevels,
    VkImageViewType type = VK_IMAGE_VIEW_TYPE_2D,
    uint32_t baseMip = 0,
    uint32_t baseLayer = 0,
    uint32_t layerCount = 1);

VkSampler createSampler(
    VkDevice device,
    VkFilter filter,
    VkSamplerAddressMode addressMode,
    float maxLod,
    bool enableAniso,
    float maxAniso);

VkCommandBuffer beginSingleTimeCommands(VkDevice device, VkCommandPool pool);
void endSingleTimeCommands(VkDevice device, VkQueue queue, VkCommandPool pool, VkCommandBuffer cmd);

void transitionImageLayout(
    VkCommandBuffer cmd,
    VkImage image,
    VkImageLayout oldLayout,
    VkImageLayout newLayout,
    VkImageAspectFlags aspect,
    uint32_t mipLevels,
    uint32_t layers = 1);

void copyBufferToImage(
    VkCommandBuffer cmd,
    VkBuffer buffer,
    VkImage image,
    uint32_t w, uint32_t h,
    uint32_t layer = 0);

void generateMipmaps(
    VkPhysicalDevice phys,
    VkCommandBuffer cmd,
    VkImage image,
    VkFormat format,
    int32_t texWidth,
    int32_t texHeight,
    uint32_t mipLevels,
    uint32_t layers = 1);

uint16_t floatToHalf(float f);
