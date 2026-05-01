#include "buffer.h"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

#include "../simulation/fluid_simulator.h"  // IWYU pragma: keep
#include "immediate_submit.h"
#include "memory.h"

resources::AllocatedBuffer resources::BufferAllocator::CreateBuffer(
    core::VulkanDevice const& vulkan_device, vk::DeviceSize size,
    vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties) {
  std::vector<uint32_t> queue_family_indices{
      vulkan_device.ComputeQueueFamilyIndex(),
      vulkan_device.GraphicsQueueFamilyIndex()};

  vk::BufferCreateInfo buffer_info{
      .size = size,
      .usage = usage,
      .sharingMode = vk::SharingMode::eConcurrent,
      .queueFamilyIndexCount =
          static_cast<uint32_t>(queue_family_indices.size()),
      .pQueueFamilyIndices = queue_family_indices.data(),
  };

  vk::raii::Buffer buffer{vulkan_device.Device(), buffer_info};
  vk::MemoryRequirements mem_requirements = buffer.getMemoryRequirements();
  vk::MemoryAllocateInfo alloc_info{
      .allocationSize = mem_requirements.size,
      .memoryTypeIndex = MemoryAllocator::FindMemoryType(
          vulkan_device, mem_requirements.memoryTypeBits, properties)};
  vk::raii::DeviceMemory memory{vulkan_device.Device(), alloc_info};
  buffer.bindMemory(memory, 0);

  return resources::AllocatedBuffer{
      .memory = std::move(memory),
      .buffer = std::move(buffer),
  };
}

resources::AllocatedBuffer
resources::BufferAllocator::CreateMappedUniformBuffer(
    core::VulkanDevice const& vulkan_device, vk::DeviceSize size) {
  auto uniform_buffer =
      CreateBuffer(vulkan_device, size, vk::BufferUsageFlagBits::eUniformBuffer,
                   vk::MemoryPropertyFlagBits::eHostVisible |
                       vk::MemoryPropertyFlagBits::eHostCoherent);
  uniform_buffer.mapped = uniform_buffer.memory.mapMemory(0, size);
  return uniform_buffer;
}

template <typename T>
resources::AllocatedBuffer resources::BufferAllocator::CreateUniformBuffer(
    core::VulkanDevice const& vulkan_device,
    core::CommandPools const& command_pools, T const& data) {
  vk::DeviceSize buffer_size = sizeof(T);

  resources::AllocatedBuffer staging_buffer = CreateBuffer(
      vulkan_device, buffer_size, vk::BufferUsageFlagBits::eTransferSrc,
      vk::MemoryPropertyFlagBits::eHostVisible |
          vk::MemoryPropertyFlagBits::eHostCoherent);

  void* data_staging = staging_buffer.memory.mapMemory(0, buffer_size);
  memcpy(data_staging, &data, (size_t)buffer_size);
  staging_buffer.memory.unmapMemory();

  resources::AllocatedBuffer storage_buffer =
      CreateBuffer(vulkan_device, buffer_size,
                   vk::BufferUsageFlagBits::eUniformBuffer |
                       vk::BufferUsageFlagBits::eTransferDst,
                   vk::MemoryPropertyFlagBits::eDeviceLocal);
  ImmediateSubmit single_time_command;
  single_time_command.CopyBuffer(vulkan_device, command_pools, staging_buffer,
                                 storage_buffer, buffer_size);

  return storage_buffer;
}

template <typename T>
std::array<resources::AllocatedBuffer, 2>
resources::BufferAllocator::CreateSSBO(core::VulkanDevice const& vulkan_device,
                                       core::CommandPools const& command_pools,
                                       std::vector<T> const& objects,
                                       bool double_buffering,
                                       vk::BufferUsageFlags extra_usage_flags) {
  if (objects.empty()) {
    throw std::runtime_error(
        "[ERROR] Vulkan: Tried creating a buffer with size 0!");
  }

  vk::DeviceSize size = sizeof(T) * objects.size();

  resources::AllocatedBuffer staging_buffer =
      CreateBuffer(vulkan_device, size, vk::BufferUsageFlagBits::eTransferSrc,
                   vk::MemoryPropertyFlagBits::eHostVisible |
                       vk::MemoryPropertyFlagBits::eHostCoherent);

  void* data_staging = staging_buffer.memory.mapMemory(0, size);
  std::memcpy(data_staging, objects.data(), static_cast<size_t>(size));
  staging_buffer.memory.unmapMemory();

  std::array<resources::AllocatedBuffer, 2> storage_buffers;
  for (size_t i = 0; i < 2; i++) {
    if (i == 1 && !double_buffering) {
      storage_buffers.at(i) = {.memory = nullptr, .buffer = nullptr};
      continue;
    }

    storage_buffers.at(i) = CreateBuffer(
        vulkan_device, size,
        vk::BufferUsageFlagBits::eStorageBuffer |
            vk::BufferUsageFlagBits::eTransferDst | extra_usage_flags,
        vk::MemoryPropertyFlagBits::eDeviceLocal);

    ImmediateSubmit single_time_command;
    single_time_command.CopyBuffer(vulkan_device, command_pools, staging_buffer,
                                   storage_buffers.at(i), size);
  }

  return storage_buffers;
}

template resources::AllocatedBuffer resources::BufferAllocator::
    CreateUniformBuffer<simulation::FluidSimulator::UniformBufferObject>(
        core::VulkanDevice const& vulkan_device,
        core::CommandPools const& command_pools,
        simulation::FluidSimulator::UniformBufferObject const& data);

template std::array<resources::AllocatedBuffer, 2> resources::BufferAllocator::
    CreateSSBO<simulation::FluidSimulator::FluidParticle>(
        core::VulkanDevice const& vulkan_device,
        core::CommandPools const& command_pools,
        std::vector<simulation::FluidSimulator::FluidParticle> const& objects,
        bool double_buffering, vk::BufferUsageFlags extra_usage_flags);

template std::array<resources::AllocatedBuffer, 2> resources::BufferAllocator::
    CreateSSBO<simulation::FluidSimulator::WallParticle>(
        core::VulkanDevice const& vulkan_device,
        core::CommandPools const& command_pools,
        std::vector<simulation::FluidSimulator::WallParticle> const& objects,
        bool double_buffering, vk::BufferUsageFlags extra_usage_flags);

template std::array<resources::AllocatedBuffer, 2>
resources::BufferAllocator::CreateSSBO<uint32_t>(
    core::VulkanDevice const& vulkan_device,
    core::CommandPools const& command_pools,
    std::vector<uint32_t> const& objects, bool double_buffering,
    vk::BufferUsageFlags extra_usage_flags);

template std::array<resources::AllocatedBuffer, 2>
resources::BufferAllocator::CreateSSBO<resources::Elevation>(
    core::VulkanDevice const& vulkan_device,
    core::CommandPools const& command_pools,
    std::vector<resources::Elevation> const& objects, bool double_buffering,
    vk::BufferUsageFlags extra_usage_flags);
