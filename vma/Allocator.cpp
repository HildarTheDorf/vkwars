#include "Allocator.hpp"

namespace vma
{

Allocator::Allocator()
    :handle(nullptr)
{
    
}

Allocator::~Allocator()
{
    vmaDestroyAllocator(handle);
}

std::pair<vk::UniqueBuffer, Allocation> Allocator::createBuffer(const VkBufferCreateInfo& bufferCreateInfo, VmaMemoryUsage memoryUsage, VmaAllocationCreateFlags flags)
{
    VmaAllocatorInfo allocatorInfo;
    vmaGetAllocatorInfo(handle, &allocatorInfo);

    VmaAllocationCreateInfo bufferAllocationInfo = { };
    bufferAllocationInfo.flags = flags;
    bufferAllocationInfo.usage = memoryUsage;

    VkBuffer buffer;
    VmaAllocation raw;

    const auto result = vmaCreateBuffer(handle, &bufferCreateInfo, &bufferAllocationInfo, &buffer, &raw, nullptr);
    if (result)
    {
        vk::throwResultException(vk::Result(result), "Allocator::createBuffer");
    }
    return {vk::UniqueBuffer{buffer, {allocatorInfo.device}}, Allocation{handle, raw}};
}

std::pair<vk::UniqueImage, Allocation> Allocator::createImage(const VkImageCreateInfo& imageCreateInfo, VmaMemoryUsage memoryUsage, VmaAllocationCreateFlags flags)
{
    VmaAllocatorInfo allocatorInfo;
    vmaGetAllocatorInfo(handle, &allocatorInfo);

    VmaAllocationCreateInfo imageAllocationInfo = { };
    imageAllocationInfo.flags = flags;
    imageAllocationInfo.usage = memoryUsage;

    VkImage image;
    VmaAllocation raw;
    const auto result = vmaCreateImage(handle, &imageCreateInfo, &imageAllocationInfo, &image, &raw, nullptr);
    if (result)
    {
        vk::throwResultException(vk::Result(result), "Allocator::createImage");
    }
    return {vk::UniqueImage{image, {allocatorInfo.device}}, Allocation{handle, raw}};
}

vk::Result Allocator::init(vk::Instance instance, vk::PhysicalDevice physicalDevice, vk::Device device, uint32_t apiVersion)
{
    VmaAllocatorCreateInfo allocatorCreateInfo = { };
    allocatorCreateInfo.physicalDevice = physicalDevice;
    allocatorCreateInfo.device = device;
    allocatorCreateInfo.instance = instance;
    allocatorCreateInfo.vulkanApiVersion = apiVersion;

    return vk::Result(vmaCreateAllocator(&allocatorCreateInfo, &handle));
}

}
