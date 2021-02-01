#pragma once

#include "Uploader.hpp"

class UIRenderer
{
public:
    UIRenderer();

    void init(vk::Device device, vma::Allocator& allocator, Uploader& uploader, vk::RenderPass renderPass, uint32_t subpass);

    void render(vk::CommandBuffer commandBuffer, vk::Extent2D framebufferExtent);

private:
    std::pair<vk::UniqueBuffer, vma::Allocation> allocate_buffer(VkDeviceSize size, vk::BufferUsageFlags usage);

private:
    vma::Allocator *pAllocator;

    vk::UniqueImage fontImage;
    vma::Allocation fontMemory;
    vk::UniqueImageView fontImageView;

    vk::UniqueSampler sampler;
    vk::UniqueDescriptorSetLayout descriptorSetLayout;
    vk::UniquePipelineLayout pipelineLayout;

    vk::UniqueDescriptorPool descriptorPool;
    vk::DescriptorSet descriptorSet;

    vk::UniqueBuffer indexBuffer, vertexBuffer;
    vma::Allocation indexMemory, vertexMemory;
    VkDeviceSize indexMemorySize, vertexMemorySize;

    vk::UniquePipeline graphicsPipeline;
};
