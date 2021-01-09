#include "Renderer.hpp"

#include "Window.hpp"

#include <algorithm>
#include <stdexcept>

constexpr auto DESIRED_PRESENT_MODES = std::array{ VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_FIFO_KHR };
constexpr uint32_t DESIRED_SWAPCHAIN_IMAGES = 3;

constexpr void check_success(VkResult result)
{
    if (result)
    {
        throw std::runtime_error("Bad VkResult");
    }
}

uint32_t compute_image_count(uint32_t min, uint32_t max)
{
    uint32_t ret = std::max(min + 1, DESIRED_SWAPCHAIN_IMAGES);
    return max ? std::min(ret, max) : ret;
}

std::pair<VkPhysicalDevice, uint32_t> select_device_and_queue(VkInstance instance, VkSurfaceKHR surface)
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

VkPresentModeKHR select_present_mode(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
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

VkCompositeAlphaFlagBitsKHR select_surface_alpha(VkCompositeAlphaFlagsKHR flags)
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

VkSurfaceFormatKHR select_surface_format(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
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
    applicationInfo.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo instanceCreateInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    instanceCreateInfo.pApplicationInfo = &applicationInfo;
    instanceCreateInfo.ppEnabledExtensionNames = window.get_required_vulkan_extensions(&instanceCreateInfo.enabledExtensionCount);
    check_success(vkCreateInstance(&instanceCreateInfo, nullptr, &_d.instance));

    check_success(window.create_vulkan_surface(_d.instance, &_d.surface));

    std::tie(_d.physicalDevice, _d.queueFamilyIndex) = select_device_and_queue(_d.instance, _d.surface);

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

    const auto surfaceFormat = select_surface_format(_d.physicalDevice, _d.surface);

    VkAttachmentDescription colorAttachmentDesc = { };
    colorAttachmentDesc.format = surfaceFormat.format;
    colorAttachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpassDesc = { };
    subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDesc.colorAttachmentCount = 1;
    subpassDesc.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency subpassDep = { };
    subpassDep.srcSubpass = VK_SUBPASS_EXTERNAL;
    subpassDep.dstSubpass = 0;
    subpassDep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDep.srcAccessMask = 0;
    subpassDep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    subpassDep.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo renderPassCreateInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    renderPassCreateInfo.attachmentCount = 1;
    renderPassCreateInfo.pAttachments = &colorAttachmentDesc;
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &subpassDesc;
    renderPassCreateInfo.dependencyCount = 1;
    renderPassCreateInfo.pDependencies = &subpassDep;
    check_success(vkCreateRenderPass(_d.device, &renderPassCreateInfo, nullptr, &_d.renderPass));

    // Reused during per-image data
    VkSemaphoreCreateInfo semaphoreCreateInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

    for (auto& perFrame : _d.perFrame)
    {
        VkCommandPoolCreateInfo commandPoolCreateInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
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
        check_success(vkCreateSemaphore(_d.device, &semaphoreCreateInfo, nullptr, &perFrame.imageAcquiredSemaphore));
    }

    VkSurfaceCapabilitiesKHR surfaceCaps;
    check_success(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_d.physicalDevice, _d.surface, &surfaceCaps));

    _d.swapchainSize = surfaceCaps.currentExtent;

    VkSwapchainCreateInfoKHR swapchainCreateInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    swapchainCreateInfo.surface = _d.surface;
    swapchainCreateInfo.minImageCount = compute_image_count(surfaceCaps.minImageCount, surfaceCaps.maxImageCount);
    swapchainCreateInfo.imageFormat = surfaceFormat.format;
    swapchainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapchainCreateInfo.imageExtent = _d.swapchainSize;
    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainCreateInfo.preTransform = surfaceCaps.currentTransform;
    swapchainCreateInfo.compositeAlpha = select_surface_alpha(surfaceCaps.supportedCompositeAlpha);
    swapchainCreateInfo.presentMode = select_present_mode(_d.physicalDevice, _d.surface);
    swapchainCreateInfo.clipped = VK_TRUE;
    check_success(vkCreateSwapchainKHR(_d.device, &swapchainCreateInfo, nullptr, &_d.swapchain));

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
        imageViewCreateInfo.format = surfaceFormat.format;
        imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewCreateInfo.subresourceRange.layerCount = 1;
        imageViewCreateInfo.subresourceRange.levelCount = 1;
        check_success(vkCreateImageView(_d.device, &imageViewCreateInfo, nullptr, &perImage.imageView));

        VkFramebufferCreateInfo framebufferCreateInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        framebufferCreateInfo.renderPass = _d.renderPass;
        framebufferCreateInfo.attachmentCount = 1;
        framebufferCreateInfo.pAttachments = &perImage.imageView;
        framebufferCreateInfo.width = _d.swapchainSize.width;
        framebufferCreateInfo.height = _d.swapchainSize.height;
        framebufferCreateInfo.layers = 1;
        check_success(vkCreateFramebuffer(_d.device, &framebufferCreateInfo, nullptr, &perImage.framebuffer));

        check_success(vkCreateSemaphore(_d.device, &semaphoreCreateInfo, nullptr, &perImage.renderCompleteSemaphore));
    }
}

void Renderer::render()
{
    const auto& perFrame = _d.perFrame[_d.nextFrameIndex++];
    _d.nextFrameIndex %= _d.perFrame.size();

    uint32_t imageIndex;
    check_success(vkAcquireNextImageKHR(_d.device, _d.swapchain, UINT64_MAX, perFrame.imageAcquiredSemaphore, nullptr, &imageIndex));
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
    check_success(vkQueuePresentKHR(_d.queue, &presentInfo));
}

void Renderer::record_command_buffer(const PerFrameData& perFrame, const PerImageData& perImage)
{
    check_success(vkResetCommandPool(_d.device, perFrame.commandPool, 0));

    VkClearValue clearValue = { };
    clearValue.color.float32[3] = 1.0f;

    VkCommandBufferBeginInfo commandBufferBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    VkRenderPassBeginInfo renderPassBeginInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    renderPassBeginInfo.renderPass = _d.renderPass;
    renderPassBeginInfo.renderArea = { {}, _d.swapchainSize };
    renderPassBeginInfo.framebuffer = perImage.framebuffer;
    renderPassBeginInfo.clearValueCount = 1;
    renderPassBeginInfo.pClearValues = &clearValue;

    check_success(vkBeginCommandBuffer(perFrame.commandBuffer, &commandBufferBeginInfo));
    vkCmdBeginRenderPass(perFrame.commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdEndRenderPass(perFrame.commandBuffer);
    check_success(vkEndCommandBuffer(perFrame.commandBuffer));
}