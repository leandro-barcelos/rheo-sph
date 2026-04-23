#ifndef RHEOSPH_BUFFER_H
#define RHEOSPH_BUFFER_H

#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <vulkan/vulkan_raii.hpp>

#include "../core/command_pool.h"
#include "../core/vulkan_device.h"

namespace resources {

struct AllocatedBuffer {
  vk::raii::DeviceMemory memory = nullptr;
  vk::raii::Buffer buffer = nullptr;
  void* mapped = nullptr;
} __attribute__((aligned(128)));

class BufferAllocator {
 public:
  [[nodiscard]] static AllocatedBuffer CreateBuffer(
      core::VulkanDevice const& vulkan_device, vk::DeviceSize size,
      vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties);

  [[nodiscard]] static AllocatedBuffer CreateMappedUniformBuffer(
      core::VulkanDevice const& vulkan_device, vk::DeviceSize size);

  static void WriteMapped(AllocatedBuffer const& buffer, void const* data,
                          vk::DeviceSize size) {
    if (buffer.mapped == nullptr) {
      throw std::runtime_error(
          "[ERROR] Vulkan: Tried writing to an unmapped buffer!");
    }
    std::memcpy(static_cast<std::byte*>(buffer.mapped), data,
                static_cast<size_t>(size));
  }

  template <typename T>
  [[nodiscard]] static resources::AllocatedBuffer CreateUniformBuffer(
      core::VulkanDevice const& vulkan_device,
      core::CommandPools const& command_pools, T const& data);
  template <typename T>
  [[nodiscard]] static std::array<resources::AllocatedBuffer, 2> CreateSSBO(
      core::VulkanDevice const& vulkan_device,
      core::CommandPools const& command_pools, std::vector<T> const& objects,
      bool double_buffering = true,
      vk::BufferUsageFlags extra_usage_flags = {});
};

}  // namespace resources

#endif  // !RHEOSPH_BUFFER_H
