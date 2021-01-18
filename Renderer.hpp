#pragma once

#include "UIRenderer.hpp"

struct PerFrameData
{
    vk::UniqueCommandPool commandPool;
    vk::CommandBuffer commandBuffer;

    vk::UniqueFence fence;
    vk::UniqueSemaphore semaphore;
};

struct PerImageData
{
    vk::UniqueImageView imageView;
    vk::UniqueFramebuffer framebuffer;

    vk::UniqueSemaphore semaphore;
};

struct Renderer
{
public:
    using RequiredExtensionsCallback = const char **(uint32_t *pCount);
    using SurfaceCreationCallback = VkResult(VkInstance instance, VkAllocationCallbacks *allocator, VkSurfaceKHR *pSurface);

    static constexpr size_t MAX_FRAMES_IN_FLIGHT = 2;

public:
    Renderer(std::function<RequiredExtensionsCallback> requiredExtensionsCallback, std::function<SurfaceCreationCallback> surfaceCreationCallback);
    Renderer(const Renderer&) = delete;
    Renderer(Renderer&&) noexcept = default;
    ~Renderer();

    Renderer& operator=(const Renderer&) = delete;
    Renderer& operator=(Renderer&&) noexcept = default;

    void render();

private:
    void build_swapchain();
    void rebuild_swapchain();
    void record_command_buffer(const PerFrameData& perFrame, const PerImageData& perImage);
    void wait_all_fences() const;
private:
    vk::UniqueInstance instance;
    vk::UniqueSurfaceKHR surface;

    vk::PhysicalDevice physicalDevice;
    uint32_t queueFamilyIndex;

    vk::UniqueDevice device;
    vk::Queue queue;

    vma::Allocator allocator;

    vk::SurfaceFormatKHR surfaceFormat;
    vk::UniqueRenderPass renderPass;

    UIRenderer uiRenderer;

    std::array<PerFrameData, MAX_FRAMES_IN_FLIGHT> perFrameData;

    vk::Extent2D swapchainExtent;
    vk::UniqueSwapchainKHR swapchain, oldSwapchain;
    vk::UniqueImage depthImage;
    vma::Allocation depthMemory;
    vk::UniqueImageView depthImageView;
    std::vector<PerImageData> perImageData;

    uint32_t frameIndex;
};
