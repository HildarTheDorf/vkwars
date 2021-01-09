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
        for (const auto& perImage : _d.perImage)
        {
            vkDestroySemaphore(_d.device, perImage.renderCompleteSemaphore, nullptr);
            vkDestroyFramebuffer(_d.device, perImage.framebuffer, nullptr);
            vkDestroyImageView(_d.device, perImage.imageView, nullptr);
        }
        vkDestroySwapchainKHR(_d.device, _d.swapchain, nullptr);

        for (const auto& perFrame : _d.perFrame)
        {
            vkDestroySemaphore(_d.device, perFrame.imageAcquiredSemaphore, nullptr);
            vkDestroyFence(_d.device, perFrame.fence, nullptr);

            vkDestroyCommandPool(_d.device, perFrame.commandPool, nullptr);
        }

        vkDestroyRenderPass(_d.device, _d.renderPass, nullptr);

        vkDestroyDevice(_d.device, nullptr);
    }
    if (_d.instance)
    {
        vkDestroySurfaceKHR(_d.instance, _d.surface, nullptr);
        vkDestroyInstance(_d.instance, nullptr);
    }
}