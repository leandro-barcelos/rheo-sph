#include "rheo_sph_app.h"

#include <GLFW/glfw3.h>

#include <cstdint>
#include <optional>
#include <vector>

#include "../core/command_pool.h"
#include "../core/vulkan_device.h"
#include "../core/window.h"
#include "../renderer/renderer.h"
#include "../simulation/fluid_simulator.h"
#include "ui_controller.h"

void app::RheoSPHApp::Run() {
  Init();
  MainLoop();

  // registry drains here; no manual texture cleanup needed
  renderer_.Shutdown();
}

void app::RheoSPHApp::Init() {
  std::vector<const char*> const window_extensions =
      core::Window::GetRequiredExtensions();

  // Context, Instance and Validation
  context_.Init(window_extensions);
  // Surface
  window_.CreateSurface(context_);
  // Physical and Logical device and queues
  vulkan_device_.Init(context_, *window_.Surface());
  // Swapchain and Image views
  vulkan_swap_chain_.Init(vulkan_device_, window_);
  // Command Pools
  command_pools_.Init(vulkan_device_);
  // Sync objects
  frame_sync_.Init(vulkan_device_);
  // Rendering
  renderer_.Init(window_, context_, vulkan_device_, vulkan_swap_chain_,
                 command_pools_);
}

void app::RheoSPHApp::MainLoop() {
  last_time_ = 0;

  should_close_ |= window_.ShouldClose();

  while (!should_close_) {
    core::Window::PollEvents();
    auto input_events = window_.DrainInputEvents();
    renderer_.ProcessInput(window_.Size(), input_events);

    // 1. Acquire
    uint32_t const image_index =
        vulkan_swap_chain_.AcquireNextImage(vulkan_device_, frame_sync_);
    if (image_index == core::VulkanSwapChain::kInvalidImageIndex) {
      vulkan_swap_chain_.RecreateSwapChain(vulkan_device_, window_);
      renderer_.OnSwapChainRecreated(vulkan_swap_chain_);
      continue;
    }

    // 2. UI
    renderer_.BeginUiFrame();
    ui_controller_.ProcessInput(input_events);
    UiIntent const intent = ui_controller_.Draw(session_.IsRunning());
    renderer_.EndUiFrame();

    // 3. Process intent
    ProcessIntent(intent);

    // 4. Simulate + Render
    auto sim_signal = session_.Tick(vulkan_device_, frame_sync_, delta_time_);
    renderer_.RenderFrame(vulkan_device_, vulkan_swap_chain_, frame_sync_,
                          session_.Simulator(), image_index, window_,
                          sim_signal);

    UpdateDeltaTime();
  }
}

void app::RheoSPHApp::ProcessIntent(UiIntent const& intent) {
  if (intent.quit_app) {
    should_close_ = true;
  }

  if (intent.save_path.has_value()) {
    (void)ui_controller_.SaveSimulationConfig(*intent.save_path);
  }

  // Reinitialize terrain when elevation data changed (upload or load)
  if (intent.elevation_changed && intent.elevation_samples != nullptr) {
    renderer_.InitTopViewCamera(intent.elevation_samples);
    std::optional<std::string> terrain_path_opt =
        intent.visualization_texture_path.empty()
            ? std::nullopt
            : std::optional<std::string>(intent.visualization_texture_path);
    renderer_.InitTerrainRenderer(
        vulkan_device_, vulkan_swap_chain_, intent.elevation_samples,
        intent.elevation_dimensions[0], intent.elevation_dimensions[1],
        terrain_path_opt);
  } else if (intent.terrain_texture_changed) {
    // Terrain texture changed but elevation didn't — reinit with current data
    if (intent.built_parameters.has_value()) {
      auto const& built_parameters = *intent.built_parameters;
      renderer_.InitTopViewCamera(built_parameters.elevation_samples);
      std::optional<std::string> terrain_path_opt =
          intent.visualization_texture_path.empty()
              ? std::nullopt
              : std::optional<std::string>(intent.visualization_texture_path);
      renderer_.InitTerrainRenderer(
          vulkan_device_, vulkan_swap_chain_,
          built_parameters.elevation_samples, built_parameters.elevation_width,
          built_parameters.elevation_height, terrain_path_opt);
    }
  }

  // Apply fluid simulation parameters when they change.
  if (intent.parameters_changed && intent.built_parameters.has_value()) {
    session_.ApplyParameters(*intent.built_parameters, vulkan_device_,
                             command_pools_);
  }

  switch (intent.sim_action) {
    case UiIntent::SimAction::kNone:
      break;
    case UiIntent::SimAction::kPlay:
      if (intent.built_parameters.has_value()) {
        session_.Play();
      }
      break;
    case UiIntent::SimAction::kPause:
      session_.Pause();
      break;
    case UiIntent::SimAction::kReset:
      if (intent.built_parameters.has_value()) {
        session_.Reset(vulkan_device_, command_pools_);
      }
      break;
  }
}

void app::RheoSPHApp::UpdateDeltaTime() {
  double const current_time = glfwGetTime();
  delta_time_ = (current_time - last_time_) * 1000.0;
  last_time_ = current_time;
}
