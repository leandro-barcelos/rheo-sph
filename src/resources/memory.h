#ifndef RHEOSPH_MEMORY_H
#define RHEOSPH_MEMORY_H

#include <cstdint>
#include <vulkan/vulkan_raii.hpp>

#include "../core/vulkan_device.h"

namespace resources {

class MemoryAllocator {
 public:
  [[nodiscard]] static uint32_t FindMemoryType(
      core::VulkanDevice const& vulkan_device, uint32_t type_filter,
      vk::MemoryPropertyFlags properties);
};

}  // namespace resources

#endif  // !RHEOSPH_MEMORY_H
