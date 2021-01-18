#pragma once

#include "Allocation.hpp"

namespace vma
{

class Allocator
{
public:
    Allocator();
    Allocator(const Allocator&) = delete;
    Allocator(Allocator&&);
    ~Allocator();

    Allocator& operator=(const Allocator&) = delete;
    Allocator& operator=(Allocator&&);

    std::pair<vk::UniqueBuffer, Allocation> createBuffer(const VkBufferCreateInfo& bufferCreateInfo, VmaMemoryUsage memoryUsage, VmaAllocationCreateFlags flags = 0);
    std::pair<vk::UniqueImage, Allocation> createImage(const VkImageCreateInfo& imageCreateInfo, VmaMemoryUsage memoryUsage, VmaAllocationCreateFlags flags = 0);
    vk::Result init(vk::Instance instance, vk::PhysicalDevice physicalDevice, vk::Device device, uint32_t apiVersion);

private:
    VmaAllocator handle;
};

}
