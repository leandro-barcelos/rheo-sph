#ifndef RHEOSPH_APP_H
#define RHEOSPH_APP_H

#include <memory>
#include <string>

#include "../core/command_pool.h"
#include "../core/frame_sync.h"
#include "../core/vulkan_context.h"
#include "../core/vulkan_device.h"
#include "../core/vulkan_swap_chain.h"
#include "../core/window.h"
#include "../renderer/fluid_renderer.h"
#include "../simulation/fluid_simulator.h"
#include "../ui/imgui_layer.h"
#include "../ui/panels/menu_bar_panel.h"

namespace app {

constexpr core::WindowProperties kWindowProperties{
    .width = 800, .height = 600, .title = "Rheo SPH"};

class RheoSPHApp {
 public:
  RheoSPHApp()
      : window_(kWindowProperties),
        simulation_parameters_(simulation::FluidSimulator::Parameters{
            .voxel_max_particles = 10,
            .rest_density = 1000,
            .total_fluid_volume = 11700000,
            .viscosity = 741,
            .gas_constant = 230,
            .coefficient_of_restitution = 0.067,
            .elevation_texture_filename = "textures/normalized_brumadinho.png",
            .min_elevation = 729,
            .max_elevation = 1120,
            .friction = 0,
            .yield_stress = 59.82,
            .initial_particle_spacing = 1.0F / 9.0F,
        }) {}
  void Run();

 private:
  void Init();
  void MainLoop();
  void RecreateFluidSimulator();

  core::VulkanContext context_;
  core::Window window_;
  core::VulkanDevice vulkan_device_;
  core::VulkanSwapChain vulkan_swap_chain_;
  core::CommandPools command_pools_;
  core::FrameSync frame_sync_;
  simulation::FluidSimulator::Parameters simulation_parameters_;
  std::unique_ptr<simulation::FluidSimulator> fluid_simulator_;
  renderer::FluidRenderer fluid_renderer_;
  ui::ImGuiLayer imgui_layer_;
  ui::MenuBarPanel menu_bar_panel_;
  bool simulation_running_ = false;
  bool recreate_simulation_requested_ = false;
  std::string pending_elevation_texture_path_;
  double last_time_ = 0;
  double delta_time_ = 0;
};
}  // namespace app

#endif  // !RHEOSPH_APP_H
