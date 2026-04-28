#include "renderer.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "imgui.h"

namespace renderer {

void Renderer::Init(core::Window const& window,
                    core::VulkanContext const& context,
                    core::VulkanDevice const& vulkan_device,
                    core::VulkanSwapChain const& vulkan_swap_chain,
                    core::CommandPools const& command_pools) {
  command_pools_ = &command_pools;
  fluid_renderer_.Init(vulkan_device, vulkan_swap_chain);
  imgui_layer_.Init(window, context, vulkan_device, vulkan_swap_chain);
  CreateGraphicsCommandBuffer(vulkan_device, command_pools);
}

void Renderer::BeginUiFrame() const { imgui_layer_.BeginFrame(); }

void Renderer::EndUiFrame() const { imgui_layer_.EndFrame(); }

void Renderer::RenderFrame(core::VulkanDevice const& vulkan_device,
                           core::VulkanSwapChain& vulkan_swap_chain,
                           core::FrameSync& frame_sync,
                           simulation::FluidSimulator const* fluid_simulator,
                           uint32_t image_index, core::Window const& window,
                           std::optional<uint64_t> simulation_signal_value) {
  uint64_t graphics_wait_value = simulation_signal_value.value_or(0);
  uint64_t graphics_signal_value = frame_sync.GetNextTimelineValue();

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

  fluid_renderer_.Render(command_buffer, vulkan_swap_chain, fluid_simulator,
                         camera_);
  terrain_renderer_.Render(command_buffer, vulkan_swap_chain, fluid_simulator,
                           camera_);
  imgui_layer_.Render(command_buffer);
  imgui_layer_.OnSwapChainRecreated(vulkan_swap_chain);

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

  vk::PipelineStageFlags wait_stage = vk::PipelineStageFlagBits::eVertexInput;
  vk::TimelineSemaphoreSubmitInfo graphics_timeline_info{
      .waitSemaphoreValueCount = simulation_signal_value.has_value() ? 1U : 0U,
      .pWaitSemaphoreValues =
          simulation_signal_value.has_value() ? &graphics_wait_value : nullptr,
      .signalSemaphoreValueCount = 1,
      .pSignalSemaphoreValues = &graphics_signal_value};

  vk::SubmitInfo graphics_submit_info{
      .pNext = &graphics_timeline_info,
      .waitSemaphoreCount = simulation_signal_value.has_value() ? 1U : 0U,
      .pWaitSemaphores = simulation_signal_value.has_value()
                             ? &*frame_sync.Semaphore()
                             : nullptr,
      .pWaitDstStageMask =
          simulation_signal_value.has_value() ? &wait_stage : nullptr,
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
    vulkan_swap_chain.RecreateSwapChain(vulkan_device, window);
  } else {
    assert(result == vk::Result::eSuccess);
  }
}

void Renderer::OnSwapChainRecreated(
    core::VulkanSwapChain const& vulkan_swap_chain) {
  imgui_layer_.OnSwapChainRecreated(vulkan_swap_chain);
}

UiTextureHandle Renderer::AddUiTexture(vk::Sampler sampler,
                                       vk::ImageView image_view,
                                       vk::ImageLayout image_layout) {
  return imgui_layer_.AddTexture(sampler, image_view, image_layout);
}

void Renderer::RemoveUiTexture(UiTextureHandle handle) {
  if (!handle.IsValid()) {
    return;
  }

  imgui_layer_.RemoveTexture(handle);
}

UiTextureHandle Renderer::LoadUiTexture(
    std::string const& path, core::VulkanDevice const& vulkan_device,
    core::CommandPools const& command_pools) {
  resources::AllocatedImage image = resources::ImageAllocator::CreateImage(
      vulkan_device, command_pools, path);

  UiTextureHandle const handle =
      AddUiTexture(*image.sampler, *image.image_view,
                   vk::ImageLayout::eShaderReadOnlyOptimal);
  if (!handle.IsValid()) {
    return kNullUiTexture;
  }

  ui_textures_.emplace(handle.id, std::move(image));
  return handle;
}

void Renderer::UnloadUiTexture(UiTextureHandle handle) {
  if (!handle.IsValid()) {
    return;
  }

  auto iter = ui_textures_.find(handle.id);
  if (iter == ui_textures_.end()) {
    RemoveUiTexture(handle);
    return;
  }

  RemoveUiTexture(handle);
  ui_textures_.erase(iter);
}

void* Renderer::ResolveImGuiTextureId(UiTextureHandle handle) const {
  return imgui_layer_.ResolveImGuiTextureId(handle);
}

void Renderer::ProcessInput(core::WindowSize const& window_size,
                            core::InputEvent const& events) {
  bool ignore_mouse_events = ImGui::GetIO().WantCaptureMouse;
  camera_.ProcessInput(window_size, events, ignore_mouse_events);
}

void Renderer::InitTopViewCamera(
    simulation::FluidSimulator::Parameters const& params) {
  glm::vec3 bounds_min(std::numeric_limits<float>::infinity());
  glm::vec3 bounds_max(-std::numeric_limits<float>::infinity());
  for (auto const& elevation_sample : *params.elevation_samples) {
    bounds_min[0] = std::min(bounds_min[0], elevation_sample.position[0]);
    bounds_min[1] = std::min(bounds_min[1], elevation_sample.position[1]);
    bounds_min[2] = std::min(bounds_min[2], elevation_sample.position[2]);

    bounds_max[0] = std::max(bounds_max[0], elevation_sample.position[0]);
    bounds_max[1] = std::max(bounds_max[1], elevation_sample.position[1]);
    bounds_max[2] = std::max(bounds_max[2], elevation_sample.position[2]);
  }

  camera_.InitTopView(bounds_min, bounds_max);
}

void Renderer::InitTerrainRenderer(
    core::VulkanDevice const& vulkan_device,
    core::VulkanSwapChain const& vulkan_swap_chain,
    uint32_t elevation_width, uint32_t elevation_height) {
  if (command_pools_ != nullptr) {
    terrain_renderer_.Init(vulkan_device, vulkan_swap_chain, *command_pools_,
                           elevation_width, elevation_height);
  }
}

void Renderer::Shutdown() {
  imgui_layer_.Shutdown();
  ui_textures_.clear();
}

void Renderer::CreateGraphicsCommandBuffer(
    core::VulkanDevice const& vulkan_device,
    core::CommandPools const& command_pools) {
  vk::CommandBufferAllocateInfo alloc_info{
      .commandPool = *command_pools.Graphics(),
      .level = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = 1};
  graphics_command_buffer_ = std::move(
      vulkan_device.Device().allocateCommandBuffers(alloc_info).front());
}

void Renderer::TransitionImageLayout(
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

}  // namespace renderer
