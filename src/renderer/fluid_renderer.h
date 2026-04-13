#ifndef RHEOSPH_FLUID_RENDERER_H
#define RHEOSPH_FLUID_RENDERER_H

#include <vulkan/vulkan_raii.hpp>

#include "../core/command_pool.h"
#include "../core/vulkan_device.h"
#include "../core/vulkan_swap_chain.h"
#include "../simulation/fluid_simulator.h"

namespace renderer {

class FluidRenderer {
 public:
  void Init(core::VulkanDevice const& vulkan_device,
            core::VulkanSwapChain const& vulkan_swap_chain,
            core::CommandPools const& command_pool);
  void Render(core::VulkanDevice const& vulkan_device,
              core::VulkanSwapChain& vulkan_swap_chain,
              core::FrameSync& frame_sync,
              simulation::FluidSimulator const& fluid_simulator,
              uint32_t image_index, core::Window const& window,
              uint64_t simulation_signal_value);

 private:
  bool framebuffer_resized_ = false;
  vk::raii::PipelineLayout graphics_pipeline_layout_ = nullptr;
  vk::raii::Pipeline graphics_pipeline_ = nullptr;
  vk::raii::CommandBuffer graphics_command_buffer_ = nullptr;

  void CreateGraphicsPipeline(core::VulkanDevice const& vulkan_device,
                              core::VulkanSwapChain const& vulkan_swap_chain);
  void CreateGraphicsCommandBuffer(core::VulkanDevice const& vulkan_device,
                                   core::CommandPools const& command_pools);
  void RecordGraphicsCommandBuffer(
      core::VulkanSwapChain& vulkan_swap_chain, uint32_t image_index,
      simulation::FluidSimulator const& fluid_simulator);
  void TransitionImageLayout(core::VulkanSwapChain const& vulkan_swap_chain,
                             uint32_t image_index, vk::ImageLayout old_layout,
                             vk::ImageLayout new_layout,
                             vk::AccessFlags2 src_access_mask,
                             vk::AccessFlags2 dst_access_mask,
                             vk::PipelineStageFlags2 src_stage_mask,
                             vk::PipelineStageFlags2 dst_stage_mask);
};

}  // namespace renderer

#endif  // !RHEOSPH_FLUID_RENDERER_H
