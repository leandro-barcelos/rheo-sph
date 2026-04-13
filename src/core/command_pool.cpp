#include "command_pool.h"

void core::CommandPools::Init(core::VulkanDevice const& vulkan_device) {
  vk::CommandPoolCreateInfo graphics_pool_info{
      .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
      .queueFamilyIndex = vulkan_device.GraphicsQueueFamilyIndex(),
  };
  graphics_command_pool_ =
      vk::raii::CommandPool(vulkan_device.Device(), graphics_pool_info);

  vk::CommandPoolCreateInfo compute_pool_info{
      .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
      .queueFamilyIndex = vulkan_device.ComputeQueueFamilyIndex(),
  };
  compute_command_pool_ =
      vk::raii::CommandPool(vulkan_device.Device(), compute_pool_info);
}
