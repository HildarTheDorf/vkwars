#include "Renderer.hpp"

#include "Window.hpp"

#include <algorithm>
#include <stdexcept>

constexpr uint32_t DESIRED_API_VERSION = VK_API_VERSION_1_1;
constexpr auto DESIRED_DEPTH_FORMATS = std::array{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_X8_D24_UNORM_PACK32 };
constexpr auto DESIRED_PRESENT_MODES = std::array{ VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_FIFO_KHR };
constexpr uint32_t DESIRED_SWAPCHAIN_IMAGES = 3;

constexpr void check_success(VkResult result)
{
    if (result)
    {
        throw std::runtime_error("Bad VkResult");
    }
}

constexpr uint32_t compute_image_count(uint32_t min, uint32_t max)
{
    uint32_t ret = std::max(min + 1, DESIRED_SWAPCHAIN_IMAGES);
    return max ? std::min(ret, max) : ret;
}

static VkFormat select_depth_format(VkPhysicalDevice physicalDevice)
{
    for (const auto desiredFormat : DESIRED_DEPTH_FORMATS)
    {
        VkFormatProperties formatProps;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, desiredFormat, &formatProps);
        if (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            return desiredFormat;
        }
    }

    throw std::runtime_error("No supported depth format");
}

static std::pair<VkPhysicalDevice, uint32_t> select_device_and_queue(VkInstance instance, VkSurfaceKHR surface)
{
    uint32_t numPhysicalDevices;
    check_success(vkEnumeratePhysicalDevices(instance, &numPhysicalDevices, nullptr));
    std::vector<VkPhysicalDevice> physicalDevices(numPhysicalDevices);
    check_success(vkEnumeratePhysicalDevices(instance, &numPhysicalDevices, physicalDevices.data()));

    for (const auto physicalDevice : physicalDevices)
    {
        uint32_t numQueueFamilies;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &numQueueFamilies, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilyProperties(numQueueFamilies);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &numQueueFamilies, queueFamilyProperties.data());

        for (uint32_t i = 0; i < numQueueFamilies; ++i)
        {
            if (queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                VkBool32 supported;
                check_success(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &supported));
                if (supported)
                {
                    return { physicalDevice, i };
                }
            }
        }
    }

    throw std::runtime_error("No supported Vulkan Device");
}

static VkPresentModeKHR select_present_mode(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
    uint32_t numPresentModes;
    check_success(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &numPresentModes, nullptr));
    std::vector<VkPresentModeKHR> presentModes(numPresentModes);
    check_success(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &numPresentModes, presentModes.data()));

    for (const auto needle : DESIRED_PRESENT_MODES)
    {
        if (std::find(presentModes.begin(), presentModes.end(), needle) != presentModes.end())
        {
            return needle;
        }
    }

    // Unreachable, FIFO should always be supported
    throw std::runtime_error("No supported present mode");
}

static VkCompositeAlphaFlagBitsKHR select_surface_alpha(VkCompositeAlphaFlagsKHR flags)
{
    if (flags & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
    {
        return VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    }
    else if (flags & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
    {
        return VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    }
    else
    {
        throw std::runtime_error("No supported composite alpha");
    }
}

static VkSurfaceFormatKHR select_surface_format(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
    uint32_t numFormats;
    check_success(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &numFormats, nullptr));
    std::vector<VkSurfaceFormatKHR> formats(numFormats);
    check_success(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &numFormats, formats.data()));

    for (const auto& surfaceFormat : formats)
    {
        switch (surfaceFormat.format)
        {
        case VK_FORMAT_R8G8B8_SRGB:
        case VK_FORMAT_R8G8B8A8_SRGB:
        case VK_FORMAT_B8G8R8_SRGB:
        case VK_FORMAT_B8G8R8A8_SRGB:
        case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
            return surfaceFormat;

        default:
            break;
        }
    }

    return formats[0];

}

Renderer::Renderer(Window& window)
{
    VkApplicationInfo applicationInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
    applicationInfo.apiVersion = DESIRED_API_VERSION;

    VkInstanceCreateInfo instanceCreateInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    instanceCreateInfo.pApplicationInfo = &applicationInfo;
    instanceCreateInfo.ppEnabledExtensionNames = window.get_required_vulkan_extensions(&instanceCreateInfo.enabledExtensionCount);
    check_success(vkCreateInstance(&instanceCreateInfo, nullptr, &_d.instance));

    check_success(window.create_vulkan_surface(_d.instance, &_d.surface));

    std::tie(_d.physicalDevice, _d.queueFamilyIndex) = select_device_and_queue(_d.instance, _d.surface);
    _d.surfaceFormat = select_surface_format(_d.physicalDevice, _d.surface);
    _d.depthFormat = select_depth_format(_d.physicalDevice);

    const float queuePriority = 0.0;

    VkDeviceQueueCreateInfo queueCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
    queueCreateInfo.queueFamilyIndex = _d.queueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    std::array<const char *, 1> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkDeviceCreateInfo deviceCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.enabledExtensionCount = deviceExtensions.size();
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

    check_success(vkCreateDevice(_d.physicalDevice, &deviceCreateInfo, nullptr, &_d.device));
    vkGetDeviceQueue(_d.device, _d.queueFamilyIndex, 0, &_d.queue);

    VmaAllocatorCreateInfo allocatorCreateInfo = { };
    allocatorCreateInfo.physicalDevice = _d.physicalDevice;
    allocatorCreateInfo.device = _d.device;
    allocatorCreateInfo.instance = _d.instance;
    allocatorCreateInfo.vulkanApiVersion = DESIRED_API_VERSION;

    check_success(vmaCreateAllocator(&allocatorCreateInfo, &_d.allocator));

    std::array<VkAttachmentDescription, 2> attachmentDescs = { };
    attachmentDescs[0].format = _d.surfaceFormat.format;
    attachmentDescs[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescs[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescs[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescs[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachmentDescs[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    attachmentDescs[1].format = _d.depthFormat;
    attachmentDescs[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescs[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescs[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescs[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachmentDescs[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    VkAttachmentReference colorAttachmentRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkAttachmentReference depthAttachmentRef = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpassDesc = { };
    subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDesc.colorAttachmentCount = 1;
    subpassDesc.pColorAttachments = &colorAttachmentRef;
    subpassDesc.pDepthStencilAttachment = &depthAttachmentRef;

    std::array<VkSubpassDependency, 2> subpassDeps = { };
    subpassDeps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    subpassDeps[0].dstSubpass = 0;
    subpassDeps[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDeps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDeps[0].srcAccessMask = 0;
    subpassDeps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    subpassDeps[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    subpassDeps[1].srcSubpass = VK_SUBPASS_EXTERNAL;
    subpassDeps[1].dstSubpass = 0;
    subpassDeps[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    subpassDeps[1].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    subpassDeps[1].srcAccessMask = 0;
    subpassDeps[1].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    subpassDeps[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo renderPassCreateInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    renderPassCreateInfo.attachmentCount = attachmentDescs.size();
    renderPassCreateInfo.pAttachments = attachmentDescs.data();
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &subpassDesc;
    renderPassCreateInfo.dependencyCount = subpassDeps.size();
    renderPassCreateInfo.pDependencies = subpassDeps.data();
    check_success(vkCreateRenderPass(_d.device, &renderPassCreateInfo, nullptr, &_d.renderPass));

    for (auto& perFrame : _d.perFrame)
    {
        VkCommandPoolCreateInfo commandPoolCreateInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        commandPoolCreateInfo.queueFamilyIndex = _d.queueFamilyIndex;
        check_success(vkCreateCommandPool(_d.device, &commandPoolCreateInfo, nullptr, &perFrame.commandPool));

        VkCommandBufferAllocateInfo commandBufferAllocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        commandBufferAllocateInfo.commandPool = perFrame.commandPool;
        commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commandBufferAllocateInfo.commandBufferCount = 1;
        check_success(vkAllocateCommandBuffers(_d.device, &commandBufferAllocateInfo, &perFrame.commandBuffer));

        VkFenceCreateInfo fenceCreateInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        check_success(vkCreateFence(_d.device, &fenceCreateInfo, nullptr, &perFrame.fence));

        VkSemaphoreCreateInfo semaphoreCreateInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        check_success(vkCreateSemaphore(_d.device, &semaphoreCreateInfo, nullptr, &perFrame.imageAcquiredSemaphore));
    }

    build_swapchain();
}

void Renderer::render()
{
    const auto& perFrame = _d.perFrame[_d.nextFrameIndex++];
    _d.nextFrameIndex %= _d.perFrame.size();

    uint32_t imageIndex;
    const auto acquireResult = vkAcquireNextImageKHR(_d.device, _d.swapchain, UINT64_MAX, perFrame.imageAcquiredSemaphore, nullptr, &imageIndex);

    bool swapchainUsable, rebuildRequired;
    switch (acquireResult)
    {
    case VK_SUCCESS:
        swapchainUsable = true;
        rebuildRequired = false;
        break;
    case VK_SUBOPTIMAL_KHR:
        swapchainUsable = true;
        rebuildRequired = true;
        break;
    case VK_ERROR_OUT_OF_DATE_KHR:
        swapchainUsable = false;
        rebuildRequired = true;
        break;
    default:
        check_success(acquireResult);
        // Unreachable
        std::terminate();
    }
    if (swapchainUsable)
    {
        const auto& perImage = _d.perImage[imageIndex];

        check_success(vkWaitForFences(_d.device, 1, &perFrame.fence, VK_TRUE, UINT64_MAX));

        record_command_buffer(perFrame, perImage);

        const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &perFrame.imageAcquiredSemaphore;
        submitInfo.pWaitDstStageMask = &waitStage;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &perFrame.commandBuffer;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &perImage.renderCompleteSemaphore;

        check_success(vkResetFences(_d.device, 1, &perFrame.fence));
        check_success(vkQueueSubmit(_d.queue, 1, &submitInfo, perFrame.fence));

        VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &perImage.renderCompleteSemaphore;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &_d.swapchain;
        presentInfo.pImageIndices = &imageIndex;
        const auto presentResult = vkQueuePresentKHR(_d.queue, &presentInfo);
        switch (presentResult)
        {
        case VK_SUCCESS:
            break;
        case VK_SUBOPTIMAL_KHR:
        case VK_ERROR_OUT_OF_DATE_KHR:
            rebuildRequired = true;
            break;
        default:
            check_success(presentResult);
            // Unreachable
            std::terminate();
        }
    }

    if (rebuildRequired)
    {
        rebuild_swapchain();
    }
}

void Renderer::build_swapchain()
{
    VkSurfaceCapabilitiesKHR surfaceCaps;
    check_success(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_d.physicalDevice, _d.surface, &surfaceCaps));

    _d.swapchainSize = surfaceCaps.currentExtent;

    VkSwapchainCreateInfoKHR swapchainCreateInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    swapchainCreateInfo.surface = _d.surface;
    swapchainCreateInfo.minImageCount = compute_image_count(surfaceCaps.minImageCount, surfaceCaps.maxImageCount);
    swapchainCreateInfo.imageFormat = _d.surfaceFormat.format;
    swapchainCreateInfo.imageColorSpace = _d.surfaceFormat.colorSpace;
    swapchainCreateInfo.imageExtent = _d.swapchainSize;
    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainCreateInfo.preTransform = surfaceCaps.currentTransform;
    swapchainCreateInfo.compositeAlpha = select_surface_alpha(surfaceCaps.supportedCompositeAlpha);
    swapchainCreateInfo.presentMode = select_present_mode(_d.physicalDevice, _d.surface);
    swapchainCreateInfo.clipped = VK_TRUE;
    swapchainCreateInfo.oldSwapchain = _d.oldSwapchain;
    check_success(vkCreateSwapchainKHR(_d.device, &swapchainCreateInfo, nullptr, &_d.swapchain));

    VkImageCreateInfo depthImageCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    depthImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    depthImageCreateInfo.format = _d.depthFormat;
    depthImageCreateInfo.extent = { _d.swapchainSize.width, _d.swapchainSize.height, 1 };
    depthImageCreateInfo.mipLevels = 1;
    depthImageCreateInfo.arrayLayers = 1;
    depthImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    depthImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    depthImageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;

    VmaAllocationCreateInfo depthImageAllocationInfo = { };
    depthImageAllocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    check_success(vmaCreateImage(_d.allocator, &depthImageCreateInfo, &depthImageAllocationInfo, &_d.depthImage, &_d.depthMemory, nullptr));

    VkImageViewCreateInfo depthImageViewCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    depthImageViewCreateInfo.image = _d.depthImage;
    depthImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    depthImageViewCreateInfo.format = _d.depthFormat;
    depthImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depthImageViewCreateInfo.subresourceRange.layerCount = 1;
    depthImageViewCreateInfo.subresourceRange.levelCount = 1;
    check_success(vkCreateImageView(_d.device, &depthImageViewCreateInfo, nullptr, &_d.depthImageView));

    uint32_t numSwapchainImages;
    check_success(vkGetSwapchainImagesKHR(_d.device, _d.swapchain, &numSwapchainImages, nullptr));
    std::vector<VkImage> swapchainImages(numSwapchainImages);
    check_success(vkGetSwapchainImagesKHR(_d.device, _d.swapchain, &numSwapchainImages, swapchainImages.data()));

    _d.perImage.resize(numSwapchainImages);
    for (uint32_t i = 0; i < numSwapchainImages; ++i)
    {
        auto& perImage = _d.perImage[i];

        VkImageViewCreateInfo imageViewCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        imageViewCreateInfo.image = swapchainImages[i];
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.format = _d.surfaceFormat.format;
        imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewCreateInfo.subresourceRange.layerCount = 1;
        imageViewCreateInfo.subresourceRange.levelCount = 1;
        check_success(vkCreateImageView(_d.device, &imageViewCreateInfo, nullptr, &perImage.imageView));

        std::array<VkImageView, 2> attachments = {};
        attachments[0] = perImage.imageView;
        attachments[1] = _d.depthImageView;

        VkFramebufferCreateInfo framebufferCreateInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        framebufferCreateInfo.renderPass = _d.renderPass;
        framebufferCreateInfo.attachmentCount = attachments.size();
        framebufferCreateInfo.pAttachments = attachments.data();
        framebufferCreateInfo.width = _d.swapchainSize.width;
        framebufferCreateInfo.height = _d.swapchainSize.height;
        framebufferCreateInfo.layers = 1;
        check_success(vkCreateFramebuffer(_d.device, &framebufferCreateInfo, nullptr, &perImage.framebuffer));

        VkSemaphoreCreateInfo semaphoreCreateInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        check_success(vkCreateSemaphore(_d.device, &semaphoreCreateInfo, nullptr, &perImage.renderCompleteSemaphore));
    }
}

void Renderer::rebuild_swapchain()
{
    std::array<VkFence, MAX_FRAMES_IN_FLIGHT> allFences;
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        allFences[i] = _d.perFrame[i].fence;
    }
    check_success(vkWaitForFences(_d.device, allFences.size(), allFences.data(), true, UINT64_MAX));

    destroy_swapchain();
    build_swapchain();
}

void Renderer::record_command_buffer(const PerFrameData& perFrame, const PerImageData& perImage)
{
    check_success(vkResetCommandPool(_d.device, perFrame.commandPool, 0));

    VkCommandBufferBeginInfo commandBufferBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    std::array<VkClearValue, 2> clearValues = { };
    clearValues[0].color.float32[3] = 1.0f;
    clearValues[1].depthStencil.depth = 1.0f;

    VkRenderPassBeginInfo renderPassBeginInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    renderPassBeginInfo.renderPass = _d.renderPass;
    renderPassBeginInfo.renderArea = { {}, _d.swapchainSize };
    renderPassBeginInfo.framebuffer = perImage.framebuffer;
    renderPassBeginInfo.clearValueCount = clearValues.size();
    renderPassBeginInfo.pClearValues = clearValues.data();

    check_success(vkBeginCommandBuffer(perFrame.commandBuffer, &commandBufferBeginInfo));
    vkCmdBeginRenderPass(perFrame.commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdEndRenderPass(perFrame.commandBuffer);
    check_success(vkEndCommandBuffer(perFrame.commandBuffer));
}
