#ifndef RHEOSPH_FLUID_RENDERER_H
#define RHEOSPH_FLUID_RENDERER_H

#include <vulkan/vulkan_raii.hpp>

#include "../core/vulkan_device.h"
#include "../core/vulkan_swap_chain.h"
#include "../simulation/fluid_simulator.h"

namespace renderer {

class FluidRenderer {
 public:
  void Init(core::VulkanDevice const& vulkan_device,
            core::VulkanSwapChain const& vulkan_swap_chain);
  void Render(vk::raii::CommandBuffer const& command_buffer,
              core::VulkanSwapChain& vulkan_swap_chain,
              simulation::FluidSimulator const* fluid_simulator);

 private:
  vk::raii::PipelineLayout graphics_pipeline_layout_ = nullptr;
  vk::raii::Pipeline graphics_pipeline_ = nullptr;

  void CreateGraphicsPipeline(core::VulkanDevice const& vulkan_device,
                              core::VulkanSwapChain const& vulkan_swap_chain);
};

}  // namespace renderer

#endif  // !RHEOSPH_FLUID_RENDERER_H
