#pragma once

#include <vulkan/vulkan.h>

#include <array>
#include <vector>

struct PerFrameData
{
    VkFence fence;
    VkSemaphore imageAcquiredSemaphore;
};

struct PerImageData
{
    VkSemaphore renderCompleteSemaphore;
};

class RendererBase
{
public:
    static constexpr size_t MAX_FRAMES_IN_FLIGHT = 2;
protected:
    RendererBase();
    RendererBase(const RendererBase&) = delete;
    RendererBase(RendererBase&&);
    ~RendererBase();

    RendererBase& operator=(const RendererBase&) = delete;
    RendererBase& operator=(RendererBase&&);

    struct {
        VkInstance instance;
        VkSurfaceKHR surface;

        VkPhysicalDevice physicalDevice;
        uint32_t queueFamilyIndex;

        VkDevice device;
        VkQueue queue;

        uint32_t frameIndex;
        std::array<PerFrameData, MAX_FRAMES_IN_FLIGHT> perFrame;

        VkSwapchainKHR swapchain;
        std::vector<PerImageData> perImage;
    } _d;
};