#include "descriptor.h"

#include <cassert>
#include <vulkan/vulkan_raii.hpp>

void resources::DescriptorAllocator::Init(
    core::VulkanDevice const& vulkan_device, uint32_t max_sets,
    std::span<const resources::DescriptorAllocator::PoolSize> sizes) {
  std::vector<vk::DescriptorPoolSize> pool_size;
  for (const auto& size : sizes) {
    pool_size.push_back({.type = size.type, .descriptorCount = size.count});
  }

  vk::DescriptorPoolCreateInfo pool_info{
      .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
      .maxSets = max_sets,
      .poolSizeCount = static_cast<uint32_t>(pool_size.size()),
      .pPoolSizes = pool_size.data(),
  };

  descriptor_pool_ =
      vk::raii::DescriptorPool(vulkan_device.Device(), pool_info);
}

vk::raii::DescriptorSet resources::DescriptorAllocator::Allocate(
    core::VulkanDevice const& vulkan_device,
    vk::DescriptorSetLayout layout) const {
  assert(descriptor_pool_ != nullptr &&
         "[ERROR] Vulkan: DescriptorAllocator not initialized!");

  vk::DescriptorSetAllocateInfo alloc_info{.descriptorPool = *descriptor_pool_,
                                           .descriptorSetCount = 1,
                                           .pSetLayouts = &layout};

  return std::move(
      vulkan_device.Device().allocateDescriptorSets(alloc_info).front());
}
