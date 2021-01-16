#include "RendererBase.hpp"

RendererBase::RendererBase()
    :d{}
{

}

RendererBase::~RendererBase()
{
    if (d.device)
    {
        vkDeviceWaitIdle(d.device);

        destroy_swapchain();
        vkDestroySwapchainKHR(d.device, d.oldSwapchain, nullptr);

        for (const auto& perFrame : d.perFrame)
        {
            vkDestroySemaphore(d.device, perFrame.imageAcquiredSemaphore, nullptr);
            vkDestroyFence(d.device, perFrame.fence, nullptr);

            vkDestroyCommandPool(d.device, perFrame.commandPool, nullptr);
        }

        vkDestroyPipeline(d.device, d.uiPipeline, nullptr);
        vkDestroyRenderPass(d.device, d.renderPass, nullptr);

        vkDestroyPipelineCache(d.device, d.pipelineCache, nullptr);

        vkDestroyDescriptorPool(d.device, d.uiDescriptorPool, nullptr);
        vkDestroyPipelineLayout(d.device, d.uiPipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(d.device, d.uiDescriptorSetLayout, nullptr);

        vmaDestroyImage(d.allocator, d.fontImage, d.fontMemory);

        vmaDestroyAllocator(d.allocator);
        vkDestroyDevice(d.device, nullptr);
    }
    if (d.instance)
    {
        vkDestroySurfaceKHR(d.instance, d.surface, nullptr);
        vkDestroyInstance(d.instance, nullptr);
    }
}

void RendererBase::destroy_swapchain()
{
    for (const auto& perImage : d.perImage)
    {
        vkDestroySemaphore(d.device, perImage.renderCompleteSemaphore, nullptr);
        vkDestroyFramebuffer(d.device, perImage.framebuffer, nullptr);
        vkDestroyImageView(d.device, perImage.imageView, nullptr);
    }
    d.perImage.clear();

    vkDestroyImageView(d.device, d.depthImageView, nullptr);
    d.depthImageView = nullptr;
    vmaDestroyImage(d.allocator, d.depthImage, d.depthMemory);
    d.depthImage = nullptr;
    d.depthMemory = nullptr;

    vkDestroySwapchainKHR(d.device, d.oldSwapchain, nullptr);
    d.oldSwapchain = d.swapchain;
    d.swapchain = nullptr;
}
