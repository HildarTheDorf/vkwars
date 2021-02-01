#include "UIRenderer.hpp"

#include "imgui.h"

#include <glm/glm.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>

constexpr VkDeviceSize DEFAULT_INDEX_BUFFER_SIZE = 1 << 20;
constexpr VkDeviceSize DEFAULT_VERTEX_BUFFER_SIZE = 1 << 20;

struct PushConstants
{
    glm::vec2 scale;
    glm::vec2 translate;
};

static std::vector<uint8_t> load_file(std::filesystem::path path)
{
    std::ifstream file(path, std::ios::binary);

    std::vector<uint8_t> ret;
    std::copy(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>(), std::back_inserter(ret));
    return ret;
}

static vk::UniqueShaderModule load_shader(vk::Device device, std::filesystem::path path)
{
    const auto raw = load_file("shaders" / path += ".spv");

    if (raw.empty())
    {
        throw std::runtime_error("Failed to load shader '" + path.string() + "'");
    }

    std::vector<uint32_t> spv(raw.size() / sizeof(uint32_t));

    memcpy(spv.data(), raw.data(), spv.size() * sizeof(uint32_t));

    const auto shaderModuleCreateInfo = vk::ShaderModuleCreateInfo()
        .setCode(spv);

    return device.createShaderModuleUnique(shaderModuleCreateInfo);
}

UIRenderer::UIRenderer()
{
    for (auto& perFrame : perFrameData)
    {
        perFrame.indexMemorySize = DEFAULT_INDEX_BUFFER_SIZE;
        perFrame.vertexMemorySize = DEFAULT_VERTEX_BUFFER_SIZE;
    }
}

void UIRenderer::init(vk::Device device, vma::Allocator& allocator, Uploader& uploader, vk::RenderPass renderPass, uint32_t subpass)
{
    pAllocator = &allocator;

    auto& io = ImGui::GetIO();

    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

    int texWidth, texHeight;
    unsigned char *pTexPixels;
    io.Fonts->GetTexDataAsRGBA32(&pTexPixels, &texWidth, &texHeight);

    const auto texExtent = vk::Extent3D{static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1};

    const auto fontImageCreateInfo = vk::ImageCreateInfo()
        .setImageType(vk::ImageType::e2D)
        .setFormat(vk::Format::eR8G8B8A8Srgb)
        .setExtent(texExtent)
        .setMipLevels(1)
        .setArrayLayers(1)
        .setSamples(vk::SampleCountFlagBits::e1)
        .setTiling(vk::ImageTiling::eOptimal)
        .setUsage(vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled);
    std::tie(fontImage, fontMemory) = allocator.createImage(fontImageCreateInfo, VMA_MEMORY_USAGE_GPU_ONLY);

    uploader.uploadImage(fontImage.get(), {vk::ImageAspectFlagBits::eColor, 0, 0, 1}, texExtent, pTexPixels, vk::ImageLayout::eShaderReadOnlyOptimal, vk::AccessFlagBits::eShaderRead, vk::PipelineStageFlagBits::eFragmentShader);

    const auto fontImageViewCreateInfo = vk::ImageViewCreateInfo()
        .setImage(fontImage.get())
        .setViewType(vk::ImageViewType::e2D)
        .setFormat(vk::Format::eR8G8B8A8Srgb)
        .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
    fontImageView = device.createImageViewUnique(fontImageViewCreateInfo);

    const auto samplerCreateInfo = vk::SamplerCreateInfo()
        .setMagFilter(vk::Filter::eLinear)
        .setMinFilter(vk::Filter::eLinear)
        .setMaxLod(VK_LOD_CLAMP_NONE);
    sampler = device.createSamplerUnique(samplerCreateInfo);

    const auto immutableSamplers = std::array{ sampler.get() };

    const auto descriptorBindings = std::array{
        vk::DescriptorSetLayoutBinding()
            .setBinding(0)
            .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
            .setDescriptorCount(1)
            .setStageFlags(vk::ShaderStageFlagBits::eFragment)
            .setImmutableSamplers(immutableSamplers)
    };

    const auto descriptorSetLayoutCreateInfo = vk::DescriptorSetLayoutCreateInfo()
        .setBindings(descriptorBindings);
    descriptorSetLayout = device.createDescriptorSetLayoutUnique(descriptorSetLayoutCreateInfo);

    const auto pushConstantRanges = std::array{
        vk::PushConstantRange()
            .setOffset(0)
            .setSize(sizeof(PushConstants))
            .setStageFlags(vk::ShaderStageFlagBits::eVertex)
    };

    const auto descriptorSetLayouts = std::array{ descriptorSetLayout.get() };

    const auto pipelineLayoutCreateInfo = vk::PipelineLayoutCreateInfo()
        .setPushConstantRanges(pushConstantRanges)
        .setSetLayouts(descriptorSetLayouts);
    pipelineLayout = device.createPipelineLayoutUnique(pipelineLayoutCreateInfo);

    const auto descriptorPoolSizes = std::array{
        vk::DescriptorPoolSize()
            .setType(vk::DescriptorType::eCombinedImageSampler)
            .setDescriptorCount(1)
    };

    const auto descriptorPoolCreateInfo = vk::DescriptorPoolCreateInfo()
        .setMaxSets(1)
        .setPoolSizes(descriptorPoolSizes);
    descriptorPool = device.createDescriptorPoolUnique(descriptorPoolCreateInfo);

    const auto descriptorSetAllocateInfo = vk::DescriptorSetAllocateInfo()
        .setDescriptorPool(descriptorPool.get())
        .setDescriptorSetCount(1)
        .setSetLayouts(descriptorSetLayouts);

    auto descriptorSets = device.allocateDescriptorSets(descriptorSetAllocateInfo);
    descriptorSet = descriptorSets[0];

    const auto descriptorImageInfos = std::array{
        vk::DescriptorImageInfo()
            .setImageView(fontImageView.get())
            .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
    };

    const auto descriptorWrites = std::array{
        vk::WriteDescriptorSet()
            .setDstSet(descriptorSet)
            .setDstBinding(0)
            .setDstArrayElement(0)
            .setDescriptorCount(1)
            .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
            .setImageInfo(descriptorImageInfos)
    };

    device.updateDescriptorSets(descriptorWrites, nullptr);

    for (auto& perFrame : perFrameData)
    {
        std::tie(perFrame.indexBuffer, perFrame.indexMemory) = allocate_buffer(perFrame.indexMemorySize, vk::BufferUsageFlagBits::eIndexBuffer);
        std::tie(perFrame.vertexBuffer, perFrame.vertexMemory) = allocate_buffer(perFrame.vertexMemorySize, vk::BufferUsageFlagBits::eVertexBuffer);
    }

    const auto fragmentShader = load_shader(device, "main.frag");
    const auto vertexShader = load_shader(device, "main.vert");

    const auto shaderStages = std::array{
        vk::PipelineShaderStageCreateInfo()
            .setStage(vk::ShaderStageFlagBits::eVertex)
            .setModule(vertexShader.get())
            .setPName("main"),
        vk::PipelineShaderStageCreateInfo()
            .setStage(vk::ShaderStageFlagBits::eFragment)
            .setModule(fragmentShader.get())
            .setPName("main")
    };

    const auto vertexBindings = std::array{
        vk::VertexInputBindingDescription()
            .setBinding(0)
            .setInputRate(vk::VertexInputRate::eVertex)
            .setStride(sizeof(ImDrawVert))
    };

    const auto vertexAttribs = std::array{
        vk::VertexInputAttributeDescription()
            .setLocation(0)
            .setBinding(0)
            .setFormat(vk::Format::eR32G32Sfloat)
            .setOffset(offsetof(ImDrawVert, pos)),
        vk::VertexInputAttributeDescription()
            .setLocation(1)
            .setBinding(0)
            .setFormat(vk::Format::eR32G32Sfloat)
            .setOffset(offsetof(ImDrawVert, uv)),
        vk::VertexInputAttributeDescription()
            .setLocation(2)
            .setBinding(0)
            .setFormat(vk::Format::eR8G8B8A8Unorm)
            .setOffset(offsetof(ImDrawVert, col))
    };

    const auto vertexInputState = vk::PipelineVertexInputStateCreateInfo()
        .setVertexBindingDescriptions(vertexBindings)
        .setVertexAttributeDescriptions(vertexAttribs);

    const auto inputAssemblyState = vk::PipelineInputAssemblyStateCreateInfo()
        .setTopology(vk::PrimitiveTopology::eTriangleList);

    const auto viewportState = vk::PipelineViewportStateCreateInfo()
        .setViewportCount(1)
        .setScissorCount(1);

    const auto rasterizationState = vk::PipelineRasterizationStateCreateInfo()
        .setLineWidth(1.0f);

    const auto colorBlendAttachments = std::array{
        vk::PipelineColorBlendAttachmentState()
            .setBlendEnable(true)
            .setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
            .setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
            .setColorBlendOp(vk::BlendOp::eAdd)
            .setSrcAlphaBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
            .setDstAlphaBlendFactor(vk::BlendFactor::eZero)
            .setAlphaBlendOp(vk::BlendOp::eAdd)
            .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA)
    };

    const auto colorBlendState = vk::PipelineColorBlendStateCreateInfo()
        .setAttachments(colorBlendAttachments);

    const auto multisampleState = vk::PipelineMultisampleStateCreateInfo()
        .setRasterizationSamples(vk::SampleCountFlagBits::e1);

    const auto dynamicStates = std::array{ vk::DynamicState::eViewport, vk::DynamicState::eScissor };
    const auto dynamicState = vk::PipelineDynamicStateCreateInfo()
        .setDynamicStates(dynamicStates);

    const auto pipelineCreateInfo = vk::GraphicsPipelineCreateInfo()
        .setStages(shaderStages)
        .setPVertexInputState(&vertexInputState)
        .setPInputAssemblyState(&inputAssemblyState)
        .setPViewportState(&viewportState)
        .setPRasterizationState(&rasterizationState)
        .setPMultisampleState(&multisampleState)
        .setPColorBlendState(&colorBlendState)
        .setPDynamicState(&dynamicState)
        .setLayout(pipelineLayout.get())
        .setRenderPass(renderPass)
        .setSubpass(subpass);

    graphicsPipeline = check_success(device.createGraphicsPipelineUnique(nullptr, pipelineCreateInfo)); // TODO: PipelineCache
}

static void for_each_cmd_list(ImDrawData *pDD, std::function<void(ImDrawList *)> callback)
{
    for (int i = 0; i < pDD->CmdListsCount; ++i)
    {
        callback(pDD->CmdLists[i]);
    }
}

void UIRenderer::render(vk::CommandBuffer commandBuffer, vk::Extent2D framebufferExtent, uint32_t frameIndex)
{
    auto& perFrame = perFrameData[frameIndex];
    const auto pDD = ImGui::GetDrawData();

    VkDeviceSize requiredIndexBufferSize = 0;
    VkDeviceSize requiredVertexBufferSize = 0;
    for_each_cmd_list(pDD, [&](const auto pCL)
    {
        requiredIndexBufferSize += pCL->IdxBuffer.size_in_bytes();
        requiredVertexBufferSize += pCL->VtxBuffer.size_in_bytes();
    });

    while (requiredIndexBufferSize > perFrame.indexMemorySize)
    {
        perFrame.indexMemorySize *= 2;
        std::tie(perFrame.indexBuffer, perFrame.indexMemory) = allocate_buffer(perFrame.indexMemorySize, vk::BufferUsageFlagBits::eIndexBuffer);
    }

    while (requiredVertexBufferSize > perFrame.vertexMemorySize)
    {
        perFrame.vertexMemorySize *= 2;
        std::tie(perFrame.vertexBuffer, perFrame.vertexMemory) = allocate_buffer(perFrame.vertexMemorySize, vk::BufferUsageFlagBits::eVertexBuffer);
    }

    PushConstants pushConstants;
    pushConstants.scale.x = 2.0f / pDD->DisplaySize.x;
    pushConstants.scale.y = 2.0f / pDD->DisplaySize.y;
    pushConstants.translate.x = -1.0f - pDD->DisplayPos.x * pushConstants.scale.x;
    pushConstants.translate.y = -1.0f - pDD->DisplayPos.y * pushConstants.scale.y;

    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout.get(), 0, descriptorSet, nullptr);
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline.get());
    commandBuffer.bindIndexBuffer(perFrame.indexBuffer.get(), 0, vk::IndexType::eUint16);
    commandBuffer.bindVertexBuffers(0, perFrame.vertexBuffer.get(), {0});
    commandBuffer.pushConstants<PushConstants>(pipelineLayout.get(), vk::ShaderStageFlagBits::eVertex, 0, pushConstants);

    uint32_t baseIdx = 0;
    int32_t baseVtx = 0;
    for_each_cmd_list(pDD, [&](const auto pCL)
    {
        perFrame.indexMemory.withMap([&idx = pCL->IdxBuffer](void *pData) {
            memcpy(pData, idx.Data, idx.size_in_bytes());
        }, sizeof(ImDrawIdx) * baseIdx);

        perFrame.vertexMemory.withMap([&vtx = pCL->VtxBuffer](void *pData) {
            memcpy(pData, vtx.Data, vtx.size_in_bytes());
        }, sizeof(ImDrawVert) * baseVtx);

        for (const auto& drawCommand : pCL->CmdBuffer)
        {
            ImVec4 clip_rect;
            clip_rect.x = (drawCommand.ClipRect.x - pDD->DisplayPos.x) * pDD->FramebufferScale.x;
            clip_rect.y = (drawCommand.ClipRect.y - pDD->DisplayPos.y) * pDD->FramebufferScale.y;
            clip_rect.z = (drawCommand.ClipRect.z - pDD->DisplayPos.x) * pDD->FramebufferScale.x;
            clip_rect.w = (drawCommand.ClipRect.w - pDD->DisplayPos.y) * pDD->FramebufferScale.y;

            vk::Rect2D scissor;
            scissor.offset.x = clip_rect.x;
            scissor.offset.y = clip_rect.y;
            scissor.extent.width = clip_rect.z - clip_rect.x;
            scissor.extent.height = clip_rect.w - clip_rect.y;

            commandBuffer.setScissor(0, scissor);
            commandBuffer.drawIndexed(drawCommand.ElemCount, 1, baseIdx + drawCommand.IdxOffset, baseVtx + drawCommand.VtxOffset, 0);
        }

        baseIdx += pCL->IdxBuffer.Size;
        baseVtx += pCL->VtxBuffer.Size;
    });

    perFrame.indexMemory.flush(0, sizeof(ImDrawIdx) * baseIdx);
    perFrame.vertexMemory.flush(0, sizeof(ImDrawVert) * baseVtx);
}

std::pair<vk::UniqueBuffer, vma::Allocation> UIRenderer::allocate_buffer(VkDeviceSize size, vk::BufferUsageFlags usage)
{
    const auto bufferCreateInfo = vk::BufferCreateInfo()
        .setSize(size)
        .setUsage(usage);
    return pAllocator->createBuffer(bufferCreateInfo, VMA_MEMORY_USAGE_CPU_TO_GPU);
}
