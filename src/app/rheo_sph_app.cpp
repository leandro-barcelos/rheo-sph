#include "rheo_sph_app.h"

void app::RheoSPHApp::Run() {
  Init();
  MainLoop();
}

void app::RheoSPHApp::Init() {
  std::vector<const char*> window_extensions =
      core::Window::GetRequiredExtensions();

  // Context, Instance and Validation
  context_.Init(window_extensions);
  // Surface
  window_.CreateSurface(context_);
  // Physical and Logical device and queues
  vulkan_device_.Init(context_, *window_.Surface());
  // Swapchain and Image views
  vulkan_swap_chain_.Init(window_, vulkan_device_);
  // Command Pools
  command_pools_.Init(vulkan_device_);
  // Sync objects
  frame_sync_.Init(vulkan_device_);
  // Simulation
  fluid_simulator_.Init(vulkan_device_, command_pools_);
  // Rendering
  fluid_renderer_.Init(vulkan_device_, vulkan_swap_chain_, command_pools_);
}

void app::RheoSPHApp::MainLoop() {
  while (!window_.ShouldClose()) {  // NOLINT(*-id-dependent-backward-branch)
    uint32_t image_index =
        vulkan_swap_chain_.AcquireNextImage(vulkan_device_, frame_sync_);

    if (image_index == core::VulkanSwapChain::kInvalidImageIndex) {
      vulkan_swap_chain_.RecreateSwapChain(window_, vulkan_device_);
      continue;
    }

    uint64_t simulation_signal_value =
        fluid_simulator_.Run(vulkan_device_, frame_sync_);

    fluid_renderer_.Render(vulkan_device_, vulkan_swap_chain_, frame_sync_,
                           fluid_simulator_, simulation_signal_value, window_,
                           image_index);

    core::Window::PollEvents();
  }
}
