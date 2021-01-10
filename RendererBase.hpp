#pragma once

#include <vk_mem_alloc.h>

#include <array>
#include <vector>

struct PerFrameData
{
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;

    VkFence fence;
    VkSemaphore imageAcquiredSemaphore;
};

struct PerImageData
{
    VkImageView imageView;
    VkFramebuffer framebuffer;

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

    void destroy_swapchain();

    struct {
        VkInstance instance;
        VkSurfaceKHR surface;

        VkPhysicalDevice physicalDevice;
        uint32_t queueFamilyIndex;
        VkSurfaceFormatKHR surfaceFormat;
        VkFormat depthFormat;

        VkDevice device;
        VkQueue queue;

        VmaAllocator allocator;

        VkRenderPass renderPass;

        uint32_t nextFrameIndex;
        std::array<PerFrameData, MAX_FRAMES_IN_FLIGHT> perFrame;

        VkExtent2D swapchainSize;
        VkSwapchainKHR swapchain, oldSwapchain;

        VkImage depthImage;
        VmaAllocation depthMemory;
        VkImageView depthImageView;

        std::vector<PerImageData> perImage;
    } _d;
};
