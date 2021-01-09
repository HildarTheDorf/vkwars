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

        }
        vkDestroySwapchainKHR(_d.device, _d.swapchain, nullptr);

        for (const auto& perFrame : _d.perFrame)
        {
            vkDestroySemaphore(_d.device, perFrame.imageAcquiredSemaphore, nullptr);
            vkDestroyFence(_d.device, perFrame.fence, nullptr);
        }

        vkDestroyDevice(_d.device, nullptr);
    }
    if (_d.instance)
    {
        vkDestroySurfaceKHR(_d.instance, _d.surface, nullptr);
        vkDestroyInstance(_d.instance, nullptr);
    }
}