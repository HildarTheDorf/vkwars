#include "Renderer.hpp"

#include "RendererUtil.hpp"
#include "Uploader.hpp"

constexpr auto DEPTH_FORMAT = vk::Format::eD16Unorm;
constexpr uint32_t DESIRED_API_VERSION = VK_API_VERSION_1_2;
constexpr auto DESIRED_COMPOSITE_ALPHA = std::array{ vk::CompositeAlphaFlagBitsKHR::eOpaque, vk::CompositeAlphaFlagBitsKHR::eInherit };
constexpr auto DESIRED_PRESENT_MODES = std::array{ vk::PresentModeKHR::eMailbox, vk::PresentModeKHR::eFifoRelaxed, vk::PresentModeKHR::eFifo };
constexpr uint32_t DEFAULT_IMAGE_COUNT = 3;

static constexpr uint32_t compute_image_count(uint32_t min, uint32_t max)
{
    const auto ret = std::max(DEFAULT_IMAGE_COUNT, min);
    return max ? std::min(ret, max) : ret;
}

static std::pair<vk::PhysicalDevice, uint32_t> select_device_and_queue(const std::vector<vk::PhysicalDevice>& physicalDevices, vk::SurfaceKHR surface)
{
    for (const auto physicalDevice : physicalDevices)
    {
        const auto queueFamilies = physicalDevice.getQueueFamilyProperties();
        for (uint32_t i = 0; i < queueFamilies.size(); ++i)
        {
            if (queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics && physicalDevice.getSurfaceSupportKHR(i, surface))
            {
                return {physicalDevice, i};
            }
        }
    }

    throw std::runtime_error("No supported vulkan device");
}

static vk::CompositeAlphaFlagBitsKHR select_composite_alpha(vk::CompositeAlphaFlagsKHR compositeAlpha)
{
    for (const auto needle : DESIRED_COMPOSITE_ALPHA)
    {
        if (compositeAlpha & needle)
        {
            return needle;
        }
    }
    
    throw std::runtime_error("No supported composite alpha");
}

template<typename T>
vk::PresentModeKHR select_present_mode(const T begin, const T end)
{
    for (const auto needle : DESIRED_PRESENT_MODES)
    {
        if (std::find(begin, end, needle) != end)
        {
            return needle;
        }
    }

    throw std::runtime_error("No supported vulkan present mode");
}

template<typename T>
constexpr vk::SurfaceFormatKHR select_surface_format(const T begin, const T end)
{
    for (auto iter = begin; iter != end; ++iter)
    {
        switch (iter->format)
        {
        case vk::Format::eR8G8B8A8Srgb:
        case vk::Format::eB8G8R8A8Srgb:
        case vk::Format::eA8B8G8R8SrgbPack32:
            return *iter;
        default:
            break;
        }
    }

    return *begin;
}

Renderer::Renderer(std::function<RequiredExtensionsCallback> requiredExtensionsCallback, std::function<SurfaceCreationCallback> surfaceCreationCallback)
    :frameIndex(0)
{
    const auto applicationInfo = vk::ApplicationInfo()
        .setApiVersion(DESIRED_API_VERSION);
    auto instanceCreateInfo = vk::InstanceCreateInfo()
        .setPApplicationInfo(&applicationInfo);
    instanceCreateInfo.setPpEnabledExtensionNames(requiredExtensionsCallback(&instanceCreateInfo.enabledExtensionCount));

    instance = vk::createInstanceUnique(instanceCreateInfo);

    VkSurfaceKHR rawSurface;
    check_success(surfaceCreationCallback(instance.get(), nullptr, &rawSurface));
    surface = vk::UniqueSurfaceKHR(rawSurface, instance.get());

    const auto physicalDevices = instance->enumeratePhysicalDevices();
    std::tie(physicalDevice, queueFamilyIndex) = select_device_and_queue(physicalDevices, surface.get());

    const auto queuePriorities = std::array{ 0.0f };

    const auto deviceExtensions = std::array{
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    const auto deviceQueueCreateInfos = std::array{
        vk::DeviceQueueCreateInfo()
            .setQueueFamilyIndex(queueFamilyIndex)
            .setQueuePriorities(queuePriorities)
    };

    const auto deviceCreateInfo = vk::DeviceCreateInfo()
        .setPEnabledExtensionNames(deviceExtensions)
        .setQueueCreateInfos(deviceQueueCreateInfos);

    device = physicalDevice.createDeviceUnique(deviceCreateInfo);
    queue = device->getQueue(queueFamilyIndex, 0);

    check_success(allocator.init(instance.get(), physicalDevice, device.get(), DESIRED_API_VERSION));

    const auto surfaceFormats = physicalDevice.getSurfaceFormatsKHR(surface.get());
    surfaceFormat = select_surface_format(surfaceFormats.begin(), surfaceFormats.end());

    const auto renderPassAttachments = std::array{
        vk::AttachmentDescription()
            .setFormat(surfaceFormat.format)
            .setSamples(vk::SampleCountFlagBits::e1)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore)
            .setInitialLayout(vk::ImageLayout::eUndefined)
            .setFinalLayout(vk::ImageLayout::ePresentSrcKHR),
        vk::AttachmentDescription()
            .setFormat(DEPTH_FORMAT)
            .setSamples(vk::SampleCountFlagBits::e1)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eDontCare)
            .setInitialLayout(vk::ImageLayout::eUndefined)
            .setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
    };

    const auto subpassColorAttachments = std::array{
        vk::AttachmentReference()
            .setAttachment(0)
            .setLayout(vk::ImageLayout::eColorAttachmentOptimal)
    };

    const auto depthStencilAttachment = vk::AttachmentReference()
        .setAttachment(1)
        .setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

    const auto renderPassSubpasses = std::array{
        vk::SubpassDescription()
            .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
            .setColorAttachments(subpassColorAttachments)
            .setPDepthStencilAttachment(&depthStencilAttachment),
        vk::SubpassDescription()
            .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
            .setColorAttachments(subpassColorAttachments)
    };

    const auto renderPassDependencies = std::array{
        vk::SubpassDependency()
            .setSrcSubpass(VK_SUBPASS_EXTERNAL)
            .setDstSubpass(0)
            .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
            .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
            .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
            .setDependencyFlags(vk::DependencyFlagBits::eByRegion),
        vk::SubpassDependency()
            .setSrcSubpass(VK_SUBPASS_EXTERNAL)
            .setDstSubpass(0)
            .setSrcStageMask(vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests)
            .setDstStageMask(vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests)
            .setDstAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite)
            .setDependencyFlags(vk::DependencyFlagBits::eByRegion),
        vk::SubpassDependency()
            .setSrcSubpass(0)
            .setDstSubpass(1)
            .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
            .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
            .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
            .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite)
            .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
    };

    const auto renderPassCreateInfo = vk::RenderPassCreateInfo()
        .setAttachments(renderPassAttachments)
        .setSubpasses(renderPassSubpasses)
        .setDependencies(renderPassDependencies);
    renderPass = device->createRenderPassUnique(renderPassCreateInfo);

    Uploader uploader(device.get(), queueFamilyIndex, 0, allocator);

    uploader.begin();

    uiRenderer.init(device.get(), allocator, uploader, renderPass.get(), 1);

    uploader.end();

    for (auto& perFrame : perFrameData)
    {
        const auto commandPoolCreateInfo = vk::CommandPoolCreateInfo()
            .setFlags(vk::CommandPoolCreateFlagBits::eTransient)
            .setQueueFamilyIndex(queueFamilyIndex);
        perFrame.commandPool = device->createCommandPoolUnique(commandPoolCreateInfo);

        const auto commandBufferAllocateInfo = vk::CommandBufferAllocateInfo()
            .setCommandPool(perFrame.commandPool.get())
            .setLevel(vk::CommandBufferLevel::ePrimary)
            .setCommandBufferCount(1);
        const auto commandBuffers = device->allocateCommandBuffers(commandBufferAllocateInfo);
        perFrame.commandBuffer = commandBuffers[0];

        const auto fenceCreateInfo = vk::FenceCreateInfo()
            .setFlags(vk::FenceCreateFlagBits::eSignaled);
        perFrame.fence = device->createFenceUnique(fenceCreateInfo);

        const auto semaphoreCreateInfo = vk::SemaphoreCreateInfo();
        perFrame.semaphore = device->createSemaphoreUnique(semaphoreCreateInfo);
    }

    build_swapchain();

    check_success(uploader.finish());
}

Renderer::~Renderer()
{
    wait_all_fences();
}

void Renderer::render()
{
    const auto& perFrame = perFrameData[frameIndex++];
    frameIndex %= perFrameData.size();

    check_success(device->waitForFences(perFrame.fence.get(), true, UINT64_MAX));

    uint32_t imageIndex;
    const auto imageIndexResult = device->acquireNextImageKHR(swapchain.get(), UINT64_MAX, perFrame.semaphore.get(), nullptr, &imageIndex);

    bool swapchainUsable, rebuildRequired;
    switch (imageIndexResult)
    {
    case vk::Result::eSuccess:
        swapchainUsable = true;
        rebuildRequired = false;
        break;
    case vk::Result::eSuboptimalKHR:
        swapchainUsable = true;
        rebuildRequired = true;
        break;
    case vk::Result::eErrorOutOfDateKHR:
        swapchainUsable = false;
        rebuildRequired = true;
        break;
    default:
        vk::throwResultException(imageIndexResult, "render");
    }
    if (swapchainUsable)
    {
        const auto& perImage = perImageData[imageIndex];

        device->resetCommandPool(perFrame.commandPool.get());
        record_command_buffer(perFrame, perImage);

        const auto waitSemaphores = std::array{ perFrame.semaphore.get()};
        const auto waitStages = std::array{ vk::PipelineStageFlags(vk::PipelineStageFlagBits::eColorAttachmentOutput) };
        const auto commandBuffers = std::array{ perFrame.commandBuffer };
        const auto renderCompleteSemaphores = std::array{ perImage.semaphore.get()};

        const auto submitInfo = vk::SubmitInfo()
            .setWaitSemaphores(waitSemaphores)
            .setWaitDstStageMask(waitStages)
            .setCommandBuffers(commandBuffers)
            .setSignalSemaphores(renderCompleteSemaphores);

        device->resetFences(perFrame.fence.get());
        queue.submit(submitInfo, perFrame.fence.get());

        const auto imageIndices = std::array{ imageIndex };
        const auto presentInfo = vk::PresentInfoKHR()
            .setWaitSemaphores(renderCompleteSemaphores)
            .setSwapchains(swapchain.get())
            .setImageIndices(imageIndices);
        // Passing a pointer (not a reference) here disables the
        // enhanced version of this method which throws on OutOfDate
        const auto presentResult = queue.presentKHR(&presentInfo);

        switch (presentResult)
        {
        case vk::Result::eSuccess:
            break;
        case vk::Result::eSuboptimalKHR:
        case vk::Result::eErrorOutOfDateKHR:
            rebuildRequired = true;
            break;
        default:
            vk::throwResultException(presentResult, "render");
        }
    }

    if (rebuildRequired)
    {
        rebuild_swapchain();
    }
}

void Renderer::build_swapchain()
{
    const auto surfaceCaps = physicalDevice.getSurfaceCapabilitiesKHR(surface.get());
    const auto compositeAlpha = select_composite_alpha(surfaceCaps.supportedCompositeAlpha);
    const auto minImageCount = compute_image_count(surfaceCaps.minImageCount, surfaceCaps.minImageCount);
    swapchainExtent = surfaceCaps.currentExtent;

    const auto presentModes = physicalDevice.getSurfacePresentModesKHR(surface.get());
    const auto presentMode = select_present_mode(presentModes.begin(), presentModes.end());

    const auto swapchainCreateInfo = vk::SwapchainCreateInfoKHR()
        .setSurface(surface.get())
        .setMinImageCount(minImageCount)
        .setImageFormat(surfaceFormat.format)
        .setImageColorSpace(surfaceFormat.colorSpace)
        .setImageExtent(swapchainExtent)
        .setImageArrayLayers(1)
        .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment)
        .setPreTransform(surfaceCaps.currentTransform)
        .setCompositeAlpha(compositeAlpha)
        .setPresentMode(presentMode)
        .setClipped(true)
        .setOldSwapchain(oldSwapchain.get());

    swapchain = device->createSwapchainKHRUnique(swapchainCreateInfo);
    const auto swapchainImages = device->getSwapchainImagesKHR(swapchain.get());

    const auto depthImageCreateInfo = vk::ImageCreateInfo()
        .setImageType(vk::ImageType::e2D)
        .setFormat(DEPTH_FORMAT)
        .setExtent({swapchainExtent.width, swapchainExtent.height, 1})
        .setMipLevels(1)
        .setArrayLayers(1)
        .setSamples(vk::SampleCountFlagBits::e1)
        .setTiling(vk::ImageTiling::eOptimal)
        .setUsage(vk::ImageUsageFlagBits::eDepthStencilAttachment);

    std::tie(depthImage, depthMemory) = allocator.createImage(depthImageCreateInfo, VMA_MEMORY_USAGE_GPU_ONLY);

    const auto depthImageViewCreateInfo = vk::ImageViewCreateInfo()
        .setImage(depthImage.get())
        .setViewType(vk::ImageViewType::e2D)
        .setFormat(DEPTH_FORMAT)
        .setSubresourceRange({ vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 });
    depthImageView = device->createImageViewUnique(depthImageViewCreateInfo);

    perImageData.resize(swapchainImages.size());
    for (uint32_t i = 0; i < swapchainImages.size(); ++i)
    {
        auto& perImage = perImageData[i];

        const auto imageViewCreateInfo = vk::ImageViewCreateInfo()
            .setImage(swapchainImages[i])
            .setViewType(vk::ImageViewType::e2D)
            .setFormat(surfaceFormat.format)
            .setSubresourceRange({ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
        perImage.imageView = device->createImageViewUnique(imageViewCreateInfo);

        const auto framebufferAttachments = std::array{ perImage.imageView.get(), depthImageView.get() };

        const auto framebufferCreateInfo = vk::FramebufferCreateInfo()
            .setRenderPass(renderPass.get())
            .setAttachments(framebufferAttachments)
            .setWidth(swapchainExtent.width)
            .setHeight(swapchainExtent.height)
            .setLayers(1);
        perImage.framebuffer = device->createFramebufferUnique(framebufferCreateInfo);

        const auto semaphoreCreateInfo = vk::SemaphoreCreateInfo();
        perImage.semaphore = device->createSemaphoreUnique(semaphoreCreateInfo);
    }
}

void Renderer::rebuild_swapchain()
{
    wait_all_fences();
    oldSwapchain = std::move(swapchain);
    build_swapchain();
}

void Renderer::record_command_buffer(const PerFrameData& perFrame, const PerImageData& perImage)
{
    const auto cb = perFrame.commandBuffer;

    const auto cbBeginInfo = vk::CommandBufferBeginInfo()
        .setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    const auto clearValues = std::array{
        vk::ClearValue(std::array{0.0f, 0.0f, 0.0f, 1.0f}),
        vk::ClearValue(vk::ClearDepthStencilValue(1.0f))
    };

    const auto rpBeginInfo = vk::RenderPassBeginInfo()
        .setRenderPass(renderPass.get())
        .setFramebuffer(perImage.framebuffer.get())
        .setRenderArea({{}, swapchainExtent})
        .setClearValues(clearValues);

    const auto viewport = vk::Viewport{
        0.0f, 0.0f,
        static_cast<float>(swapchainExtent.width), static_cast<float>(swapchainExtent.height),
        0.0f, 1.0
    };

    cb.begin(cbBeginInfo);
    cb.beginRenderPass(rpBeginInfo, vk::SubpassContents::eInline);
    cb.setViewport(0, viewport);

    cb.nextSubpass(vk::SubpassContents::eInline);

    uiRenderer.render(cb, swapchainExtent);

    cb.endRenderPass();
    cb.end();
}

void Renderer::wait_all_fences() const
{
    std::vector<vk::Fence> allFences;
    for (const auto& perFrame : perFrameData)
    {
        allFences.emplace_back(perFrame.fence.get());
    }
    check_success(device->waitForFences(allFences, true, UINT64_MAX));
}
