#include "fluid_simulator.h"

#include <array>
#include <cmath>
#include <cstddef>

#include "../core/pipeline.h"
#include "vulkan/vulkan.hpp"

vk::VertexInputBindingDescription
simulation::FluidSimulator::FluidParticle::GetBindingDescription() {
  return {.binding = 0,
          .stride = sizeof(FluidParticle),
          .inputRate = vk::VertexInputRate::eVertex};
}

std::vector<vk::VertexInputAttributeDescription>
simulation::FluidSimulator::FluidParticle::GetAttributeDescriptions() {
  return {
      vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat,
                                          offsetof(FluidParticle, position)),
      vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat,
                                          offsetof(FluidParticle, velocity)),
      vk::VertexInputAttributeDescription(2, 0, vk::Format::eR32Sfloat,
                                          offsetof(FluidParticle, density)),
  };
}

simulation::FluidSimulator::FluidSimulator(Parameters const& parameters)
    : uniform_buffer_data_{} {
  const uint32_t requested_fluid_particle_count =
      parameters.fluid_particle_count;

  uint32_t grid_resolution = 2;
  if (requested_fluid_particle_count > 0) {
    grid_resolution =
        static_cast<uint32_t>(std::ceil(
            std::cbrt(static_cast<double>(requested_fluid_particle_count)))) +
        2;
  }

  const auto grid_resolution_size = static_cast<std::size_t>(grid_resolution);
  const auto interior_resolution_size =
      static_cast<std::size_t>(grid_resolution - 2);
  const std::size_t total_grid_particles =
      (grid_resolution_size * grid_resolution_size * grid_resolution_size);
  const std::size_t interior_grid_particles =
      (interior_resolution_size * interior_resolution_size *
       interior_resolution_size);
  const std::size_t max_wall_count =
      total_grid_particles - interior_grid_particles;
  fluid_particles_.reserve(requested_fluid_particle_count);
  wall_particles_.reserve(max_wall_count);

  const float step = 1.0F / static_cast<float>(grid_resolution - 1);
  for (uint32_t z_index = 0; z_index < grid_resolution; ++z_index) {
    for (uint32_t y_index = 0; y_index < grid_resolution; ++y_index) {
      for (uint32_t x_index = 0; x_index < grid_resolution; ++x_index) {
        const glm::vec3 position{static_cast<float>(x_index) * step,
                                 static_cast<float>(y_index) * step,
                                 static_cast<float>(z_index) * step};
        const bool is_edge = x_index == 0 || x_index == (grid_resolution - 1) ||
                             y_index == 0 || y_index == (grid_resolution - 1) ||
                             z_index == 0 || z_index == (grid_resolution - 1);
        if (is_edge) {
          wall_particles_.emplace_back(WallParticle{.position = position});
          continue;
        }

        if (fluid_particles_.size() < requested_fluid_particle_count) {
          fluid_particles_.emplace_back(
              FluidParticle{.position = position,
                            .velocity = {0.0F, 0.0F, 0.0F},
                            .distance_traveled = {0.0F, 0.0F, 0.0F},
                            .color = {0.2F, 0.6F, 1.0F, 1.0F},
                            .density = 0.0F});
        }
      }
    }
  }

  uniform_buffer_data_ = {
      .voxel_max_particles = parameters.voxel_max_particles,
      .fluid_particle_count = static_cast<uint32_t>(fluid_particles_.size()),
      .wall_particle_count = static_cast<uint32_t>(wall_particles_.size()),
      .total_particle_count = static_cast<uint32_t>(fluid_particles_.size() +
                                                    wall_particles_.size()),
      .bucket_size = parameters.bucket_size,
      .min_bound = {0.0F, 0.0F, 0.0F, 0.0F},
      .max_bound = {1.0F, 1.0F, 1.0F, 0.0F}};

  bucket_.resize(
      static_cast<std::size_t>(uniform_buffer_data_.bucket_size[0]) *
      static_cast<std::size_t>(uniform_buffer_data_.bucket_size[1]) *
      static_cast<std::size_t>(uniform_buffer_data_.bucket_size[2]) *
      static_cast<std::size_t>(uniform_buffer_data_.voxel_max_particles));
}

void simulation::FluidSimulator::Init(core::VulkanDevice const& vulkan_device,
                                      core::CommandPools const& command_pools) {
  CreateBucketDescriptorSetLayout(vulkan_device);
  const std::array descriptor_pool_sizes = {
      resources::DescriptorAllocator::PoolSize{
          .type = vk::DescriptorType::eUniformBuffer, .count = 1},
      resources::DescriptorAllocator::PoolSize{
          .type = vk::DescriptorType::eStorageBuffer, .count = 3}};
    bucket_descriptor_allocator_.Init(
      vulkan_device, 1, descriptor_pool_sizes);
  CreateBucketPipelines(vulkan_device);
  CreateBuffers(vulkan_device, command_pools);
    CreateFluidBucketDescriptorSets(vulkan_device, bucket_descriptor_allocator_);
  CreateBucketCommandBuffers(vulkan_device, command_pools);
}

uint64_t simulation::FluidSimulator::Run(
        core::VulkanDevice const& vulkan_device, core::FrameSync& frame_sync) {
    uint64_t bucket_wait_value = frame_sync.CurrentTimelineValue();
  uint64_t bucket_signal_value = frame_sync.GetNextTimelineValue();

  RecordClearBucketCommandBuffer();
  RecordFluidBucketCommandBuffer();
  RecordWallBucketCommandBuffer();
  RecordBucketPrimaryCommandBuffer();

  vk::TimelineSemaphoreSubmitInfo bucket_timeline_info{
      .waitSemaphoreValueCount = 1,
      .pWaitSemaphoreValues = &bucket_wait_value,
      .signalSemaphoreValueCount = 1,
      .pSignalSemaphoreValues = &bucket_signal_value};

  std::array<vk::PipelineStageFlags, 1> wait_stages{
      vk::PipelineStageFlagBits::eComputeShader};

  vk::SubmitInfo bucket_submit_info{
      .pNext = &bucket_timeline_info,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &*frame_sync.Semaphore(),
      .pWaitDstStageMask = wait_stages.data(),
      .commandBufferCount = 1,
      .pCommandBuffers = &*bucket_primary_command_buffer_,
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &*frame_sync.Semaphore()};

  vulkan_device.ComputeQueue().submit(bucket_submit_info, nullptr);

    SwapParticleBufferIndices();
    return bucket_signal_value;
}

void simulation::FluidSimulator::CreateBucketDescriptorSetLayout(
    core::VulkanDevice const& vulkan_device) {
  std::array layout_bindings{
      vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1,
                                     vk::ShaderStageFlagBits::eCompute,
                                     nullptr),
      vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1,
                                     vk::ShaderStageFlagBits::eCompute,
                                     nullptr),
      vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1,
                                     vk::ShaderStageFlagBits::eCompute,
                                     nullptr),
      vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eStorageBuffer, 1,
                                     vk::ShaderStageFlagBits::eCompute,
                                     nullptr)};

  vk::DescriptorSetLayoutCreateInfo layout_info{
      .bindingCount = static_cast<uint32_t>(layout_bindings.size()),
      .pBindings = layout_bindings.data()};
  bucket_descriptor_set_layout_ =
      vk::raii::DescriptorSetLayout(vulkan_device.Device(), layout_info);
}

void simulation::FluidSimulator::CreateBucketPipelines(
    core::VulkanDevice const& vulkan_device) {
  vk::PipelineLayoutCreateInfo pipeline_layout_info{
      .setLayoutCount = 1, .pSetLayouts = &*bucket_descriptor_set_layout_};
  bucket_pipeline_layout_ =
      vk::raii::PipelineLayout(vulkan_device.Device(), pipeline_layout_info);

  fluid_bucket_pipeline_ = core::PipelineBuilder::Compute(
      vulkan_device, bucket_pipeline_layout_, "shaders/compute/bucket.spv",
      "fluid_bucket");
  clear_bucket_pipeline_ = core::PipelineBuilder::Compute(
      vulkan_device, bucket_pipeline_layout_, "shaders/compute/bucket.spv",
      "clear_bucket");
  wall_bucket_pipeline_ = core::PipelineBuilder::Compute(
      vulkan_device, bucket_pipeline_layout_, "shaders/compute/bucket.spv",
      "wall_bucket");
}

void simulation::FluidSimulator::CreateBuffers(
    core::VulkanDevice const& vulkan_device,
    core::CommandPools const& command_pools) {
  uniform_buffer_ =
      resources::BufferAllocator::CreateUniformBuffer<UniformBufferObject>(
          vulkan_device, command_pools, uniform_buffer_data_);

  fluid_particles_buffers_ = resources::BufferAllocator::CreateSSBO(
      vulkan_device, command_pools, fluid_particles_, true,
      vk::BufferUsageFlagBits::eVertexBuffer);
  wall_particles_buffer_ = std::move(resources::BufferAllocator::CreateSSBO(
                                 vulkan_device, command_pools, wall_particles_,
                                 false)
                                 .front());
  bucket_buffer_ = std::move(resources::BufferAllocator::CreateSSBO(
                                 vulkan_device, command_pools, bucket_, false)
                                 .front());
}

void simulation::FluidSimulator::CreateFluidBucketDescriptorSets(
    core::VulkanDevice const& vulkan_device,
    resources::DescriptorAllocator const& descriptor_allocator) {
  bucket_descriptor_set_ = descriptor_allocator.Allocate(
      vulkan_device, bucket_descriptor_set_layout_);

  vk::DescriptorBufferInfo uniform_buffer_info(uniform_buffer_.buffer, 0,
                                               sizeof(UniformBufferObject));

  vk::DescriptorBufferInfo fluid_particles_buffer_info(
      fluid_particles_buffers_.at(read_index_).buffer, 0,
      sizeof(FluidParticle) * uniform_buffer_data_.fluid_particle_count);

  vk::DescriptorBufferInfo wall_particles_buffer_info(
      wall_particles_buffer_.buffer, 0,
      sizeof(WallParticle) * uniform_buffer_data_.wall_particle_count);

  vk::DescriptorBufferInfo bucket_buffer_info(
      bucket_buffer_.buffer, 0, sizeof(uint32_t) * bucket_.size());

  std::array descriptor_writes{
      vk::WriteDescriptorSet{
          .dstSet = *bucket_descriptor_set_,
          .dstBinding = 0,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = vk::DescriptorType::eUniformBuffer,
          .pImageInfo = nullptr,
          .pBufferInfo = &uniform_buffer_info,
          .pTexelBufferView = nullptr},
      vk::WriteDescriptorSet{
          .dstSet = *bucket_descriptor_set_,
          .dstBinding = 1,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = vk::DescriptorType::eStorageBuffer,
          .pImageInfo = nullptr,
          .pBufferInfo = &fluid_particles_buffer_info,
          .pTexelBufferView = nullptr},
      vk::WriteDescriptorSet{
          .dstSet = *bucket_descriptor_set_,
          .dstBinding = 2,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = vk::DescriptorType::eStorageBuffer,
          .pImageInfo = nullptr,
          .pBufferInfo = &wall_particles_buffer_info,
          .pTexelBufferView = nullptr},
      vk::WriteDescriptorSet{
          .dstSet = *bucket_descriptor_set_,
          .dstBinding = 3,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = vk::DescriptorType::eStorageBuffer,
          .pImageInfo = nullptr,
          .pBufferInfo = &bucket_buffer_info,
          .pTexelBufferView = nullptr},
  };

  vulkan_device.Device().updateDescriptorSets(descriptor_writes, {});
}

void simulation::FluidSimulator::CreateBucketCommandBuffers(
    core::VulkanDevice const& vulkan_device,
    core::CommandPools const& command_pools) {
  vk::CommandBufferAllocateInfo primary_allocate_info = {
      .commandPool = command_pools.Compute(),
      .level = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = 1};
  auto primary_command_buffers =
      vk::raii::CommandBuffers(vulkan_device.Device(), primary_allocate_info);
  bucket_primary_command_buffer_ = std::move(primary_command_buffers.front());

  vk::CommandBufferAllocateInfo secondary_allocate_info = {
      .commandPool = command_pools.Compute(),
      .level = vk::CommandBufferLevel::eSecondary,
      .commandBufferCount = 1};
  auto clear_command_buffers =
      vk::raii::CommandBuffers(vulkan_device.Device(), secondary_allocate_info);
  clear_bucket_secondary_command_buffer_ =
      std::move(clear_command_buffers.front());

  auto fluid_command_buffers =
      vk::raii::CommandBuffers(vulkan_device.Device(), secondary_allocate_info);
  fluid_bucket_secondary_command_buffer_ =
      std::move(fluid_command_buffers.front());

  auto wall_command_buffers =
      vk::raii::CommandBuffers(vulkan_device.Device(), secondary_allocate_info);
  wall_bucket_secondary_command_buffer_ =
      std::move(wall_command_buffers.front());
}

void simulation::FluidSimulator::RecordFluidBucketCommandBuffer() {
  vk::CommandBufferInheritanceInfo inheritance_info{};
  vk::CommandBufferBeginInfo begin_info{
      .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
      .pInheritanceInfo = &inheritance_info};

  fluid_bucket_secondary_command_buffer_.begin(begin_info);

  fluid_bucket_secondary_command_buffer_.bindPipeline(
      vk::PipelineBindPoint::eCompute, fluid_bucket_pipeline_);
  fluid_bucket_secondary_command_buffer_.bindDescriptorSets(
      vk::PipelineBindPoint::eCompute, bucket_pipeline_layout_, 0,
      {bucket_descriptor_set_}, {});
  fluid_bucket_secondary_command_buffer_.dispatch(
      (uniform_buffer_data_.fluid_particle_count + kNumThreads - 1) /
          kNumThreads,
      1, 1);

  fluid_bucket_secondary_command_buffer_.end();
}

void simulation::FluidSimulator::RecordClearBucketCommandBuffer() {
  clear_bucket_secondary_command_buffer_.reset();

  vk::CommandBufferInheritanceInfo inheritance_info{};
  vk::CommandBufferBeginInfo begin_info{
      .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
      .pInheritanceInfo = &inheritance_info};

  clear_bucket_secondary_command_buffer_.begin(begin_info);

  clear_bucket_secondary_command_buffer_.bindPipeline(
      vk::PipelineBindPoint::eCompute, clear_bucket_pipeline_);
  clear_bucket_secondary_command_buffer_.bindDescriptorSets(
      vk::PipelineBindPoint::eCompute, bucket_pipeline_layout_, 0,
      {bucket_descriptor_set_}, {});

  const uint32_t bucket_dispatch_x =
      (uniform_buffer_data_.bucket_size[0] + kClearBucketsNumThreads - 1) /
      kClearBucketsNumThreads;
  const uint32_t bucket_dispatch_y =
      (uniform_buffer_data_.bucket_size[1] + kClearBucketsNumThreads - 1) /
      kClearBucketsNumThreads;
  const uint32_t bucket_dispatch_z =
      (uniform_buffer_data_.bucket_size[2] + kClearBucketsNumThreads - 1) /
      kClearBucketsNumThreads;
  clear_bucket_secondary_command_buffer_.dispatch(
      bucket_dispatch_x, bucket_dispatch_y, bucket_dispatch_z);

  clear_bucket_secondary_command_buffer_.end();
}

void simulation::FluidSimulator::RecordBucketPrimaryCommandBuffer() {
  bucket_primary_command_buffer_.reset();

  vk::CommandBufferBeginInfo begin_info{};
  bucket_primary_command_buffer_.begin(begin_info);

  std::array<vk::CommandBuffer, 3> secondary_command_buffer{
      *clear_bucket_secondary_command_buffer_,
      *fluid_bucket_secondary_command_buffer_,
      *wall_bucket_secondary_command_buffer_};
  bucket_primary_command_buffer_.executeCommands(secondary_command_buffer);

  bucket_primary_command_buffer_.end();
}

void simulation::FluidSimulator::RecordWallBucketCommandBuffer() {
  wall_bucket_secondary_command_buffer_.reset();

  vk::CommandBufferInheritanceInfo inheritance_info{};
  vk::CommandBufferBeginInfo begin_info{
      .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
      .pInheritanceInfo = &inheritance_info};

  wall_bucket_secondary_command_buffer_.begin(begin_info);

  wall_bucket_secondary_command_buffer_.bindPipeline(
      vk::PipelineBindPoint::eCompute, wall_bucket_pipeline_);
  wall_bucket_secondary_command_buffer_.bindDescriptorSets(
      vk::PipelineBindPoint::eCompute, bucket_pipeline_layout_, 0,
      {bucket_descriptor_set_}, {});
  wall_bucket_secondary_command_buffer_.dispatch(
      (uniform_buffer_data_.wall_particle_count + kNumThreads - 1) /
          kNumThreads,
      1, 1);

  wall_bucket_secondary_command_buffer_.end();
}
