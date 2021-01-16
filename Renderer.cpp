#include "Renderer.hpp"

#include "BadVkResult.hpp"
#include "Uploader.hpp"
#include "Window.hpp"

#include "imgui.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <stdexcept>

constexpr uint32_t DESIRED_API_VERSION = VK_API_VERSION_1_2;
constexpr auto DESIRED_DEPTH_FORMATS = std::array{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_X8_D24_UNORM_PACK32 };
constexpr auto DESIRED_PRESENT_MODES = std::array{ VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_FIFO_KHR };
constexpr uint32_t DESIRED_SWAPCHAIN_IMAGES = 3;

constexpr uint32_t compute_image_count(uint32_t min, uint32_t max)
{
    uint32_t ret = std::max(min + 1, DESIRED_SWAPCHAIN_IMAGES);
    return max ? std::min(ret, max) : ret;
}

static void draw_ui(VkCommandBuffer cb)
{
    const auto dd = ImGui::GetDrawData();
    const auto& clip_off = dd->DisplayPos;
    const auto& clip_scale = dd->FramebufferScale;

    uint32_t idx_offset = 0;
    uint32_t vtx_offset = 0;

    for (auto i = 0; i < dd->CmdListsCount; ++i)
    {
        const auto cl = dd->CmdLists[i];
        for (const auto& dc : cl->CmdBuffer)
        {
            ImVec4 clip_rect;
            clip_rect.x = (dc.ClipRect.x - clip_off.x) * clip_scale.x;
            clip_rect.y = (dc.ClipRect.y - clip_off.y) * clip_scale.y;
            clip_rect.z = (dc.ClipRect.z - clip_off.x) * clip_scale.x;
            clip_rect.w = (dc.ClipRect.w - clip_off.y) * clip_scale.y;

            VkRect2D scissor;
            scissor.offset.x = static_cast<int32_t>(clip_rect.x);
            scissor.offset.y = static_cast<int32_t>(clip_rect.y);
            scissor.extent.width = static_cast<uint32_t>(clip_rect.z - clip_rect.x);
            scissor.extent.height = static_cast<uint32_t>(clip_rect.w - clip_rect.y);

            vkCmdSetScissor(cb, 0, 1, &scissor);
            //vkCmdDrawIndexed(cb, dc.ElemCount, 1, idx_offset + dc.IdxOffset, static_cast<int32_t>(vtx_offset + dc.VtxOffset), 0);
        }
        idx_offset += static_cast<uint32_t>(cl->IdxBuffer.Size);
        vtx_offset += static_cast<uint32_t>(cl->VtxBuffer.Size);
    }
}

static std::vector<uint8_t> load_file(const char *filename)
{
    std::ifstream file(filename, std::ios::binary);

    std::vector<uint8_t> ret;

    std::copy(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>(), std::back_inserter(ret));

    return ret;
}

static std::vector<uint32_t> load_pipelinecache()
{
    const auto raw = load_file("pipelinecache.bin");

    std::vector<uint32_t> ret(raw.size() / sizeof(uint32_t));
    memcpy(ret.data(), raw.data(), sizeof(uint32_t) * ret.size());

    return ret;
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
    check_success(vkCreateInstance(&instanceCreateInfo, nullptr, &d.instance));

    check_success(window.create_vulkan_surface(d.instance, &d.surface));

    std::tie(d.physicalDevice, d.queueFamilyIndex) = select_device_and_queue(d.instance, d.surface);
    d.surfaceFormat = select_surface_format(d.physicalDevice, d.surface);
    d.depthFormat = select_depth_format(d.physicalDevice);

    const float queuePriority = 0.0;

    VkDeviceQueueCreateInfo queueCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
    queueCreateInfo.queueFamilyIndex = d.queueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    std::array<const char *, 1> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkDeviceCreateInfo deviceCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

    check_success(vkCreateDevice(d.physicalDevice, &deviceCreateInfo, nullptr, &d.device));
    vkGetDeviceQueue(d.device, d.queueFamilyIndex, 0, &d.queue);

    VmaAllocatorCreateInfo allocatorCreateInfo = { };
    allocatorCreateInfo.physicalDevice = d.physicalDevice;
    allocatorCreateInfo.device = d.device;
    allocatorCreateInfo.instance = d.instance;
    allocatorCreateInfo.vulkanApiVersion = DESIRED_API_VERSION;

    check_success(vmaCreateAllocator(&allocatorCreateInfo, &d.allocator));

    unsigned char *fontPixels;
    int fontWidth, fontHeight;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&fontPixels, &fontWidth, &fontHeight);

    VkImageCreateInfo fontImageCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    fontImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    fontImageCreateInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    fontImageCreateInfo.extent = { static_cast<uint32_t>(fontWidth), static_cast<uint32_t>(fontHeight), 1 };
    fontImageCreateInfo.mipLevels = 1;
    fontImageCreateInfo.arrayLayers = 1;
    fontImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    fontImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    fontImageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    VmaAllocationCreateInfo fontImageAllocationInfo = { };
    fontImageAllocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    check_success(vmaCreateImage(d.allocator, &fontImageCreateInfo, &fontImageAllocationInfo, &d.fontImage, &d.fontMemory, nullptr));

    Uploader uploader(d.device, d.queue, d.allocator, d.queueFamilyIndex);
    uploader.begin();
    
    uploader.upload(d.fontImage,
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, fontImageCreateInfo.extent, 4,
        fontPixels,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT);

    uploader.end();

    VkDescriptorSetLayoutBinding fontTextureBinding = { };
    fontTextureBinding.binding = 0;
    fontTextureBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    fontTextureBinding.descriptorCount = 1;
    fontTextureBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    fontTextureBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo uiDescriptorSetLayoutCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    uiDescriptorSetLayoutCreateInfo.bindingCount = 1;
    uiDescriptorSetLayoutCreateInfo.pBindings = &fontTextureBinding;
    check_success(vkCreateDescriptorSetLayout(d.device, &uiDescriptorSetLayoutCreateInfo, nullptr, &d.uiDescriptorSetLayout));

    VkPipelineLayoutCreateInfo uiPipelineLayoutCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    uiPipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    uiPipelineLayoutCreateInfo.pPushConstantRanges = nullptr;
    uiPipelineLayoutCreateInfo.setLayoutCount = 1;
    uiPipelineLayoutCreateInfo.pSetLayouts = &d.uiDescriptorSetLayout;

    check_success(vkCreatePipelineLayout(d.device, &uiPipelineLayoutCreateInfo, nullptr, &d.uiPipelineLayout));

    VkDescriptorPoolSize poolSize = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 };

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    descriptorPoolCreateInfo.maxSets = 1;
    descriptorPoolCreateInfo.poolSizeCount = 1;
    descriptorPoolCreateInfo.pPoolSizes = &poolSize;
    check_success(vkCreateDescriptorPool(d.device, &descriptorPoolCreateInfo, nullptr, &d.uiDescriptorPool));

    VkDescriptorSetAllocateInfo uiDescriptorSetAllocateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    uiDescriptorSetAllocateInfo.descriptorPool = d.uiDescriptorPool;
    uiDescriptorSetAllocateInfo.descriptorSetCount = 1;
    uiDescriptorSetAllocateInfo.pSetLayouts = &d.uiDescriptorSetLayout;

    check_success(vkAllocateDescriptorSets(d.device, &uiDescriptorSetAllocateInfo, &d.uiDescriptorSet));

    VkDescriptorImageInfo fontImageInfo = { };
    fontImageInfo.sampler = nullptr;
    fontImageInfo.imageView = nullptr;
    fontImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet descriptorWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    descriptorWrite.dstSet = d.uiDescriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.pImageInfo = &fontImageInfo;

    vkUpdateDescriptorSets(d.device, 1, &descriptorWrite, 0, nullptr);

    const auto pipelineCacheData = load_pipelinecache();

    VkPipelineCacheCreateInfo pipelineCacheCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
    pipelineCacheCreateInfo.initialDataSize = sizeof(uint32_t) * pipelineCacheData.size();
    pipelineCacheCreateInfo.pInitialData = pipelineCacheData.data();

    check_success(vkCreatePipelineCache(d.device, &pipelineCacheCreateInfo, nullptr, &d.pipelineCache));

    std::array<VkAttachmentDescription, 2> attachmentDescs = { };
    attachmentDescs[0].format = d.surfaceFormat.format;
    attachmentDescs[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescs[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescs[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescs[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachmentDescs[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    attachmentDescs[1].format = d.depthFormat;
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
    renderPassCreateInfo.attachmentCount = static_cast<uint32_t>(attachmentDescs.size());
    renderPassCreateInfo.pAttachments = attachmentDescs.data();
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &subpassDesc;
    renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(subpassDeps.size());
    renderPassCreateInfo.pDependencies = subpassDeps.data();
    check_success(vkCreateRenderPass(d.device, &renderPassCreateInfo, nullptr, &d.renderPass));

    //check_success(vkCreateGraphicsPipelines()); 

    for (auto& perFrame : d.perFrame)
    {
        VkCommandPoolCreateInfo commandPoolCreateInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        commandPoolCreateInfo.queueFamilyIndex = d.queueFamilyIndex;
        check_success(vkCreateCommandPool(d.device, &commandPoolCreateInfo, nullptr, &perFrame.commandPool));

        VkCommandBufferAllocateInfo commandBufferAllocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        commandBufferAllocateInfo.commandPool = perFrame.commandPool;
        commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commandBufferAllocateInfo.commandBufferCount = 1;
        check_success(vkAllocateCommandBuffers(d.device, &commandBufferAllocateInfo, &perFrame.commandBuffer));

        VkFenceCreateInfo fenceCreateInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        check_success(vkCreateFence(d.device, &fenceCreateInfo, nullptr, &perFrame.fence));

        VkSemaphoreCreateInfo semaphoreCreateInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        check_success(vkCreateSemaphore(d.device, &semaphoreCreateInfo, nullptr, &perFrame.imageAcquiredSemaphore));
    }

    build_swapchain();

    uploader.finish();
}

void Renderer::render()
{
    const auto& perFrame = d.perFrame[d.nextFrameIndex++];
    d.nextFrameIndex %= d.perFrame.size();

    check_success(vkWaitForFences(d.device, 1, &perFrame.fence, VK_TRUE, UINT64_MAX));

    uint32_t imageIndex;
    const auto acquireResult = vkAcquireNextImageKHR(d.device, d.swapchain, UINT64_MAX, perFrame.imageAcquiredSemaphore, nullptr, &imageIndex);

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
        const auto& perImage = d.perImage[imageIndex];

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

        check_success(vkResetFences(d.device, 1, &perFrame.fence));
        check_success(vkQueueSubmit(d.queue, 1, &submitInfo, perFrame.fence));

        VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &perImage.renderCompleteSemaphore;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &d.swapchain;
        presentInfo.pImageIndices = &imageIndex;
        const auto presentResult = vkQueuePresentKHR(d.queue, &presentInfo);
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
    check_success(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(d.physicalDevice, d.surface, &surfaceCaps));

    d.swapchainSize = surfaceCaps.currentExtent;

    VkSwapchainCreateInfoKHR swapchainCreateInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    swapchainCreateInfo.surface = d.surface;
    swapchainCreateInfo.minImageCount = compute_image_count(surfaceCaps.minImageCount, surfaceCaps.maxImageCount);
    swapchainCreateInfo.imageFormat = d.surfaceFormat.format;
    swapchainCreateInfo.imageColorSpace = d.surfaceFormat.colorSpace;
    swapchainCreateInfo.imageExtent = d.swapchainSize;
    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainCreateInfo.preTransform = surfaceCaps.currentTransform;
    swapchainCreateInfo.compositeAlpha = select_surface_alpha(surfaceCaps.supportedCompositeAlpha);
    swapchainCreateInfo.presentMode = select_present_mode(d.physicalDevice, d.surface);
    swapchainCreateInfo.clipped = VK_TRUE;
    swapchainCreateInfo.oldSwapchain = d.oldSwapchain;
    check_success(vkCreateSwapchainKHR(d.device, &swapchainCreateInfo, nullptr, &d.swapchain));

    VkImageCreateInfo depthImageCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    depthImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    depthImageCreateInfo.format = d.depthFormat;
    depthImageCreateInfo.extent = { d.swapchainSize.width, d.swapchainSize.height, 1 };
    depthImageCreateInfo.mipLevels = 1;
    depthImageCreateInfo.arrayLayers = 1;
    depthImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    depthImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    depthImageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;

    VmaAllocationCreateInfo depthImageAllocationInfo = { };
    depthImageAllocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    check_success(vmaCreateImage(d.allocator, &depthImageCreateInfo, &depthImageAllocationInfo, &d.depthImage, &d.depthMemory, nullptr));

    VkImageViewCreateInfo depthImageViewCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    depthImageViewCreateInfo.image = d.depthImage;
    depthImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    depthImageViewCreateInfo.format = d.depthFormat;
    depthImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depthImageViewCreateInfo.subresourceRange.layerCount = 1;
    depthImageViewCreateInfo.subresourceRange.levelCount = 1;
    check_success(vkCreateImageView(d.device, &depthImageViewCreateInfo, nullptr, &d.depthImageView));

    uint32_t numSwapchainImages;
    check_success(vkGetSwapchainImagesKHR(d.device, d.swapchain, &numSwapchainImages, nullptr));
    std::vector<VkImage> swapchainImages(numSwapchainImages);
    check_success(vkGetSwapchainImagesKHR(d.device, d.swapchain, &numSwapchainImages, swapchainImages.data()));

    d.perImage.resize(numSwapchainImages);
    for (uint32_t i = 0; i < numSwapchainImages; ++i)
    {
        auto& perImage = d.perImage[i];

        VkImageViewCreateInfo imageViewCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        imageViewCreateInfo.image = swapchainImages[i];
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.format = d.surfaceFormat.format;
        imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewCreateInfo.subresourceRange.layerCount = 1;
        imageViewCreateInfo.subresourceRange.levelCount = 1;
        check_success(vkCreateImageView(d.device, &imageViewCreateInfo, nullptr, &perImage.imageView));

        std::array<VkImageView, 2> attachments = {};
        attachments[0] = perImage.imageView;
        attachments[1] = d.depthImageView;

        VkFramebufferCreateInfo framebufferCreateInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        framebufferCreateInfo.renderPass = d.renderPass;
        framebufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferCreateInfo.pAttachments = attachments.data();
        framebufferCreateInfo.width = d.swapchainSize.width;
        framebufferCreateInfo.height = d.swapchainSize.height;
        framebufferCreateInfo.layers = 1;
        check_success(vkCreateFramebuffer(d.device, &framebufferCreateInfo, nullptr, &perImage.framebuffer));

        VkSemaphoreCreateInfo semaphoreCreateInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        check_success(vkCreateSemaphore(d.device, &semaphoreCreateInfo, nullptr, &perImage.renderCompleteSemaphore));
    }
}

void Renderer::rebuild_swapchain()
{
    std::array<VkFence, MAX_FRAMES_IN_FLIGHT> allFences;
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        allFences[i] = d.perFrame[i].fence;
    }
    check_success(vkWaitForFences(d.device, static_cast<uint32_t>(allFences.size()), allFences.data(), true, UINT64_MAX));

    destroy_swapchain();
    build_swapchain();
}

void Renderer::record_command_buffer(const PerFrameData& perFrame, const PerImageData& perImage)
{
    check_success(vkResetCommandPool(d.device, perFrame.commandPool, 0));

    VkCommandBufferBeginInfo commandBufferBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    std::array<VkClearValue, 2> clearValues = { };
    clearValues[0].color.float32[3] = 1.0f;
    clearValues[1].depthStencil.depth = 1.0f;

    VkRenderPassBeginInfo renderPassBeginInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    renderPassBeginInfo.renderPass = d.renderPass;
    renderPassBeginInfo.renderArea = { {}, d.swapchainSize };
    renderPassBeginInfo.framebuffer = perImage.framebuffer;
    renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassBeginInfo.pClearValues = clearValues.data();

    check_success(vkBeginCommandBuffer(perFrame.commandBuffer, &commandBufferBeginInfo));
    vkCmdBeginRenderPass(perFrame.commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    //vkCmdBindPipeline(perFrame.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, d.uiPipeline);
    vkCmdBindDescriptorSets(perFrame.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, d.uiPipelineLayout, 0, 1, &d.uiDescriptorSet, 0, nullptr);
    draw_ui(perFrame.commandBuffer);

    vkCmdEndRenderPass(perFrame.commandBuffer);
    check_success(vkEndCommandBuffer(perFrame.commandBuffer));
}
