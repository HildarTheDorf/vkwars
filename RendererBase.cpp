#include "RendererBase.hpp"

RendererBase::RendererBase()
    :_d{}
{

}

RendererBase::~RendererBase()
{
    if (_d.device)
    {
        vkDeviceWaitIdle(_d.device);
        
        destroy_swapchain();
        vkDestroySwapchainKHR(_d.device, _d.oldSwapchain, nullptr);

        for (const auto& perFrame : _d.perFrame)
        {
            vkDestroySemaphore(_d.device, perFrame.imageAcquiredSemaphore, nullptr);
            vkDestroyFence(_d.device, perFrame.fence, nullptr);

            vkDestroyCommandPool(_d.device, perFrame.commandPool, nullptr);
        }

        vkDestroyDescriptorPool(_d.device, _d.uiDescriptorPool, nullptr);

        vkDestroyRenderPass(_d.device, _d.renderPass, nullptr);

        vmaDestroyAllocator(_d.allocator);
        vkDestroyDevice(_d.device, nullptr);
    }
    if (_d.instance)
    {
        vkDestroySurfaceKHR(_d.instance, _d.surface, nullptr);
        vkDestroyInstance(_d.instance, nullptr);
    }
}

void RendererBase::destroy_swapchain()
{
    for (const auto& perImage : _d.perImage)
    {
        vkDestroySemaphore(_d.device, perImage.renderCompleteSemaphore, nullptr);
        vkDestroyFramebuffer(_d.device, perImage.framebuffer, nullptr);
        vkDestroyImageView(_d.device, perImage.imageView, nullptr);
    }
    _d.perImage.clear();

    vkDestroyImageView(_d.device, _d.depthImageView, nullptr);
    _d.depthImageView = nullptr;
    vmaDestroyImage(_d.allocator, _d.depthImage, _d.depthMemory);
    _d.depthImage = nullptr;
    _d.depthMemory = nullptr;

    vkDestroySwapchainKHR(_d.device, _d.oldSwapchain, nullptr);
    _d.oldSwapchain = _d.swapchain;
    _d.swapchain = nullptr;
}
