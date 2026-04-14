#ifndef RHEOSPH_IMMEDIATE_SUBMIT_H
#define RHEOSPH_IMMEDIATE_SUBMIT_H

#include <vulkan/vulkan_raii.hpp>

#include "../core/command_pool.h"
#include "../core/vulkan_device.h"
#include "buffer.h"
#include "images.h"

namespace resources {

class ImmediateSubmit {
 public:
  void CopyBuffer(core::VulkanDevice const& vulkan_device,
                  core::CommandPools const& command_pools,
                  resources::AllocatedBuffer const& src_buffer,
                  resources::AllocatedBuffer const& dst_buffer,
                  vk::DeviceSize size);
  void CopyBufferToImage(core::VulkanDevice const& vulkan_device,
                         core::CommandPools const& command_pools,
                         resources::AllocatedBuffer const& buffer,
                         resources::AllocatedImage const& image, uint32_t width,
                         uint32_t height);
  void TransitionImageLayout(core::VulkanDevice const& vulkan_device,
                             core::CommandPools const& command_pools,
                             resources::AllocatedImage const& image,
                             vk::ImageLayout old_layout,
                             vk::ImageLayout new_layout);

 private:
  vk::raii::CommandBuffer command_buffer_ = nullptr;

  void BeginSingleTimeCommands(core::VulkanDevice const& vulkan_device,
                               core::CommandPools const& command_pools);
  void EndSingleTimeCommands(core::VulkanDevice const& vulkan_device);
};

}  // namespace resources

#endif  // !RHEOSPH_IMMEDIATE_SUBMIT_H
