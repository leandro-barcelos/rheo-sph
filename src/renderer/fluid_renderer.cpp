#include "fluid_renderer.h"

#include "../core/pipeline.h"
#include "../simulation/fluid_simulator.h"

namespace {}  // namespace

void renderer::FluidRenderer::Init(
    core::VulkanDevice const& vulkan_device,
    core::VulkanSwapChain const& vulkan_swap_chain,
    core::CommandPools const& command_pools) {
  CreateGraphicsPipeline(vulkan_device, vulkan_swap_chain);
  CreateGraphicsCommandBuffer(vulkan_device, command_pools);
}

void renderer::FluidRenderer::Render(
    core::VulkanDevice const& vulkan_device,
    core::VulkanSwapChain& vulkan_swap_chain, core::FrameSync& frame_sync,
    simulation::FluidSimulator const& fluid_simulator,
  uint64_t simulation_signal_value, core::Window const& window,
  uint32_t image_index) {
    uint64_t graphics_wait_value = simulation_signal_value;
  uint64_t graphics_signal_value = frame_sync.GetNextTimelineValue();

  RecordGraphicsCommandBuffer(vulkan_swap_chain, image_index, fluid_simulator);

  vk::PipelineStageFlags wait_stage = vk::PipelineStageFlagBits::eVertexInput;
  vk::TimelineSemaphoreSubmitInfo graphics_timeline_info{
      .waitSemaphoreValueCount = 1,
      .pWaitSemaphoreValues = &graphics_wait_value,
      .signalSemaphoreValueCount = 1,
      .pSignalSemaphoreValues = &graphics_signal_value};

  vk::SubmitInfo graphics_submit_info{
      .pNext = &graphics_timeline_info,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &*frame_sync.Semaphore(),
      .pWaitDstStageMask = &wait_stage,
      .commandBufferCount = 1,
      .pCommandBuffers = &*graphics_command_buffer_,
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &*frame_sync.Semaphore()};
  vulkan_device.GraphicsQueue().submit(graphics_submit_info, nullptr);

    frame_sync.WaitSemaphore(vulkan_device, graphics_signal_value);

  vk::PresentInfoKHR present_info{
      .waitSemaphoreCount = 0,
      .pWaitSemaphores = nullptr,
      .swapchainCount = 1,
      .pSwapchains = &*vulkan_swap_chain.SwapChain(),
      .pImageIndices = &image_index};

    auto result = vulkan_device.PresentQueue().presentKHR(present_info);
  if ((result == vk::Result::eSuboptimalKHR) ||
      (result == vk::Result::eErrorOutOfDateKHR) || framebuffer_resized_) {
    framebuffer_resized_ = false;
    vulkan_swap_chain.RecreateSwapChain(window, vulkan_device);
  } else {
    assert(result == vk::Result::eSuccess);
  }
}

void renderer::FluidRenderer::TransitionImageLayout(
    core::VulkanSwapChain const& vulkan_swap_chain, uint32_t image_index,
    vk::ImageLayout old_layout, vk::ImageLayout new_layout,
    vk::AccessFlags2 src_access_mask, vk::AccessFlags2 dst_access_mask,
    vk::PipelineStageFlags2 src_stage_mask,
    vk::PipelineStageFlags2 dst_stage_mask) {
  vk::ImageMemoryBarrier2 barrier = {
      .srcStageMask = src_stage_mask,
      .srcAccessMask = src_access_mask,
      .dstStageMask = dst_stage_mask,
      .dstAccessMask = dst_access_mask,
      .oldLayout = old_layout,
      .newLayout = new_layout,
      .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
      .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
      .image = vulkan_swap_chain.GetImage(image_index),
      .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                           .baseMipLevel = 0,
                           .levelCount = 1,
                           .baseArrayLayer = 0,
                           .layerCount = 1}};
  vk::DependencyInfo dependency_info{.dependencyFlags = {},
                                     .imageMemoryBarrierCount = 1,
                                     .pImageMemoryBarriers = &barrier};
  graphics_command_buffer_.pipelineBarrier2(dependency_info);
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

void renderer::FluidRenderer::CreateGraphicsCommandBuffer(
    core::VulkanDevice const& vulkan_device,
    core::CommandPools const& command_pools) {
  vk::CommandBufferAllocateInfo alloc_info{
      .commandPool = *command_pools.Graphics(),
      .level = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = 1};
  graphics_command_buffer_ = std::move(
      vulkan_device.Device().allocateCommandBuffers(alloc_info).front());
}

void renderer::FluidRenderer::RecordGraphicsCommandBuffer(
    core::VulkanSwapChain& vulkan_swap_chain, uint32_t image_index,
    simulation::FluidSimulator const& fluid_simulator) {
  auto& command_buffer = graphics_command_buffer_;
  command_buffer.reset();
  command_buffer.begin({});

  TransitionImageLayout(vulkan_swap_chain, image_index,
                        vulkan_swap_chain.GetImageLayout(image_index),
                        vk::ImageLayout::eColorAttachmentOptimal, {},
                        vk::AccessFlagBits2::eColorAttachmentWrite,
                        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                        vk::PipelineStageFlagBits2::eColorAttachmentOutput);
  vulkan_swap_chain.SetImageLayout(image_index,
                                   vk::ImageLayout::eColorAttachmentOptimal);

  vk::ClearValue clear_color = vk::ClearColorValue(0.0F, 0.0F, 0.0F, 1.0F);
  vk::RenderingAttachmentInfo attachment_info{
      .imageView = vulkan_swap_chain.GetImageView(image_index),
      .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
      .loadOp = vk::AttachmentLoadOp::eClear,
      .storeOp = vk::AttachmentStoreOp::eStore,
      .clearValue = clear_color};
  vk::RenderingInfo rendering_info{
      .renderArea = {.offset = {.x = 0, .y = 0},
                     .extent = vulkan_swap_chain.Extent()},
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = &attachment_info};
  command_buffer.beginRendering(rendering_info);
  command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                              *graphics_pipeline_);
  command_buffer.setViewport(
      0, vk::Viewport(0.0F, 0.0F,
                      static_cast<float>(vulkan_swap_chain.Extent().width),
                      static_cast<float>(vulkan_swap_chain.Extent().height),
                      0.0F, 1.0F));
  command_buffer.setScissor(
      0, vk::Rect2D(vk::Offset2D(0, 0), vulkan_swap_chain.Extent()));
  command_buffer.bindVertexBuffers(
      0, {fluid_simulator.FluidParticlesReadBuffer().buffer}, {0});
  command_buffer.draw(fluid_simulator.FluidParticleCount(), 1, 0, 0);
  command_buffer.endRendering();

  TransitionImageLayout(vulkan_swap_chain, image_index,
                        vk::ImageLayout::eColorAttachmentOptimal,
                        vk::ImageLayout::ePresentSrcKHR,
                        vk::AccessFlagBits2::eColorAttachmentWrite, {},
                        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                        vk::PipelineStageFlagBits2::eBottomOfPipe);
  vulkan_swap_chain.SetImageLayout(image_index,
                                   vk::ImageLayout::ePresentSrcKHR);
  command_buffer.end();
}
