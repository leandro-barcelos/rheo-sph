#ifndef RHEOSPH_RENDERER_H
#define RHEOSPH_RENDERER_H

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

#include "../core/command_pool.h"
#include "../core/frame_sync.h"
#include "../core/vulkan_context.h"
#include "../core/vulkan_device.h"
#include "../core/vulkan_swap_chain.h"
#include "../core/window.h"
#include "../resources/elevation.h"
#include "../resources/images.h"
#include "../simulation/fluid_simulator.h"
#include "../ui/imgui_layer.h"
#include "fluid_renderer.h"
#include "rheo-sph/src/core/input_events.h"
#include "rheo-sph/src/renderer/camera.h"
#include "terrain_renderer.h"

namespace renderer {

class Renderer {
 public:
  void Init(core::Window const& window, core::VulkanContext const& context,
            core::VulkanDevice const& vulkan_device,
            core::VulkanSwapChain const& vulkan_swap_chain,
            core::CommandPools const& command_pools);
  void BeginUiFrame() const;
  void EndUiFrame() const;
  void RenderFrame(core::VulkanDevice const& vulkan_device,
                   core::VulkanSwapChain& vulkan_swap_chain,
                   core::FrameSync& frame_sync,
                   simulation::FluidSimulator const* fluid_simulator,
                   uint32_t image_index, core::Window const& window,
                   std::optional<uint64_t> simulation_signal_value);
  void OnSwapChainRecreated(core::VulkanSwapChain const& vulkan_swap_chain);

  void ProcessInput(core::WindowSize const& window_size,
                    core::InputState const& input_state);
  void InitTopViewCamera(
      std::shared_ptr<const std::vector<resources::Elevation>> const&
          elevation_samples);

  // Initializes (or re-initializes) the terrain renderer from elevation data.
  // This is independent of the fluid simulation and can be called as soon as
  // an elevation texture is loaded.
  void InitTerrainRenderer(
      core::VulkanDevice const& vulkan_device,
      core::VulkanSwapChain const& vulkan_swap_chain,
      std::shared_ptr<const std::vector<resources::Elevation>> const&
          elevation_samples,
      uint32_t elevation_width, uint32_t elevation_height);
  void InitTerrainRenderer(
      core::VulkanDevice const& vulkan_device,
      core::VulkanSwapChain const& vulkan_swap_chain,
      std::shared_ptr<const std::vector<resources::Elevation>> const&
          elevation_samples,
      uint32_t elevation_width, uint32_t elevation_height,
      std::optional<std::string> const& terrain_texture_filepath);

  void Shutdown();

 private:
  bool framebuffer_resized_ = false;
  vk::raii::CommandBuffer graphics_command_buffer_ = nullptr;
  FluidRenderer fluid_renderer_;
  TerrainRenderer terrain_renderer_;
  ui::ImGuiLayer imgui_layer_;
  std::unordered_map<uint32_t, resources::AllocatedImage> ui_textures_;
  Camera camera_{glm::vec3(0.5F, 2.0F, 0.5F)};
  core::CommandPools const* command_pools_ = nullptr;

  void CreateGraphicsCommandBuffer(core::VulkanDevice const& vulkan_device,
                                   core::CommandPools const& command_pools);
  void TransitionImageLayout(core::VulkanSwapChain const& vulkan_swap_chain,
                             uint32_t image_index, vk::ImageLayout old_layout,
                             vk::ImageLayout new_layout,
                             vk::AccessFlags2 src_access_mask,
                             vk::AccessFlags2 dst_access_mask,
                             vk::PipelineStageFlags2 src_stage_mask,
                             vk::PipelineStageFlags2 dst_stage_mask);
};

}  // namespace renderer

#endif  // !RHEOSPH_RENDERER_H
