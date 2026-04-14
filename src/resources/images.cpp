#define STB_IMAGE_IMPLEMENTATION
#include "images.h"

#include <array>
#include <cstring>
#include <stdexcept>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "buffer.h"
#include "immediate_submit.h"
#include "memory.h"

resources::AllocatedImage resources::ImageAllocator::CreateImage(
    core::VulkanDevice const& vulkan_device,
    core::CommandPools const& command_pools, std::string const& filepath) {
  int tex_width = 0;
  int tex_height = 0;
  int tex_channels = 0;
  stbi_uc* pixels = stbi_load(filepath.c_str(), &tex_width, &tex_height,
                              &tex_channels, STBI_rgb_alpha);
  vk::DeviceSize size = static_cast<vk::DeviceSize>(tex_width) *
                        static_cast<vk::DeviceSize>(tex_height) * 4;

  if (pixels == nullptr) {
    throw std::runtime_error("[ERROR] STB: failed to load elevation texture!");
  }
  vk::MemoryPropertyFlags staging_properties =
      vk::MemoryPropertyFlagBits::eHostVisible |
      vk::MemoryPropertyFlagBits::eHostCoherent;
  resources::AllocatedBuffer staging_buffer =
      resources::BufferAllocator::CreateBuffer(
          vulkan_device, size, vk::BufferUsageFlagBits::eTransferSrc,
          staging_properties);

  void* data = staging_buffer.memory.mapMemory(0, size);
  std::memcpy(data, pixels, size);
  staging_buffer.memory.unmapMemory();

  stbi_image_free(pixels);

  const std::array queue_family_indices{
      vulkan_device.ComputeQueueFamilyIndex(),
      vulkan_device.GraphicsQueueFamilyIndex()};
  vk::ImageCreateInfo image_info{
      .imageType = vk::ImageType::e2D,
      .format = vk::Format::eR8G8B8A8Unorm,
      .extent = {.width = static_cast<uint32_t>(tex_width),
                 .height = static_cast<uint32_t>(tex_height),
                 .depth = 1},
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = vk::SampleCountFlagBits::e1,
      .tiling = vk::ImageTiling::eOptimal,
      .usage = vk::ImageUsageFlagBits::eTransferDst |
               vk::ImageUsageFlagBits::eSampled,
      .sharingMode = vk::SharingMode::eConcurrent,
      .queueFamilyIndexCount =
          static_cast<uint32_t>(queue_family_indices.size()),
      .pQueueFamilyIndices = queue_family_indices.data(),
      .initialLayout = vk::ImageLayout::eUndefined};
  vk::raii::Image image{vulkan_device.Device(), image_info};
  vk::MemoryRequirements mem_requirements = image.getMemoryRequirements();
  vk::MemoryPropertyFlags image_properties =
      vk::MemoryPropertyFlagBits::eDeviceLocal;
  vk::MemoryAllocateInfo alloc_info{
      .allocationSize = mem_requirements.size,
      .memoryTypeIndex = resources::MemoryAllocator::FindMemoryType(
          vulkan_device, mem_requirements.memoryTypeBits, image_properties)};
  vk::raii::DeviceMemory memory{vulkan_device.Device(), alloc_info};
  image.bindMemory(memory, 0);

  resources::AllocatedImage alloc_image{.memory = std::move(memory),
                                        .image = std::move(image)};

  resources::ImmediateSubmit immediate_submit;
  immediate_submit.TransitionImageLayout(
      vulkan_device, command_pools, alloc_image, vk::ImageLayout::eUndefined,
      vk::ImageLayout::eTransferDstOptimal);
  immediate_submit.CopyBufferToImage(vulkan_device, command_pools,
                                     staging_buffer, alloc_image, tex_width,
                                     tex_height);
  immediate_submit.TransitionImageLayout(
      vulkan_device, command_pools, alloc_image,
      vk::ImageLayout::eTransferDstOptimal,
      vk::ImageLayout::eShaderReadOnlyOptimal);

  vk::ImageViewCreateInfo view_info{
      .image = alloc_image.image,
      .viewType = vk::ImageViewType::e2D,
      .format = vk::Format::eR8G8B8A8Unorm,
      .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                           .baseMipLevel = 0,
                           .levelCount = 1,
                           .baseArrayLayer = 0,
                           .layerCount = 1}};
  alloc_image.image_view =
      vk::raii::ImageView(vulkan_device.Device(), view_info);

  vk::SamplerCreateInfo sampler_info{
      .magFilter = vk::Filter::eLinear,
      .minFilter = vk::Filter::eLinear,
      .mipmapMode = vk::SamplerMipmapMode::eLinear,
      .addressModeU = vk::SamplerAddressMode::eClampToEdge,
      .addressModeV = vk::SamplerAddressMode::eClampToEdge,
      .addressModeW = vk::SamplerAddressMode::eClampToEdge,
      .mipLodBias = 0.0F,
      .anisotropyEnable = vk::False,
      .compareEnable = vk::False,
      .compareOp = vk::CompareOp::eAlways};
  alloc_image.sampler = vk::raii::Sampler(vulkan_device.Device(), sampler_info);

  return alloc_image;
}
