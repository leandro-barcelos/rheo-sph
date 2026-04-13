#ifndef RHEOSPH_DESCRIPTOR_H
#define RHEOSPH_DESCRIPTOR_H

#include <vulkan/vulkan_raii.hpp>

#include "../core/vulkan_device.h"
#include "vulkan/vulkan.hpp"

namespace resources {

class DescriptorAllocator {
 public:
  struct PoolSize {
    vk::DescriptorType type;
    uint32_t count;
  } __attribute__((aligned(8)));

  void Init(core::VulkanDevice const& vulkan_device, uint32_t max_sets,
            std::span<const PoolSize> sizes);
  [[nodiscard]] vk::raii::DescriptorSet Allocate(
      core::VulkanDevice const& vulkan_device,
      vk::DescriptorSetLayout layout) const;

 private:
  vk::raii::DescriptorPool descriptor_pool_ = nullptr;
};

}  // namespace resources

#endif  // !RHEOSPH_DESCRIPTOR_H
