#pragma once

#include "vma/Allocator.hpp"

class Uploader
{
public:
    Uploader(vk::Device device, uint32_t queueFamilyIndex, uint32_t queueIndex, vma::Allocator& allocater);
    //FIXME: Rule of 5
    ~Uploader();

    void begin();
    void end();
    vk::Result finish();

    void clearImage(vk::Image image, vk::ImageSubresourceRange subresourceRange, vk::ClearColorValue clearColor, vk::ImageLayout newLayout, vk::AccessFlags newAccess, vk::PipelineStageFlags newStage);
    void uploadImage(vk::Image image, vk::ImageSubresourceLayers subresourceLayers, vk::Extent3D imageExtent, void *pData, vk::ImageLayout newLayout, vk::AccessFlags newAccess, vk::PipelineStageFlags newStage);

private:
    vk::Device device;
    vk::Queue queue;

    vma::Allocation stagingMemory;
    vk::UniqueBuffer stagingBuffer;

    vk::UniqueCommandPool commandPool;
    vk::CommandBuffer commandBuffer;
    vk::UniqueFence fence;

    VkDeviceSize currentOffset;
    bool uploadInProgress;
};
