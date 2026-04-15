#include "rheo_sph_app.h"

#include <optional>

#include "backends/imgui_impl_vulkan.h"
#include "../ui/panels/top_bar_panel.h"

namespace {

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
  DestroyElevationTexturePreview();
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
      RecreateElevationTexturePreview(*uploaded_texture_path);
      parameters_dirty_ = true;
    }

    bool const parameters_changed = parameters_panel_.Draw();
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

void app::RheoSPHApp::RecreateElevationTexturePreview(
    std::string const& texture_path) {
  DestroyElevationTexturePreview();

  elevation_preview_image_ = resources::ImageAllocator::CreateImage(
      vulkan_device_, command_pools_, texture_path);

  VkDescriptorSet descriptor_set = ImGui_ImplVulkan_AddTexture(
      *elevation_preview_image_.sampler, *elevation_preview_image_.image_view,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  elevation_preview_texture_id_ =
      reinterpret_cast<ui::ParametersPanel::TextureId>(descriptor_set);
  parameters_panel_.SetElevationTexturePreview(elevation_preview_texture_id_);
}

void app::RheoSPHApp::DestroyElevationTexturePreview() {
  if (elevation_preview_texture_id_ != nullptr) {
    ImGui_ImplVulkan_RemoveTexture(
        reinterpret_cast<VkDescriptorSet>(elevation_preview_texture_id_));
    elevation_preview_texture_id_ = nullptr;
    parameters_panel_.SetElevationTexturePreview(nullptr);
  }

  elevation_preview_image_ = {};
}
