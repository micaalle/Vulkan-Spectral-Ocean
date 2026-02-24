#include "vk_helpers.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <fstream>
#include <stdexcept>

std::vector<uint8_t> readFileBinary(const std::string &path)
{
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("Failed to open file: " + path);
    size_t size = (size_t)file.tellg();
    std::vector<uint8_t> buf(size);
    file.seekg(0);
    file.read(reinterpret_cast<char *>(buf.data()), (std::streamsize)size);
    return buf;
}

VkShaderModule createShaderModule(VkDevice device, const std::vector<uint8_t> &code)
{
    VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    ci.codeSize = code.size();
    ci.pCode = reinterpret_cast<const uint32_t *>(code.data());
    VkShaderModule m{};
    if (vkCreateShaderModule(device, &ci, nullptr, &m) != VK_SUCCESS)
        throw std::runtime_error("vkCreateShaderModule failed");
    return m;
}

uint32_t findMemoryType(VkPhysicalDevice phys, uint32_t typeFilter, VkMemoryPropertyFlags props)
{
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(phys, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++)
    {
        if ((typeFilter & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props)
            return i;
    }
    throw std::runtime_error("Failed to find suitable memory type");
}

AllocatedBuffer createBuffer(VkPhysicalDevice phys, VkDevice device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props)
{
    AllocatedBuffer b{};
    b.size = size;

    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = size;
    bi.usage = usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bi, nullptr, &b.buffer) != VK_SUCCESS)
        throw std::runtime_error("vkCreateBuffer failed");

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(device, b.buffer, &req);

    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = findMemoryType(phys, req.memoryTypeBits, props);

    if (vkAllocateMemory(device, &ai, nullptr, &b.memory) != VK_SUCCESS)
        throw std::runtime_error("vkAllocateMemory(buffer) failed");

    vkBindBufferMemory(device, b.buffer, b.memory, 0);
    return b;
}

void destroyBuffer(VkDevice device, AllocatedBuffer &b)
{
    if (b.buffer)
        vkDestroyBuffer(device, b.buffer, nullptr);
    if (b.memory)
        vkFreeMemory(device, b.memory, nullptr);
    b = {};
}

VkImageView createImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspect, uint32_t mipLevels,
                            VkImageViewType type, uint32_t baseMip, uint32_t baseLayer, uint32_t layerCount)
{
    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = image;
    vi.viewType = type;
    vi.format = format;
    vi.subresourceRange.aspectMask = aspect;
    vi.subresourceRange.baseMipLevel = baseMip;
    vi.subresourceRange.levelCount = mipLevels;
    vi.subresourceRange.baseArrayLayer = baseLayer;
    vi.subresourceRange.layerCount = layerCount;

    VkImageView view{};
    if (vkCreateImageView(device, &vi, nullptr, &view) != VK_SUCCESS)
        throw std::runtime_error("vkCreateImageView failed");
    return view;
}

AllocatedImage createImage2D(VkPhysicalDevice phys, VkDevice device, uint32_t w, uint32_t h, uint32_t mipLevels, VkFormat format,
                             VkImageUsageFlags usage, VkImageAspectFlags aspect, VkSampleCountFlagBits samples, VkImageTiling tiling,
                             VkImageCreateFlags flags, uint32_t layers)
{
    AllocatedImage img{};
    img.width = w;
    img.height = h;
    img.mipLevels = mipLevels;
    img.layers = layers;
    img.format = format;

    VkImageCreateInfo ii{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ii.flags = flags;
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.extent = {w, h, 1};
    ii.mipLevels = mipLevels;
    ii.arrayLayers = layers;
    ii.format = format;
    ii.tiling = tiling;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ii.usage = usage;
    ii.samples = samples;
    ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &ii, nullptr, &img.image) != VK_SUCCESS)
        throw std::runtime_error("vkCreateImage failed");

    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(device, img.image, &req);

    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = findMemoryType(phys, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &ai, nullptr, &img.memory) != VK_SUCCESS)
        throw std::runtime_error("vkAllocateMemory(image) failed");

    vkBindImageMemory(device, img.image, img.memory, 0);

    // default view for 2D
    VkImageViewType vt = (layers == 6 && (flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT)) ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
    img.view = createImageView(device, img.image, format, aspect, mipLevels, vt, 0, 0, layers);

    return img;
}

void destroyImage(VkDevice device, AllocatedImage &img)
{
    if (img.view)
        vkDestroyImageView(device, img.view, nullptr);
    if (img.image)
        vkDestroyImage(device, img.image, nullptr);
    if (img.memory)
        vkFreeMemory(device, img.memory, nullptr);
    img = {};
}

VkSampler createSampler(VkDevice device, VkFilter filter, VkSamplerAddressMode addressMode, float maxLod, bool enableAniso, float maxAniso)
{
    VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    si.magFilter = filter;
    si.minFilter = filter;
    si.addressModeU = addressMode;
    si.addressModeV = addressMode;
    si.addressModeW = addressMode;
    si.anisotropyEnable = enableAniso ? VK_TRUE : VK_FALSE;
    si.maxAnisotropy = enableAniso ? maxAniso : 1.0f;
    si.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    si.unnormalizedCoordinates = VK_FALSE;
    si.compareEnable = VK_FALSE;
    si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    si.minLod = 0.0f;
    si.maxLod = maxLod;

    VkSampler sampler{};
    if (vkCreateSampler(device, &si, nullptr, &sampler) != VK_SUCCESS)
        throw std::runtime_error("vkCreateSampler failed");
    return sampler;
}

VkCommandBuffer beginSingleTimeCommands(VkDevice device, VkCommandPool pool)
{
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool = pool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;

    VkCommandBuffer cmd{};
    vkAllocateCommandBuffers(device, &ai, &cmd);

    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &bi);
    return cmd;
}

void endSingleTimeCommands(VkDevice device, VkQueue queue, VkCommandPool pool, VkCommandBuffer cmd)
{
    vkEndCommandBuffer(cmd);

    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;

    vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(device, pool, 1, &cmd);
}

static VkAccessFlags accessForLayout(VkImageLayout layout)
{
    switch (layout)
    {
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        return VK_ACCESS_TRANSFER_WRITE_BIT;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        return VK_ACCESS_TRANSFER_READ_BIT;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        return VK_ACCESS_SHADER_READ_BIT;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
        return VK_ACCESS_SHADER_READ_BIT;
    case VK_IMAGE_LAYOUT_GENERAL:
        return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    default:
        return 0;
    }
}

static VkPipelineStageFlags stageForLayout(VkImageLayout layout)
{
    switch (layout)
    {
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        return VK_PIPELINE_STAGE_TRANSFER_BIT;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
    case VK_IMAGE_LAYOUT_GENERAL:
        return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    default:
        return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    }
}

void transitionImageLayout(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
                           VkImageAspectFlags aspect, uint32_t mipLevels, uint32_t layers)
{
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspect;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = layers;

    VkPipelineStageFlags srcStage = stageForLayout(oldLayout);
    VkPipelineStageFlags dstStage = stageForLayout(newLayout);
    barrier.srcAccessMask = accessForLayout(oldLayout);
    barrier.dstAccessMask = accessForLayout(newLayout);

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void copyBufferToImage(VkCommandBuffer cmd, VkBuffer buffer, VkImage image, uint32_t w, uint32_t h, uint32_t layer)
{
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = layer;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {w, h, 1};

    vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

void generateMipmaps(VkPhysicalDevice phys, VkCommandBuffer cmd, VkImage image, VkFormat format,
                     int32_t texWidth, int32_t texHeight, uint32_t mipLevels, uint32_t layers)
{
    VkFormatProperties props{};
    vkGetPhysicalDeviceFormatProperties(phys, format, &props);
    if (!(props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
    {
        throw std::runtime_error("Format does not support linear blitting for mipmaps");
    }

    for (uint32_t layer = 0; layer < layers; ++layer)
    {
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.image = image;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseArrayLayer = layer;
        barrier.subresourceRange.layerCount = 1;
        barrier.subresourceRange.levelCount = 1;

        int32_t mipW = texWidth;
        int32_t mipH = texHeight;

        for (uint32_t i = 1; i < mipLevels; ++i)
        {
            // trans i-1 to SRC
            barrier.subresourceRange.baseMipLevel = i - 1;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

            VkImageBlit blit{};
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel = i - 1;
            blit.srcSubresource.baseArrayLayer = layer;
            blit.srcSubresource.layerCount = 1;
            blit.srcOffsets[0] = {0, 0, 0};
            blit.srcOffsets[1] = {mipW, mipH, 1};

            int32_t nextW = std::max(1, mipW / 2);
            int32_t nextH = std::max(1, mipH / 2);

            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel = i;
            blit.dstSubresource.baseArrayLayer = layer;
            blit.dstSubresource.layerCount = 1;
            blit.dstOffsets[0] = {0, 0, 0};
            blit.dstOffsets[1] = {nextW, nextH, 1};

            vkCmdBlitImage(cmd,
                           image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1, &blit, VK_FILTER_LINEAR);

            // trans i-1 to SHADER_READ
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

            mipW = nextW;
            mipH = nextH;
        }

        // trans last mip to SHADER_READ
        barrier.subresourceRange.baseMipLevel = mipLevels - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }
}

uint16_t floatToHalf(float f)
{
    union
    {
        float f;
        uint32_t u;
    } v{f};
    uint32_t sign = (v.u >> 31) & 1u;
    int32_t exp = int32_t((v.u >> 23) & 0xFFu) - 127;
    uint32_t mant = v.u & 0x7FFFFFu;

    uint16_t hsign = uint16_t(sign << 15);

    if (exp == 128)
    {
        uint16_t hexp = 0x1Fu;
        uint16_t hmant = (mant ? 0x200u : 0u);
        return uint16_t(hsign | (hexp << 10) | hmant);
    }

    if (exp > 15)
    {
        return uint16_t(hsign | (0x1Fu << 10)); //
    }
    if (exp < -14)
    {

        if (exp < -24)
            return hsign;
        mant |= 0x800000u;
        uint32_t shift = uint32_t(-exp - 14);
        uint32_t hm = mant >> (shift + 13);

        uint32_t rem = mant >> (shift + 12);
        if (rem & 1u)
            hm += 1u;
        return uint16_t(hsign | uint16_t(hm));
    }

    uint16_t hexp = uint16_t(exp + 15);
    uint32_t hm = mant >> 13;
    if (mant & 0x00001000u)
        hm += 1u;

    return uint16_t(hsign | (hexp << 10) | uint16_t(hm & 0x3FFu));
}
