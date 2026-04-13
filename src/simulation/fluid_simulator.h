#ifndef RHEOSPH_FLUID_SIMULATOR_H
#define RHEOSPH_FLUID_SIMULATOR_H

#include <cstdint>
#include <glm/glm.hpp>
#include <utility>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

#include "../core/frame_sync.h"
#include "../core/vulkan_device.h"
#include "../resources/buffer.h"
#include "../resources/descriptor.h"

namespace simulation {

constexpr uint32_t kNumThreads = 256;
constexpr uint32_t kClearBucketsNumThreads = 10;

class FluidSimulator {
 public:
  struct Parameters {
    uint32_t voxel_max_particles;
    uint32_t fluid_particle_count;
    float rest_density;
    float total_fluid_volume;
    glm::uvec4 bucket_size;
  } __attribute__((aligned(32)));

  struct UniformBufferObject {  // NOLINT(altera-struct-pack-align)
    uint32_t voxel_max_particles;
    uint32_t fluid_particle_count;
    uint32_t wall_particle_count;
    uint32_t total_particle_count;
    float rest_density;
    float particle_mass;
    float effective_radius_2;
    float effective_radius_9;
    glm::uvec4 bucket_size;
    glm::vec4 min_bound;
    glm::vec4 max_bound;
  } __attribute__((aligned(16)));

  struct FluidParticle {  // NOLINT(altera-struct-pack-align)
    glm::vec4 position;
    glm::vec4 velocity;
    glm::vec4 distance_traveled;
    glm::vec4 color;
    float density;

    static vk::VertexInputBindingDescription GetBindingDescription();
    static std::vector<vk::VertexInputAttributeDescription>
    GetAttributeDescriptions();
  } __attribute__((aligned(16)));

  struct WallParticle {
    glm::vec3 position;
  } __attribute__((aligned(16)));

  explicit FluidSimulator(Parameters const& parameters);

  void Init(core::VulkanDevice const& vulkan_device,
            core::CommandPools const& command_pools);
  [[nodiscard]] uint64_t Run(core::VulkanDevice const& vulkan_device,
                             core::FrameSync& frame_sync);

  [[nodiscard]] resources::AllocatedBuffer const& FluidParticlesReadBuffer()
      const {
    return fluid_particles_buffers_.at(read_index_);
  }
  [[nodiscard]] resources::AllocatedBuffer const& FluidParticlesWriteBuffer()
      const {
    return fluid_particles_buffers_.at(write_index_);
  }
  [[nodiscard]] uint32_t FluidParticleCount() const {
    return uniform_buffer_data_.fluid_particle_count;
  }

 private:
  UniformBufferObject uniform_buffer_data_;
  std::vector<uint32_t> bucket_;
  std::vector<FluidParticle> fluid_particles_;
  std::vector<WallParticle> wall_particles_;
  resources::AllocatedBuffer uniform_buffer_;
  std::array<resources::AllocatedBuffer, 2> fluid_particles_buffers_;
  resources::AllocatedBuffer wall_particles_buffer_;
  resources::AllocatedBuffer bucket_buffer_;

  size_t read_index_ = 0;
  size_t write_index_ = 1;

  void SwapParticleBufferIndices() { std::swap(read_index_, write_index_); }

  // Bucket Shader
  vk::raii::DescriptorSetLayout bucket_descriptor_set_layout_ = nullptr;
  vk::raii::PipelineLayout bucket_pipeline_layout_ = nullptr;
  vk::raii::Pipeline clear_bucket_pipeline_ = nullptr;
  vk::raii::Pipeline fluid_bucket_pipeline_ = nullptr;
  vk::raii::Pipeline wall_bucket_pipeline_ = nullptr;
  resources::DescriptorAllocator bucket_descriptor_allocator_;
  vk::raii::DescriptorSet bucket_descriptor_set_ = nullptr;
  vk::raii::CommandBuffer clear_bucket_secondary_command_buffer_ = nullptr;
  vk::raii::CommandBuffer fluid_bucket_secondary_command_buffer_ = nullptr;
  vk::raii::CommandBuffer wall_bucket_secondary_command_buffer_ = nullptr;
  vk::raii::CommandBuffer bucket_primary_command_buffer_ = nullptr;

  void CreateBucketDescriptorSetLayout(core::VulkanDevice const& vulkan_device);
  void CreateBucketPipelines(core::VulkanDevice const& vulkan_device);
  void CreateBuffers(core::VulkanDevice const& vulkan_device,
                     core::CommandPools const& command_pools);
  void CreateFluidBucketDescriptorSets(
      core::VulkanDevice const& vulkan_device,
      resources::DescriptorAllocator const& descriptor_allocator);
  void CreateBucketCommandBuffers(core::VulkanDevice const& vulkan_device,
                                  core::CommandPools const& command_pools);

  void RecordClearBucketCommandBuffer();
  void RecordFluidBucketCommandBuffer();
  void RecordWallBucketCommandBuffer();
  void RecordBucketPrimaryCommandBuffer();
  [[nodiscard]] uint64_t DispatchBucket(
      core::VulkanDevice const& vulkan_device, core::FrameSync& frame_sync,
      uint64_t wait_value);

  // Density Shader
  vk::raii::CommandBuffer density_command_buffer_ = nullptr;
  vk::raii::PipelineLayout density_pipeline_layout_ = nullptr;
  vk::raii::Pipeline density_pipeline_ = nullptr;
  vk::raii::DescriptorSetLayout density_descriptor_set_layout_ = nullptr;
  resources::DescriptorAllocator density_descriptor_allocator_;
  vk::raii::DescriptorSet density_descriptor_set_ = nullptr;

  void CreateDensityDescriptorSetLayout(
      core::VulkanDevice const& vulkan_device);
  void CreateDensityPipeline(core::VulkanDevice const& vulkan_device);
  void CreateDensityDescriptorSets(
      core::VulkanDevice const& vulkan_device,
      resources::DescriptorAllocator const& descriptor_allocator);
  void CreateDensityCommandBuffers(core::VulkanDevice const& vulkan_device,
                                   core::CommandPools const& command_pools);

  void RecordDensityCommandBuffer();
  [[nodiscard]] uint64_t DispatchDensity(
      core::VulkanDevice const& vulkan_device, core::FrameSync& frame_sync,
      uint64_t wait_value);
};

}  // namespace simulation

static_assert(sizeof(simulation::FluidSimulator::UniformBufferObject) == 80);
static_assert(sizeof(simulation::FluidSimulator::FluidParticle) == 80);
static_assert(sizeof(simulation::FluidSimulator::WallParticle) == 16);

#endif  // !RHEOSPH_FLUID_SIMULATOR_H
