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

constexpr simulation::FluidSimulator::Parameters kParameters{
    .voxel_max_particles = 10,
    .fluid_particle_count = 500,
    .bucket_size = {50, 50, 50, 0},
};

class RheoSPHApp {
 public:
    RheoSPHApp() : window_(kWindowProperties), fluid_simulator_(kParameters) {}
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
