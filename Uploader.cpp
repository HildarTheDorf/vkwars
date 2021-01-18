#include "Uploader.hpp"

#include "RendererUtil.hpp"

constexpr VkDeviceSize STAGING_BUFFER_SIZE = 1 << 20;

Uploader::Uploader(vk::Device device, uint32_t queueFamilyIndex, uint32_t queueIndex, vma::Allocator& allocator)
    :device(device), queue(device.getQueue(queueFamilyIndex, queueIndex)), currentOffset(0), uploadInProgress(false)
{
    const auto stagingBufferCreateInfo = vk::BufferCreateInfo()
        .setSize(STAGING_BUFFER_SIZE)
        .setUsage(vk::BufferUsageFlagBits::eTransferSrc);
    std::tie(stagingBuffer, stagingMemory) = allocator.createBuffer(stagingBufferCreateInfo, VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);

    const auto commandPoolCreateInfo = vk::CommandPoolCreateInfo()
        .setFlags(vk::CommandPoolCreateFlagBits::eTransient)
        .setQueueFamilyIndex(queueFamilyIndex);
    commandPool = device.createCommandPoolUnique(commandPoolCreateInfo);

    const auto commandBufferAllocateInfo = vk::CommandBufferAllocateInfo()
        .setCommandPool(commandPool.get())
        .setCommandBufferCount(1)
        .setLevel(vk::CommandBufferLevel::ePrimary);
    const auto commandBuffers = device.allocateCommandBuffers(commandBufferAllocateInfo);
    commandBuffer = commandBuffers[0];

    const auto fenceCreateInfo = vk::FenceCreateInfo();
    fence = device.createFenceUnique(fenceCreateInfo);
}

Uploader::~Uploader()
{
    if (uploadInProgress)
    {
        static_cast<void>(finish());
    }
}

void Uploader::begin()
{
    const auto stagingCommandBufferBeginInfo = vk::CommandBufferBeginInfo()
        .setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    commandBuffer.begin(stagingCommandBufferBeginInfo);
}

void Uploader::end()
{
    commandBuffer.end();

    const auto commandBuffers = std::array{ commandBuffer };

    const auto stagingSubmitInfo = vk::SubmitInfo()
        .setCommandBuffers(commandBuffers);

    queue.submit(stagingSubmitInfo, fence.get());
    uploadInProgress = true;
}

vk::Result Uploader::finish()
{
    uploadInProgress = false;
    return device.waitForFences(fence.get(), true, UINT64_MAX);
}

void Uploader::clearImage(vk::Image image, vk::ImageSubresourceRange subresourceRange, vk::ClearColorValue clearColor, vk::ImageLayout newLayout, vk::AccessFlags newAccess, vk::PipelineStageFlags newStage)
{
    auto imageBarrier = vk::ImageMemoryBarrier()
        .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
        .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
        .setImage(image)
        .setSubresourceRange(subresourceRange);

    imageBarrier.setSrcAccessMask(vk::AccessFlags());
    imageBarrier.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
    imageBarrier.setOldLayout(vk::ImageLayout::eUndefined);
    imageBarrier.setNewLayout(vk::ImageLayout::eTransferDstOptimal);
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), nullptr, nullptr, imageBarrier);

    commandBuffer.clearColorImage(image, vk::ImageLayout::eTransferDstOptimal, clearColor, subresourceRange);

    imageBarrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
    imageBarrier.setDstAccessMask(newAccess);
    imageBarrier.setOldLayout(vk::ImageLayout::eTransferDstOptimal);
    imageBarrier.setNewLayout(newLayout);
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, newStage, vk::DependencyFlags(), nullptr, nullptr, imageBarrier);
}

void Uploader::uploadImage(vk::Image image, vk::ImageSubresourceLayers subresourceLayers, vk::Extent3D imageExtent, void *pData, vk::ImageLayout newLayout, vk::AccessFlags newAccess, vk::PipelineStageFlags newStage)
{
    const auto size = 4 * imageExtent.width * imageExtent.height * imageExtent.depth; // TODO: Support formats with sizes other than 4-bytes
    if (currentOffset + size > STAGING_BUFFER_SIZE)
    {
        vk::throwResultException(vk::Result::eErrorOutOfDeviceMemory, "Uploader::uploadImage");
    }

    check_success(stagingMemory.withMap([pData, size](void *pStaging) {
        memcpy(pStaging, pData, size);
    }, currentOffset));

    auto imageBarrier = vk::ImageMemoryBarrier()
        .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
        .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
        .setImage(image)
        .setSubresourceRange({subresourceLayers.aspectMask, subresourceLayers.mipLevel, 1, subresourceLayers.baseArrayLayer, subresourceLayers.layerCount});

    imageBarrier.setSrcAccessMask(vk::AccessFlags());
    imageBarrier.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
    imageBarrier.setOldLayout(vk::ImageLayout::eUndefined);
    imageBarrier.setNewLayout(vk::ImageLayout::eTransferDstOptimal);
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), nullptr, nullptr, imageBarrier);

    const auto copyRegion = vk::BufferImageCopy()
        .setBufferOffset(currentOffset)
        .setBufferRowLength(0)
        .setBufferImageHeight(0)
        .setImageSubresource(subresourceLayers)
        .setImageOffset({})
        .setImageExtent(imageExtent);
    commandBuffer.copyBufferToImage(stagingBuffer.get(), image, vk::ImageLayout::eTransferDstOptimal, copyRegion);

    imageBarrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
    imageBarrier.setDstAccessMask(newAccess);
    imageBarrier.setOldLayout(vk::ImageLayout::eTransferDstOptimal);
    imageBarrier.setNewLayout(newLayout);
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, newStage, vk::DependencyFlags(), nullptr, nullptr, imageBarrier);

    currentOffset += size;
}