#include "fluid_renderer.h"

#include <vulkan/vulkan_raii.hpp>

#include "../core/pipeline.h"
#include "../simulation/fluid_simulator.h"

namespace {}  // namespace

void renderer::FluidRenderer::Init(
    core::VulkanDevice const& vulkan_device,
    core::VulkanSwapChain const& vulkan_swap_chain) {
  CreateGraphicsPipeline(vulkan_device, vulkan_swap_chain);
}

void renderer::FluidRenderer::Render(
    vk::raii::CommandBuffer const& command_buffer,
    core::VulkanSwapChain& vulkan_swap_chain,
    simulation::FluidSimulator const* fluid_simulator) {
  command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                              *graphics_pipeline_);
  command_buffer.setViewport(
      0, vk::Viewport(0.0F, 0.0F,
                      static_cast<float>(vulkan_swap_chain.Extent().width),
                      static_cast<float>(vulkan_swap_chain.Extent().height),
                      0.0F, 1.0F));
  command_buffer.setScissor(
      0, vk::Rect2D(vk::Offset2D(0, 0), vulkan_swap_chain.Extent()));
  if (fluid_simulator != nullptr) {
    command_buffer.bindVertexBuffers(
        0, {fluid_simulator->FluidParticlesReadBuffer().buffer}, {0});
    command_buffer.draw(fluid_simulator->FluidParticleCount(), 1, 0, 0);
  }
}

void renderer::FluidRenderer::CreateGraphicsPipeline(
    core::VulkanDevice const& vulkan_device,
    core::VulkanSwapChain const& vulkan_swap_chain) {
  auto binding_description =
      simulation::FluidSimulator::FluidParticle::GetBindingDescription();
  auto attribute_descriptions =
      simulation::FluidSimulator::FluidParticle::GetAttributeDescriptions();

  vk::PipelineLayoutCreateInfo pipeline_layout_info;
  graphics_pipeline_layout_ =
      vk::raii::PipelineLayout(vulkan_device.Device(), pipeline_layout_info);

  graphics_pipeline_ = core::PipelineBuilder::Graphics(
      vulkan_device, binding_description, attribute_descriptions,
      graphics_pipeline_layout_, vulkan_swap_chain,
      "shaders/graphics/particle.spv");
}
