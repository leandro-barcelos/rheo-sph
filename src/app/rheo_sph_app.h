#ifndef RHEOSPH_APP_H
#define RHEOSPH_APP_H

#include <memory>
#include <optional>
#include <string>

#include "../core/command_pool.h"
#include "../core/frame_sync.h"
#include "../core/vulkan_context.h"
#include "../core/vulkan_device.h"
#include "../core/vulkan_swap_chain.h"
#include "../core/window.h"
#include "../renderer/fluid_renderer.h"
#include "../resources/images.h"
#include "../simulation/fluid_simulator.h"
#include "../ui/imgui_layer.h"
#include "../ui/panels/menu_bar_panel.h"
#include "../ui/panels/parameters_panel.h"

namespace app {

constexpr core::WindowProperties kWindowProperties{
    .width = 1280, .height = 720, .title = "Rheo SPH"};

class RheoSPHApp {
 public:
    RheoSPHApp() : window_(kWindowProperties) {}
  void Run();

 private:
  void Init();
  void MainLoop();
  void RecreateFluidSimulator();
  void RecreateElevationTexturePreview(std::string const& texture_path);
  void DestroyElevationTexturePreview();
  [[nodiscard]] bool SaveSimulationParameters(std::string const& file_path) const;
  [[nodiscard]] bool LoadSimulationParameters(std::string const& file_path);

  core::VulkanContext context_;
  core::Window window_;
  core::VulkanDevice vulkan_device_;
  core::VulkanSwapChain vulkan_swap_chain_;
  core::CommandPools command_pools_;
  core::FrameSync frame_sync_;
    std::optional<simulation::FluidSimulator::Parameters> simulation_parameters_;
  std::unique_ptr<simulation::FluidSimulator> fluid_simulator_;
  renderer::FluidRenderer fluid_renderer_;
  ui::ImGuiLayer imgui_layer_;
  ui::MenuBarPanel menu_bar_panel_;
    ui::ParametersPanel parameters_panel_;
  resources::AllocatedImage elevation_preview_image_;
  ui::ParametersPanel::TextureId elevation_preview_texture_id_ = nullptr;
  bool simulation_running_ = false;
    bool parameters_dirty_ = true;
  std::string pending_elevation_texture_path_;
  double last_time_ = 0;
  double delta_time_ = 0;
};
}  // namespace app

#endif  // !RHEOSPH_APP_H
