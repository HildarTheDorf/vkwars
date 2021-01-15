#include "UploaderBase.hpp"

UploaderBase::UploaderBase()
    :r{}, d{}
{
    
}

UploaderBase::~UploaderBase()
{
    if (r.allocator)
    {
        vmaDestroyBuffer(r.allocator, d.stagingBuffer, d.stagingAllocation);
    }

    if (r.device)
    {
        vkDestroyFence(r.device, d.fence, nullptr);
        vkDestroyCommandPool(r.device, d.commandPool, nullptr);
    }
}
