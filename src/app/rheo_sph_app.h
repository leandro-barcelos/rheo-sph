#ifndef RHEOSPH_APP_H
#define RHEOSPH_APP_H

#include "../core/command_pool.h"
#include "../core/frame_sync.h"
#include "../core/vulkan_context.h"
#include "../core/vulkan_device.h"
#include "../core/vulkan_swap_chain.h"
#include "../core/window.h"
#include "../renderer/fluid_renderer.h"
#include "../simulation/fluid_simulator.h"

namespace app {

constexpr core::WindowProperties kWindowProperties{
    .width = 800, .height = 600, .title = "Rheo SPH"};

class RheoSPHApp {
 public:
  RheoSPHApp()
      : window_(kWindowProperties),
        fluid_simulator_(simulation::FluidSimulator::Parameters{
            .voxel_max_particles = 10,
            .fluid_particle_count = 500,
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
            .bucket_size = {50, 50, 50, 0},
        }) {}
  void Run();

 private:
  void Init();
  void MainLoop();

  core::VulkanContext context_;
  core::Window window_;
  core::VulkanDevice vulkan_device_;
  core::VulkanSwapChain vulkan_swap_chain_;
  core::CommandPools command_pools_;
  core::FrameSync frame_sync_;
  simulation::FluidSimulator fluid_simulator_;
  renderer::FluidRenderer fluid_renderer_;
};
}  // namespace app

#endif  // !RHEOSPH_APP_H
