#include "rheo_sph_app.h"

#include <GLFW/glfw3.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "../core/command_pool.h"
#include "../core/vulkan_device.h"
#include "../core/window.h"
#include "../renderer/renderer.h"
#include "../renderer/ui_texture_handle.h"
#include "../simulation/fluid_simulator.h"
#include "ui_controller.h"

namespace {

void SetElevationPreviewTexture(
    std::string const& texture_path, renderer::Renderer& renderer,
    core::VulkanDevice const& vulkan_device,
    core::CommandPools const& command_pools, app::UiController& ui_controller,
    renderer::UiTextureHandle& elevation_preview_texture) {
  if (elevation_preview_texture.IsValid()) {
    renderer.UnloadUiTexture(elevation_preview_texture);
    elevation_preview_texture = renderer::kNullUiTexture;
  }

  if (texture_path.empty()) {
    ui_controller.NotifyTextureLoaded(renderer::kNullUiTexture, nullptr);
    return;
  }

  elevation_preview_texture =
      renderer.LoadUiTexture(texture_path, vulkan_device, command_pools);
  ui_controller.NotifyTextureLoaded(
      elevation_preview_texture,
      renderer.ResolveImGuiTextureId(elevation_preview_texture));
}

}  // namespace

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

  while (!window_.ShouldClose()) {
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
  if (intent.new_texture_path.has_value()) {
    SetElevationPreviewTexture(*intent.new_texture_path, renderer_,
                               vulkan_device_, command_pools_, ui_controller_,
                               elevation_preview_texture_);
  }

  if (intent.save_path.has_value()) {
    (void)ui_controller_.SaveSimulationConfig(*intent.save_path);
  }

  bool loaded_config = false;
  if (intent.load_path.has_value()) {
    loaded_config = ui_controller_.LoadSimulationConfig(*intent.load_path);
    if (loaded_config) {
      session_.Pause();

      SetElevationPreviewTexture(ui_controller_.GetElevationTexturePath(),
                                 renderer_, vulkan_device_, command_pools_,
                                 ui_controller_, elevation_preview_texture_);
    }
  }

  std::optional<simulation::FluidSimulator::Parameters> built_parameters =
      loaded_config ? ui_controller_.BuildParameters()
                    : intent.built_parameters;

  if ((intent.parameters_changed || loaded_config) &&
      built_parameters.has_value()) {
    session_.ApplyParameters(*built_parameters, vulkan_device_, command_pools_);
    renderer_.InitTopViewCamera(*built_parameters);
  }

  switch (intent.sim_action) {
    case UiIntent::SimAction::kNone:
      break;
    case UiIntent::SimAction::kPlay:
      if (built_parameters.has_value()) {
        session_.Play();
      }
      break;
    case UiIntent::SimAction::kPause:
      session_.Pause();
      break;
    case UiIntent::SimAction::kReset:
      if (built_parameters.has_value()) {
        renderer_.InitTopViewCamera(*built_parameters);
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
