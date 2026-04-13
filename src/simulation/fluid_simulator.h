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
    glm::uvec4 bucket_size;
  } __attribute__((aligned(32)));

  struct UniformBufferObject {
    uint32_t voxel_max_particles;
    uint32_t fluid_particle_count;
    uint32_t wall_particle_count;
    uint32_t total_particle_count;
    glm::uvec4 bucket_size;
    glm::vec4 min_bound;
    glm::vec4 max_bound;
  } __attribute__((aligned(64)));

  struct FluidParticle {
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 distance_traveled;
    glm::vec4 color;
    float density;

    static vk::VertexInputBindingDescription GetBindingDescription();
    static std::vector<vk::VertexInputAttributeDescription>
    GetAttributeDescriptions();
  } __attribute__((aligned(64)));

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
  std::vector<FluidParticle> fluid_particles_;
  std::vector<WallParticle> wall_particles_;

  // Bucket Shader
  std::vector<uint32_t> bucket_;
  vk::raii::CommandBuffer bucket_primary_command_buffer_ = nullptr;
  vk::raii::CommandBuffer clear_bucket_secondary_command_buffer_ = nullptr;
  vk::raii::CommandBuffer fluid_bucket_secondary_command_buffer_ = nullptr;
  vk::raii::CommandBuffer wall_bucket_secondary_command_buffer_ = nullptr;
  vk::raii::PipelineLayout bucket_pipeline_layout_ = nullptr;
  vk::raii::Pipeline clear_bucket_pipeline_ = nullptr;
  vk::raii::Pipeline fluid_bucket_pipeline_ = nullptr;
  vk::raii::Pipeline wall_bucket_pipeline_ = nullptr;
  vk::raii::DescriptorSetLayout bucket_descriptor_set_layout_ = nullptr;
  resources::DescriptorAllocator bucket_descriptor_allocator_;
  resources::AllocatedBuffer uniform_buffer_;
  std::array<resources::AllocatedBuffer, 2> fluid_particles_buffers_;
  resources::AllocatedBuffer wall_particles_buffer_;
  resources::AllocatedBuffer bucket_buffer_;
  vk::raii::DescriptorSet bucket_descriptor_set_ = nullptr;
  size_t read_index_ = 0;
  size_t write_index_ = 1;

  void SwapParticleBufferIndices() {
    std::swap(read_index_, write_index_);
  }

  void CreateBucketDescriptorSetLayout(core::VulkanDevice const& vulkan_device);
  void CreateBucketPipelines(core::VulkanDevice const& vulkan_device);
  void CreateBuffers(core::VulkanDevice const& vulkan_device,
                     core::CommandPools const& command_pools);
  void CreateFluidBucketDescriptorSets(
      core::VulkanDevice const& vulkan_device,
      resources::DescriptorAllocator const& descriptor_allocator);
  void CreateBucketCommandBuffers(core::VulkanDevice const& vulkan_device,
                                  core::CommandPools const& command_pools);

  void RecordBucketPrimaryCommandBuffer();
  void RecordClearBucketCommandBuffer();
  void RecordFluidBucketCommandBuffer();
  void RecordWallBucketCommandBuffer();
};

}  // namespace simulation

#endif  // !RHEOSPH_FLUID_SIMULATOR_H
