#ifndef RHEOSPH_IMAGES_H
#define RHEOSPH_IMAGES_H

#include <stb_image.h>

#include <string>
#include <vulkan/vulkan_raii.hpp>

#include "../core/command_pool.h"
#include "../core/vulkan_device.h"

// TODO: Muito específico, generalizar para
// a criação de qualquer imagem

namespace resources {

struct AllocatedImage {
  vk::raii::DeviceMemory memory = nullptr;
  vk::raii::ImageView image_view = nullptr;
  vk::raii::Sampler sampler = nullptr;
  vk::raii::Image image = nullptr;
} __attribute__((aligned(128)));

class ImageAllocator {
 public:
  [[nodiscard]] static AllocatedImage CreateImage(
      core::VulkanDevice const& vulkan_device,
      core::CommandPools const& command_pools, std::string const& filepath);
    [[nodiscard]] static AllocatedImage CreateSolidColorImage(
      core::VulkanDevice const& vulkan_device,
      core::CommandPools const& command_pools, uint8_t red, uint8_t green,
      uint8_t blue, uint8_t alpha);
};

}  // namespace resources

#endif  // !RHEOSPH_IMAGES_H
