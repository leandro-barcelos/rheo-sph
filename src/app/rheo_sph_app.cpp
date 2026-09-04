#include "rheo_sph_app.h"

#include <GLFW/glfw3.h>

#include <optional>
#include <vector>

#include "../core/command_pool.h"
#include "../core/vulkan_device.h"
#include "../core/window.h"
#include "../events/event_manager.h"
#include "rheo-sph/src/events/window_event.h"
#include "ui_controller.h"

app::RheoSPHApp::RheoSPHApp()
    : window_(kWindowProperties), renderer_(window_.Size()) {
  key_pressed_event_handler_ = [this](events::KeyPressedEvent const& event) {
    OnKeyPressedEvent(event);
  };
  key_released_event_handler_ = [this](events::KeyReleasedEvent const& event) {
    OnKeyReleasedEvent(event);
  };
  window_resized_event_handler_ =
      [this](events::WindowResizedEvent const& event) {
        OnWindowResizedEvent(event);
      };

  events::Subscribe<events::KeyPressedEvent>(key_pressed_event_handler_);
  events::Subscribe<events::KeyReleasedEvent>(key_released_event_handler_);
  events::Subscribe<events::WindowResizedEvent>(window_resized_event_handler_);
}

app::RheoSPHApp::~RheoSPHApp() {
  events::Unsubscribe<events::KeyPressedEvent>(key_pressed_event_handler_);
  events::Unsubscribe<events::KeyReleasedEvent>(key_released_event_handler_);
  events::Unsubscribe(window_resized_event_handler_);
}

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

  while (!should_close_ && !window_.ShouldClose()) {
    core::Window::PollEvents();
    events::event_manager.DispatchEvents();
    // TODO: Subscribe to KeyPressedEvent and quit on ALT+F4

    if (swap_chain_recreate_pending_) {
      RecreateSwapChainAndNotifyRenderer();
      swap_chain_recreate_pending_ = false;
    }

    // 1. Acquire
    uint32_t const image_index =
        vulkan_swap_chain_.AcquireNextImage(vulkan_device_, frame_sync_);
    if (image_index == core::VulkanSwapChain::kInvalidImageIndex) {
      RecreateSwapChainAndNotifyRenderer();
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

void app::RheoSPHApp::OnKeyPressedEvent(events::KeyPressedEvent const& event) {
  switch (event.GetKey()) {
    case core::kLeftAlt:
    case core::kRightAlt:
      alt_pressed_ = true;
      break;
    case core::kF4:
      if (alt_pressed_) {
        should_close_ = true;
      }
      break;
    default:
      break;
  }
}

void app::RheoSPHApp::OnKeyReleasedEvent(
    events::KeyReleasedEvent const& event) {
  switch (event.GetKey()) {
    case core::kLeftAlt:
    case core::kRightAlt:
      alt_pressed_ = false;
      break;
    default:
      break;
  }
}

void app::RheoSPHApp::OnWindowResizedEvent(
    events::WindowResizedEvent const& event) {
  if (event.GetWidth() <= 0 || event.GetHeight() <= 0) {
    return;
  }

  swap_chain_recreate_pending_ = true;
}

void app::RheoSPHApp::RecreateSwapChainAndNotifyRenderer() {
  vulkan_swap_chain_.RecreateSwapChain(vulkan_device_, window_);
  renderer_.OnSwapChainRecreated(vulkan_swap_chain_);
}