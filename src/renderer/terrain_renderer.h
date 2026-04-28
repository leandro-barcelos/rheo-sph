#ifndef RHEOSPH_TERRAIN_RENDERER_H
#define RHEOSPH_TERRAIN_RENDERER_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "../core/vulkan_device.h"
#include "../core/vulkan_swap_chain.h"
#include "../simulation/fluid_simulator.h"
#include "rheo-sph/src/renderer/camera.h"
#include "rheo-sph/src/resources/buffer.h"
#include "rheo-sph/src/resources/descriptor.h"

namespace renderer {

class TerrainRenderer {
 public:
  void Init(core::VulkanDevice const& vulkan_device,
            core::VulkanSwapChain const& vulkan_swap_chain,
            core::CommandPools const& command_pools, uint32_t elevation_width,
            uint32_t elevation_height);
  void Render(vk::raii::CommandBuffer const& command_buffer,
              core::VulkanSwapChain& vulkan_swap_chain,
              simulation::FluidSimulator const* fluid_simulator,
              renderer::Camera const& camera);

 private:
  struct CameraUBO {  // NOLINT(altera-struct-pack-align)
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
  } __attribute__((aligned(16)));

  bool is_initialized_ = false;
  vk::raii::PipelineLayout graphics_pipeline_layout_ = nullptr;
  vk::raii::Pipeline graphics_pipeline_ = nullptr;
  resources::AllocatedBuffer camera_ubo_buffer_;
  resources::DescriptorAllocator camera_descriptor_allocator_;
  vk::raii::DescriptorSetLayout camera_descriptor_set_layout_ = nullptr;
  vk::raii::DescriptorSet camera_descriptor_set_ = nullptr;
  std::vector<uint32_t> indices_;
  resources::AllocatedBuffer indices_buffer_;

  void CreateGraphicsPipeline(core::VulkanDevice const& vulkan_device,
                              core::VulkanSwapChain const& vulkan_swap_chain);
  void CreateCameraDescriptorSetLayout(core::VulkanDevice const& vulkan_device);
  void CreateBuffers(core::VulkanDevice const& vulkan_device,
                     core::CommandPools const& command_pools);
  void CreateCameraDescriptorSet(
      core::VulkanDevice const& vulkan_device,
      resources::DescriptorAllocator const& descriptor_allocator);
  void UpdateUniformBuffer(core::VulkanSwapChain const& vulkan_swap_chain,
                           renderer::Camera const& camera);
  void CreateIndices(uint32_t width, uint32_t height);
};

}  // namespace renderer

#endif  // !RHEOSPH_TERRAIN_RENDERER_H
