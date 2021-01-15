#pragma once

#include <vk_mem_alloc.h>

class UploaderBase
{
protected:
    UploaderBase();
    UploaderBase(const UploaderBase&) = delete;
    UploaderBase(UploaderBase&&);
    ~UploaderBase();

    UploaderBase& operator=(const UploaderBase&) = delete;
    UploaderBase& operator=(UploaderBase&&);

    struct {
        VkDevice device;
        VmaAllocator allocator;    
    } r;

    struct {
        VkCommandPool commandPool;
        VkCommandBuffer commandBuffer;
        VkFence fence;

        VmaAllocation stagingAllocation;
        VkBuffer stagingBuffer;
    } d;
};
