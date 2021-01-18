#pragma once

#include "vk_mem_alloc.h"

#include <vulkan/vulkan.hpp>

#include <functional>

namespace vma
{

class Allocation
{
public:
    Allocation();
    Allocation(VmaAllocator parent, VmaAllocation handle);
    Allocation(const Allocation&) = delete;
    Allocation(Allocation&&);
    ~Allocation();

    Allocation& operator=(const Allocation&) = delete;
    Allocation& operator=(Allocation&&);

    vk::Result flush(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
    vk::Result  withMap(std::function<void(void*)> func, VkDeviceSize offset = 0);

private:
    VmaAllocator parent;
    VmaAllocation handle;
};

}