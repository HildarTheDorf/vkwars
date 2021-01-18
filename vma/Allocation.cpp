#include "Allocation.hpp"

namespace vma
{

Allocation::Allocation()
    :parent(nullptr), handle(nullptr)
{

}

Allocation::Allocation(VmaAllocator parent, VmaAllocation handle)
    :parent(parent), handle(handle)
{

}

Allocation::Allocation(Allocation&& other)
    :parent(other.parent), handle(other.handle)
{
    other.parent = nullptr;
    other.handle = nullptr;
}

Allocation::~Allocation()
{
    if (parent)
    {
        vmaFreeMemory(parent, handle);
    }
}

Allocation& Allocation::operator=(Allocation&& other)
{
    if (parent)
    {
        vmaFreeMemory(parent, handle);
    }

    parent = other.parent;
    other.parent = nullptr;

    handle = other.handle;
    other.handle = nullptr;

    return *this;
}

vk::Result Allocation::flush(VkDeviceSize offset, VkDeviceSize size)
{
    return vk::Result(vmaFlushAllocation(parent, handle, offset, size));
}

vk::Result Allocation::withMap(std::function<void(void*)> func, VkDeviceSize offset)
{
    void *pData;
    const auto ret = vmaMapMemory(parent, handle, &pData);
    if (!ret)
    {
        pData = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(pData) + offset);
        func(pData);
        vmaUnmapMemory(parent, handle);
    }
    return vk::Result(ret);
}

}
