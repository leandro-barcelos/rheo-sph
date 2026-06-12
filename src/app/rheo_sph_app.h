#ifndef RHEOSPH_APP_H
#define RHEOSPH_APP_H

#include <optional>
#include "../core/command_pool.h"
#include "../core/frame_sync.h"
#include "../core/vulkan_context.h"
#include "../core/vulkan_device.h"
#include "../core/vulkan_swap_chain.h"
#include "../core/window.h"
#include "../events/app_event.h"
#include "../events/keyboard_event.h"
#include "../events/window_event.h"
#include "../renderer/renderer.h"
#include "rheo-sph/src/events/file_event.h"
#include "rheo-sph/src/events/parameter_event.h"
#include "simulation_session.h"
#include "ui_controller.h"

namespace app {

constexpr core::WindowProperties kWindowProperties{
    .width = 1280, .height = 720, .title = "Rheo SPH"};

class RheoSPHApp {
 public:
  RheoSPHApp();
  RheoSPHApp(const RheoSPHApp&) = delete;
  RheoSPHApp(RheoSPHApp&&) = delete;
  RheoSPHApp& operator=(const RheoSPHApp&) = delete;
  RheoSPHApp& operator=(RheoSPHApp&&) = delete;
  ~RheoSPHApp();

  void Run();

 private:
  void Init();
  void MainLoop();
  void ProcessIntent(UiIntent const& intent);
  void UpdateDeltaTime();
  void OnKeyPressedEvent(events::KeyPressedEvent const& event);
  void OnKeyReleasedEvent(events::KeyReleasedEvent const& event);
  void OnWindowResizedEvent(events::WindowResizedEvent const& event);
  void RecreateSwapChainAndNotifyRenderer();

  core::VulkanContext context_;
  core::Window window_;
  core::VulkanDevice vulkan_device_;
  core::VulkanSwapChain vulkan_swap_chain_;
  core::CommandPools command_pools_;
  core::FrameSync frame_sync_;
  renderer::Renderer renderer_;
  SimulationSession session_;
  UiController ui_controller_;
  double last_time_ = 0;
  double delta_time_ = 0;
  bool should_close_ = false;
  bool swap_chain_recreate_pending_ = false;
  bool alt_pressed_ = false;

  events::EventHandler<events::KeyPressedEvent> key_pressed_event_handler_;
  events::EventHandler<events::KeyReleasedEvent> key_released_event_handler_;
  events::EventHandler<events::WindowResizedEvent>
      window_resized_event_handler_;
  events::EventHandler<events::QuitRequestedEvent>
      quit_requested_event_handler_;
  events::EventHandler<events::DEMFileUploadedEvent>
      dem_file_uploaded_event_handler_;
  events::EventHandler<events::DEMResolutionChangedEvent>
      dem_resolution_event_handler_;

  std::optional<std::string> dem_file_path_;
  std::optional<float> dem_resolution_;
};

}  // namespace app

#endif  // !RHEOSPH_APP_H
