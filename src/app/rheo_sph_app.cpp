#include "rheo_sph_app.h"

#include <optional>

#include "imgui.h"
#include "../ui/panels/top_bar_panel.h"

namespace {

void DrawMainDockspaceBelowTopBars() {
  ImGuiViewport* viewport = ImGui::GetMainViewport();
  float const reserved_top_height =
  ImGui::GetFrameHeight() + ui::TopBarPanel::kToolbarHeight;

  ImGui::SetNextWindowPos(
    ImVec2(viewport->Pos.x, viewport->Pos.y + reserved_top_height));
  ImGui::SetNextWindowSize(
    ImVec2(viewport->Size.x, viewport->Size.y - reserved_top_height));
  ImGui::SetNextWindowViewport(viewport->ID);

  ImGuiWindowFlags const window_flags =
    ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
    ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
    ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0F);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0F, 0.0F));
  ImGui::Begin("MainDockspaceHost", nullptr, window_flags);
  ImGui::PopStyleVar(3);

  ImGuiID const dockspace_id = ImGui::GetID("MainDockspace");
  ImGui::DockSpace(dockspace_id, ImVec2(0.0F, 0.0F),
           ImGuiDockNodeFlags_PassthruCentralNode);
  ImGui::End();
}

std::optional<simulation::FluidSimulator::Parameters> BuildSimulationParameters(
    ui::ParametersPanel::Values const& values,
    std::string const& elevation_texture_path) {
  if (!values.total_fluid_volume.has_value() ||
      !values.min_elevation.has_value() || !values.max_elevation.has_value() ||
      !values.initial_particle_spacing.has_value() ||
      !values.voxel_max_particles.has_value() || !values.viscosity.has_value() ||
      !values.rest_density.has_value() || !values.gas_constant.has_value() ||
      !values.coefficient_of_restitution.has_value() ||
      !values.friction.has_value() || !values.yield_stress.has_value() ||
      elevation_texture_path.empty()) {
    return std::nullopt;
  }

  return simulation::FluidSimulator::Parameters{
      .voxel_max_particles = *values.voxel_max_particles,
      .rest_density = *values.rest_density,
      .total_fluid_volume = *values.total_fluid_volume,
      .viscosity = *values.viscosity,
      .gas_constant = *values.gas_constant,
      .coefficient_of_restitution = *values.coefficient_of_restitution,
      .elevation_texture_filename = elevation_texture_path,
      .min_elevation = *values.min_elevation,
      .max_elevation = *values.max_elevation,
      .friction = *values.friction,
      .yield_stress = *values.yield_stress,
      .initial_particle_spacing = *values.initial_particle_spacing,
  };
}

}  // namespace

void app::RheoSPHApp::Run() {
  Init();
  MainLoop();
  imgui_layer_.Shutdown();
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
  vulkan_swap_chain_.Init(vulkan_device_, window_);
  // Command Pools
  command_pools_.Init(vulkan_device_);
  // Sync objects
  frame_sync_.Init(vulkan_device_);
  // Rendering
  fluid_renderer_.Init(vulkan_device_, vulkan_swap_chain_, command_pools_);
  // UI
  imgui_layer_.Init(window_, context_, vulkan_device_, vulkan_swap_chain_);
}

void app::RheoSPHApp::MainLoop() {
  last_time_ = 0;
  while (!window_.ShouldClose()) {  // NOLINT(*-id-dependent-backward-branch)
    core::Window::PollEvents();

    uint32_t image_index =
        vulkan_swap_chain_.AcquireNextImage(vulkan_device_, frame_sync_);

    if (image_index == core::VulkanSwapChain::kInvalidImageIndex) {
      vulkan_swap_chain_.RecreateSwapChain(vulkan_device_, window_);
      imgui_layer_.OnSwapChainRecreated(vulkan_swap_chain_);
      continue;
    }

    imgui_layer_.BeginFrame();

    std::optional<std::string> const uploaded_texture_path =
        menu_bar_panel_.Draw();
    if (uploaded_texture_path.has_value() &&
        !uploaded_texture_path->empty()) {
      pending_elevation_texture_path_ = *uploaded_texture_path;
      menu_bar_panel_.SetElevationTexturePath(*uploaded_texture_path);
      parameters_dirty_ = true;
    }

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    float const top_offset =
        ImGui::GetFrameHeight() + ui::TopBarPanel::kToolbarHeight;
    bool const parameters_changed =
        parameters_panel_.Draw(top_offset, viewport->Size.y);
    if (parameters_changed) {
      parameters_dirty_ = true;
    }

    std::optional<simulation::FluidSimulator::Parameters> const ui_parameters =
        BuildSimulationParameters(parameters_panel_.GetValues(),
                                 pending_elevation_texture_path_);
    bool const can_play = ui_parameters.has_value();

    ui::TopBarPanel::Events const top_bar_events =
        ui::TopBarPanel::Draw(simulation_running_, can_play);
    if (top_bar_events.play_pressed && can_play) {
      if (!fluid_simulator_ || parameters_dirty_) {
        simulation_parameters_ = ui_parameters;
        RecreateFluidSimulator();
        parameters_dirty_ = false;
      }
      simulation_running_ = true;
    }
    if (top_bar_events.pause_pressed) {
      simulation_running_ = false;
    }
    if (top_bar_events.reset_pressed) {
      simulation_running_ = false;
      if (can_play) {
        simulation_parameters_ = ui_parameters;
        RecreateFluidSimulator();
        parameters_dirty_ = false;
      }
    }

    DrawMainDockspaceBelowTopBars();

    imgui_layer_.EndFrame();

    std::optional<uint64_t> simulation_signal_value;
    if (simulation_running_ && fluid_simulator_) {
      simulation_signal_value =
          fluid_simulator_->Run(vulkan_device_, frame_sync_, delta_time_);
    }

    fluid_renderer_.Render(vulkan_device_, vulkan_swap_chain_, frame_sync_,
                           fluid_simulator_.get(), image_index, window_,
                           simulation_signal_value,
                           [this](vk::raii::CommandBuffer const& command_buffer) {
                             imgui_layer_.Render(command_buffer);
                           });
    imgui_layer_.OnSwapChainRecreated(vulkan_swap_chain_);

    double current_time = glfwGetTime();
    delta_time_ = (current_time - last_time_) * 1000.0;
    last_time_ = current_time;
  }
}

void app::RheoSPHApp::RecreateFluidSimulator() {
  if (!simulation_parameters_.has_value()) {
    return;
  }

  vulkan_device_.Device().waitIdle();

  fluid_simulator_.reset();
  fluid_simulator_ =
      std::make_unique<simulation::FluidSimulator>(*simulation_parameters_);
  fluid_simulator_->Init(vulkan_device_, command_pools_);
}
