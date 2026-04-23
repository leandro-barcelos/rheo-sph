#ifndef RHEOSPH_FLUID_RENDERER_H
#define RHEOSPH_FLUID_RENDERER_H

#include <vulkan/vulkan_raii.hpp>

#include "../core/vulkan_device.h"
#include "../core/vulkan_swap_chain.h"
#include "../simulation/fluid_simulator.h"
#include "rheo-sph/src/renderer/camera.h"
#include "rheo-sph/src/resources/buffer.h"
#include "rheo-sph/src/resources/descriptor.h"

namespace renderer {

class FluidRenderer {
 public:
  void Init(core::VulkanDevice const& vulkan_device,
            core::VulkanSwapChain const& vulkan_swap_chain);
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

  vk::raii::PipelineLayout graphics_pipeline_layout_ = nullptr;
  vk::raii::Pipeline graphics_pipeline_ = nullptr;
  resources::AllocatedBuffer camera_ubo_buffer_;
  resources::DescriptorAllocator camera_descriptor_allocator_;
  vk::raii::DescriptorSetLayout camera_descriptor_set_layout_ = nullptr;
  vk::raii::DescriptorSet camera_descriptor_set_ = nullptr;

  void CreateGraphicsPipeline(core::VulkanDevice const& vulkan_device,
                              core::VulkanSwapChain const& vulkan_swap_chain);
  void CreateCameraDescriptorSetLayout(core::VulkanDevice const& vulkan_device);
  void CreateBuffers(core::VulkanDevice const& vulkan_device);
  void CreateCameraDescriptorSet(
      core::VulkanDevice const& vulkan_device,
      resources::DescriptorAllocator const& descriptor_allocator);
  void UpdateUniformBuffer(core::VulkanSwapChain const& vulkan_swap_chain,
                           renderer::Camera const& camera);
};

}  // namespace renderer

#endif  // !RHEOSPH_FLUID_RENDERER_H
