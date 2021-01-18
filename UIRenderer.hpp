#pragma once

#include "Uploader.hpp"

class UIRenderer
{
public:
    void init(vk::Device device, vma::Allocator& allocator, Uploader& uploader, vk::RenderPass renderPass, uint32_t subpass);

    void render(vk::CommandBuffer commandBuffer, vk::Extent2D framebufferExtent);

private:
    vk::UniqueImage fontImage;
    vma::Allocation fontMemory;
    vk::UniqueImageView fontImageView;

    vk::UniqueSampler sampler;
    vk::UniqueDescriptorSetLayout descriptorSetLayout;
    vk::UniquePipelineLayout pipelineLayout;

    vk::UniqueDescriptorPool descriptorPool;
    vk::DescriptorSet descriptorSet;

    vk::UniquePipeline graphicsPipeline;
    vk::UniqueBuffer indexBuffer, vertexBuffer;
    vma::Allocation indexMemory, vertexMemory;
};
