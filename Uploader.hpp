#pragma once

#include "UploaderBase.hpp"

class Uploader : protected UploaderBase
{
public:
    Uploader(VkDevice device, VkQueue queue, VmaAllocator allocator, uint32_t queueFamilyIndex);
    ~Uploader();

    void begin();
    void end();
    VkResult finish();
    void upload(VkBuffer destinationBuffer, VkDeviceSize offset, VkDeviceSize size, const void *pSrc);
    void upload(VkImage destinationImage, const VkImageSubresourceLayers& subresource, VkExtent3D extent, VkDeviceSize size, const void *pSrc, VkPipelineStageFlags newStageMask, VkImageLayout newLayout, VkAccessFlags newAccess);

    // TODO: Delete me
    VkCommandBuffer get_command_buffer() { return d.commandBuffer; }

private:
    void update_offset(VkDeviceSize size);

    VkQueue _queue;

    void *_pData;
    VkDeviceSize _currentOffset;
    bool _finishRequired;
};
