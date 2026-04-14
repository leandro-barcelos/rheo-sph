#include "immediate_submit.h"

#include <stdexcept>

#include "vulkan/vulkan.hpp"

void resources::ImmediateSubmit::CopyBuffer(
    core::VulkanDevice const& vulkan_device,
    core::CommandPools const& command_pools,
    resources::AllocatedBuffer const& src_buffer,
    resources::AllocatedBuffer const& dst_buffer, vk::DeviceSize size) {
  BeginSingleTimeCommands(vulkan_device, command_pools);
  command_buffer_.copyBuffer(src_buffer.buffer, dst_buffer.buffer,
                             vk::BufferCopy(0, 0, size));
  EndSingleTimeCommands(vulkan_device);
}

void resources::ImmediateSubmit::CopyBufferToImage(
    core::VulkanDevice const& vulkan_device,
    core::CommandPools const& command_pools,
    resources::AllocatedBuffer const& buffer,
    resources::AllocatedImage const& image, uint32_t width, uint32_t height) {
  BeginSingleTimeCommands(vulkan_device, command_pools);
  vk::BufferImageCopy region{
      .bufferOffset = 0,
      .bufferRowLength = 0,
      .bufferImageHeight = 0,
      .imageSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                           .mipLevel = 0,
                           .baseArrayLayer = 0,
                           .layerCount = 1},
      .imageOffset = {.x = 0, .y = 0, .z = 0},
      .imageExtent = {.width = width, .height = height, .depth = 1}};
  command_buffer_.copyBufferToImage(buffer.buffer, image.image,
                                    vk::ImageLayout::eTransferDstOptimal,
                                    {region});
  EndSingleTimeCommands(vulkan_device);
}

void resources::ImmediateSubmit::TransitionImageLayout(
    core::VulkanDevice const& vulkan_device,
    core::CommandPools const& command_pools,
    resources::AllocatedImage const& image, vk::ImageLayout old_layout,
    vk::ImageLayout new_layout) {
  BeginSingleTimeCommands(vulkan_device, command_pools);

  vk::ImageMemoryBarrier barrier{
      .oldLayout = old_layout,
      .newLayout = new_layout,
      .image = image.image,
      .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                           .baseMipLevel = 0,
                           .levelCount = 1,
                           .baseArrayLayer = 0,
                           .layerCount = 1}};

  vk::PipelineStageFlags source_stage;
  vk::PipelineStageFlags destination_stage;

  if (old_layout == vk::ImageLayout::eUndefined &&
      new_layout == vk::ImageLayout::eTransferDstOptimal) {
    barrier.srcAccessMask = {};
    barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

    source_stage = vk::PipelineStageFlagBits::eTopOfPipe;
    destination_stage = vk::PipelineStageFlagBits::eTransfer;
  } else if (old_layout == vk::ImageLayout::eTransferDstOptimal &&
             new_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

    source_stage = vk::PipelineStageFlagBits::eTransfer;
    destination_stage = vk::PipelineStageFlagBits::eComputeShader;
  } else {
    throw std::invalid_argument(
        "[ERROR] Vulkan: unsupported layout transition!");
  }

  command_buffer_.pipelineBarrier(source_stage, destination_stage, {}, {},
                                  nullptr, barrier);
  EndSingleTimeCommands(vulkan_device);
}

void resources::ImmediateSubmit::BeginSingleTimeCommands(
    core::VulkanDevice const& vulkan_device,
    core::CommandPools const& command_pools) {
  if (command_buffer_ != nullptr) {
    throw std::runtime_error(
        "[ERROR] Buffer: tried to start a single time command that hasn't "
        "been "
        "finished yet!");
  }

  vk::CommandBufferAllocateInfo alloc_info{};
  alloc_info.commandPool = *command_pools.Graphics();
  alloc_info.level = vk::CommandBufferLevel::ePrimary;
  alloc_info.commandBufferCount = 1;
  command_buffer_ = std::move(
      vk::raii::CommandBuffers(vulkan_device.Device(), alloc_info).front());

  vk::CommandBufferBeginInfo begin_info{
      .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
  command_buffer_.begin(begin_info);
}

void resources::ImmediateSubmit::EndSingleTimeCommands(
    core::VulkanDevice const& vulkan_device) {
  command_buffer_.end();

  vk::SubmitInfo submit_info{};
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &*command_buffer_;
  vulkan_device.GraphicsQueue().submit(submit_info, nullptr);
  vulkan_device.GraphicsQueue().waitIdle();

  command_buffer_ = nullptr;
}
