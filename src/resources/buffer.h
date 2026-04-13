#ifndef RHEOSPH_BUFFER_H
#define RHEOSPH_BUFFER_H

#include <vulkan/vulkan_raii.hpp>

#include "../core/command_pool.h"
#include "../core/vulkan_device.h"

namespace resources {

struct AllocatedBuffer {
  vk::raii::DeviceMemory memory = nullptr;
  vk::raii::Buffer buffer = nullptr;
} __attribute__((aligned(64)));

class BufferAllocator {
 public:
  [[nodiscard]] static AllocatedBuffer CreateBuffer(
    core::VulkanDevice const& vulkan_device, vk::DeviceSize size,
    vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties);
  template <typename T>
  [[nodiscard]] static resources::AllocatedBuffer CreateUniformBuffer(
      core::VulkanDevice const& vulkan_device,
      core::CommandPools const& command_pools, T const& data);
  template <typename T>
  [[nodiscard]] static std::array<resources::AllocatedBuffer, 2> CreateSSBO(
      core::VulkanDevice const& vulkan_device,
      core::CommandPools const& command_pools,
      std::vector<T> const& objects,
      bool double_buffering = true,
      vk::BufferUsageFlags extra_usage_flags = {});

 private:
  static uint32_t FindMemoryType(core::VulkanDevice const& vulkan_device,
                                 uint32_t type_filter,
                                 vk::MemoryPropertyFlags properties);
};

}  // namespace resources

#endif  // !RHEOSPH_BUFFER_H
