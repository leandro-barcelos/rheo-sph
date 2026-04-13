#include "immediate_submit.h"

#include <stdexcept>

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

void resources::ImmediateSubmit::BeginSingleTimeCommands(
    core::VulkanDevice const& vulkan_device,
    core::CommandPools const& command_pools) {
  if (command_buffer_ != nullptr) {
    throw std::runtime_error(
        "[ERROR] Buffer: tried to start a single time command that hasn't been "
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
