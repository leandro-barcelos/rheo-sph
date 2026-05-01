#define STB_IMAGE_IMPLEMENTATION
#include "images.h"

#include <gdal.h>
#include <gdal_priv.h>

#include <array>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <vector>
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
  vk::DeviceSize size = 0;

  if (pixels == nullptr) {
    // STB failed to load — if file is TIFF, try GDAL as a fallback to support
    // GeoTIFF / TIFF images which STB may not handle reliably.
    std::filesystem::path path(filepath);
    std::string ext = path.extension().string();
    if (!ext.empty()) {
      for (auto& character : ext) {
        character = static_cast<char>(std::tolower(character));
      }
    }
    if (ext == ".tif" || ext == ".tiff") {
      GDALAllRegister();
      auto* dataset = reinterpret_cast<GDALDataset*>(
          GDALOpen(filepath.c_str(), GA_ReadOnly));
      if (dataset != nullptr) {
        int width = GDALGetRasterXSize(dataset);
        int height = GDALGetRasterYSize(dataset);
        int bands = GDALGetRasterCount(dataset);
        if (width > 0 && height > 0) {
          tex_width = width;
          tex_height = height;
          std::vector<uint8_t> rgba(static_cast<size_t>(width) * height * 4, 255);
          std::vector<uint8_t> bandbuf(static_cast<size_t>(width) * height);

          if (bands >= 3) {
            for (int image_band = 1; image_band <= 3; ++image_band) {
              GDALRasterBand* band = dataset->GetRasterBand(image_band);
              CPLErr err = band->RasterIO(GF_Read, 0, 0, width, height, bandbuf.data(),
                                          width, height, GDT_Byte, 0, 0);
              if (err != CE_None) {
                GDALClose(dataset);
                throw std::runtime_error(
                    "[ERROR] GDAL: failed reading TIFF band");
              }
              for (size_t i = 0; i < static_cast<size_t>(width) * height; ++i) {
                rgba[(4 * i) + (image_band - 1)] = bandbuf[i];
              }
            }
          } else if (bands == 1) {
            GDALRasterBand* band = dataset->GetRasterBand(1);
            CPLErr err = band->RasterIO(GF_Read, 0, 0, width, height, bandbuf.data(), width,
                                        height, GDT_Byte, 0, 0);
            if (err != CE_None) {
              GDALClose(dataset);
              throw std::runtime_error(
                  "[ERROR] GDAL: failed reading TIFF band");
            }
            for (size_t i = 0; i < static_cast<size_t>(width) * height; ++i) {
              rgba[(4 * i) + 0] = bandbuf[i];
              rgba[(4 * i) + 1] = bandbuf[i];
              rgba[(4 * i) + 2] = bandbuf[i];
            }
          } else {
            GDALClose(dataset);
            throw std::runtime_error("[ERROR] GDAL: TIFF has no raster bands");
          }

          // Allocate pixels and copy RGBA data so rest of pipeline can use it.
          pixels = static_cast<stbi_uc*>(malloc(rgba.size())); // NOLINT
          if (pixels == nullptr) {
            GDALClose(dataset);
            throw std::runtime_error("[ERROR] malloc failed for TIFF pixels");
          }
          std::memcpy(pixels, rgba.data(), rgba.size());
          size = static_cast<vk::DeviceSize>(rgba.size());
          GDALClose(dataset);
        }
      }
    }

    if (pixels == nullptr) {
      throw std::runtime_error(
          "[ERROR] STB: failed to load elevation texture!");
    }
  } else {
    size = static_cast<vk::DeviceSize>(tex_width) *
           static_cast<vk::DeviceSize>(tex_height) * 4;
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

resources::AllocatedImage resources::ImageAllocator::CreateSolidColorImage(
    core::VulkanDevice const& vulkan_device,
    core::CommandPools const& command_pools, uint8_t red, uint8_t green,
    uint8_t blue, uint8_t alpha) {
  // Create a 1x1 RGBA image with the given color.
  std::array<uint8_t, 4> pixel = {red, green, blue, alpha};
  vk::DeviceSize size = 4;

  vk::MemoryPropertyFlags staging_properties =
      vk::MemoryPropertyFlagBits::eHostVisible |
      vk::MemoryPropertyFlagBits::eHostCoherent;
  resources::AllocatedBuffer staging_buffer =
      resources::BufferAllocator::CreateBuffer(
          vulkan_device, size, vk::BufferUsageFlagBits::eTransferSrc,
          staging_properties);

  void* data = staging_buffer.memory.mapMemory(0, size);
  std::memcpy(data, pixel.data(), static_cast<size_t>(size));
  staging_buffer.memory.unmapMemory();

  const std::array queue_family_indices{
      vulkan_device.ComputeQueueFamilyIndex(),
      vulkan_device.GraphicsQueueFamilyIndex()};
  vk::ImageCreateInfo image_info{
      .imageType = vk::ImageType::e2D,
      .format = vk::Format::eR8G8B8A8Unorm,
      .extent = {.width = 1U, .height = 1U, .depth = 1U},
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
                                     staging_buffer, alloc_image, 1, 1);
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
