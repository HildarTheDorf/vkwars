#include "Uploader.hpp"

#include "BadVkResult.hpp"

#include <cstring>
#include <stdexcept>

constexpr VkDeviceSize STAGING_BUFFER_ALIGNMENT = 4;
constexpr VkDeviceSize STAGING_BUFFER_SIZE = 1 << 20;

Uploader::Uploader(VkDevice device, VkQueue queue, VmaAllocator allocator, uint32_t queueFamilyIndex)
    :_queue(queue), _finishRequired(false)
{
    r.device = device;
    r.allocator = allocator;

    VkCommandPoolCreateInfo commandPoolCreateInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndex;
    check_success(vkCreateCommandPool(r.device, &commandPoolCreateInfo, nullptr, &d.commandPool));

    VkCommandBufferAllocateInfo commandBufferAllocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    commandBufferAllocateInfo.commandPool = d.commandPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = 1;
    check_success(vkAllocateCommandBuffers(r.device, &commandBufferAllocateInfo, &d.commandBuffer));

    VkFenceCreateInfo fenceCreateInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    check_success(vkCreateFence(r.device, &fenceCreateInfo, nullptr, &d.fence));

    VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferCreateInfo.size = STAGING_BUFFER_SIZE;
    bufferCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo bufferAllocationInfo = {};
    bufferAllocationInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    bufferAllocationInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

    check_success(vmaCreateBuffer(r.allocator, &bufferCreateInfo, &bufferAllocationInfo, &d.stagingBuffer, &d.stagingAllocation, nullptr));
}

Uploader::~Uploader()
{
    if (_finishRequired)
    {
        static_cast<void>(finish());
    }
}

void Uploader::begin()
{
    check_success(vmaMapMemory(r.allocator, d.stagingAllocation, &_pData));
    _currentOffset = 0;

    VkCommandBufferBeginInfo commandBufferBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    check_success(vkBeginCommandBuffer(d.commandBuffer, &commandBufferBeginInfo));
}

void Uploader::end()
{
    check_success(vkEndCommandBuffer(d.commandBuffer));
    vmaUnmapMemory(r.allocator, d.stagingAllocation);

    VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &d.commandBuffer;
    check_success(vkQueueSubmit(_queue, 1, &submitInfo, d.fence));
    _finishRequired = true;
}

VkResult Uploader::finish()
{
    _finishRequired = false;
    return vkWaitForFences(r.device, 1, &d.fence, true, UINT64_MAX);
}

void Uploader::upload(VkBuffer destinationBuffer, VkDeviceSize offset, VkDeviceSize size, const void *pSrc)
{
    if (_currentOffset + size > STAGING_BUFFER_SIZE)
    {
        throw std::runtime_error("Staging buffer out of memory");
    }

    const auto pDst = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(_pData) + _currentOffset);
    memcpy(pDst, pSrc, size);

    VkBufferCopy bufferCopy = { _currentOffset, offset, size };
    vkCmdCopyBuffer(d.commandBuffer, d.stagingBuffer, destinationBuffer, 1, &bufferCopy);

    update_offset(size);
}

void Uploader::upload(VkImage destinationImage, const VkImageSubresourceLayers& subresource, VkExtent3D extent, uint8_t formatSize, const void *pSrc, VkPipelineStageFlags newStageMask, VkImageLayout newLayout, VkAccessFlags newAccess)
{
    const auto size = extent.width * extent.height * extent.depth * formatSize;
    if (_currentOffset + size > STAGING_BUFFER_SIZE)
    {
        throw std::runtime_error("Staging buffer out of memory");
    }

    const auto pDst = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(_pData) + _currentOffset);
    memcpy(pDst, pSrc, size);

    VkImageMemoryBarrier imageMemoryBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.image = destinationImage;
    imageMemoryBarrier.subresourceRange.aspectMask = subresource.aspectMask;
    imageMemoryBarrier.subresourceRange.baseMipLevel = subresource.mipLevel;
    imageMemoryBarrier.subresourceRange.levelCount = 1;
    imageMemoryBarrier.subresourceRange.baseArrayLayer = subresource.baseArrayLayer;
    imageMemoryBarrier.subresourceRange.layerCount = subresource.layerCount;

    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageMemoryBarrier.srcAccessMask = 0;
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(d.commandBuffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &imageMemoryBarrier);

    VkBufferImageCopy bufferImageCopy;
    bufferImageCopy.bufferOffset = _currentOffset;
    bufferImageCopy.bufferRowLength = 0;
    bufferImageCopy.bufferImageHeight = 0;
    bufferImageCopy.imageSubresource = subresource;
    bufferImageCopy.imageOffset = { };
    bufferImageCopy.imageExtent = extent;
    vkCmdCopyBufferToImage(d.commandBuffer, d.stagingBuffer, destinationImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferImageCopy);

    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageMemoryBarrier.newLayout = newLayout;
    imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imageMemoryBarrier.dstAccessMask = newAccess;
    vkCmdPipelineBarrier(d.commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, newStageMask,
        0,
        0, nullptr,
        0, nullptr,
        1, &imageMemoryBarrier);

    update_offset(size);
}

void Uploader::update_offset(VkDeviceSize size)
{
    const auto alignment = size % STAGING_BUFFER_ALIGNMENT;
    if (alignment)
    {
        size += STAGING_BUFFER_ALIGNMENT - alignment;
    }

    _currentOffset += size;
}
