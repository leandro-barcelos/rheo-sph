#include "rheo_sph_app.h"

#include <filesystem>
#include <fstream>
#include <optional>
#include <utility>

#include <yaml-cpp/yaml.h>
#include "../ui/panels/top_bar_panel.h"

namespace {

std::string EnsureYamlExtension(std::string path) {
  if (path.empty()) {
    return path;
  }

  std::filesystem::path const file_path(path);
  std::string const extension = file_path.extension().string();
  if (extension == ".yaml" || extension == ".yml") {
    return path;
  }

  return path + ".yaml";
}

template <typename T>
std::optional<T> ReadOptionalScalar(YAML::Node const& node, char const* key) {
  YAML::Node const child = node[key];
  if (!child || child.IsNull()) {
    return std::nullopt;
  }
  return child.as<T>();
}

YAML::Node SerializeParametersPanelValues(ui::ParametersPanel::Values const& values) {
  auto set_optional = [](YAML::Node& node, char const* key, auto const& optional_value) {
    if (optional_value.has_value()) {
      node[key] = *optional_value;
    } else {
      node[key] = YAML::Node(YAML::NodeType::Null);
    }
  };

  YAML::Node parameters;

  set_optional(parameters, "total_fluid_volume", values.total_fluid_volume);
  set_optional(parameters, "min_elevation", values.min_elevation);
  set_optional(parameters, "max_elevation", values.max_elevation);
  set_optional(parameters, "initial_particle_spacing", values.initial_particle_spacing);
  set_optional(parameters, "voxel_max_particles", values.voxel_max_particles);
  set_optional(parameters, "viscosity", values.viscosity);
  set_optional(parameters, "rest_density", values.rest_density);
  set_optional(parameters, "gas_constant", values.gas_constant);
  set_optional(parameters, "coefficient_of_restitution", values.coefficient_of_restitution);
  set_optional(parameters, "friction", values.friction);
  set_optional(parameters, "yield_stress", values.yield_stress);

  return parameters;
}

ui::ParametersPanel::Values DeserializeParametersPanelValues(
    YAML::Node const& parameters_node) {
  ui::ParametersPanel::Values values{};

  values.total_fluid_volume =
      ReadOptionalScalar<float>(parameters_node, "total_fluid_volume");
  values.min_elevation =
      ReadOptionalScalar<float>(parameters_node, "min_elevation");
  values.max_elevation =
      ReadOptionalScalar<float>(parameters_node, "max_elevation");
  values.initial_particle_spacing =
      ReadOptionalScalar<float>(parameters_node, "initial_particle_spacing");
  values.voxel_max_particles =
      ReadOptionalScalar<uint32_t>(parameters_node, "voxel_max_particles");
  values.viscosity = ReadOptionalScalar<float>(parameters_node, "viscosity");
  values.rest_density =
      ReadOptionalScalar<float>(parameters_node, "rest_density");
  values.gas_constant =
      ReadOptionalScalar<float>(parameters_node, "gas_constant");
  values.coefficient_of_restitution =
      ReadOptionalScalar<float>(parameters_node, "coefficient_of_restitution");
  values.friction = ReadOptionalScalar<float>(parameters_node, "friction");
  values.yield_stress =
      ReadOptionalScalar<float>(parameters_node, "yield_stress");

  return values;
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
  DestroyElevationTexturePreview();
  renderer_.Shutdown();
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
  renderer_.Init(window_, context_, vulkan_device_, vulkan_swap_chain_,
                 command_pools_);
}

void app::RheoSPHApp::MainLoop() {
  last_time_ = 0;
  while (!window_.ShouldClose()) {  // NOLINT(*-id-dependent-backward-branch)
    core::Window::PollEvents();

    uint32_t image_index =
        vulkan_swap_chain_.AcquireNextImage(vulkan_device_, frame_sync_);

    if (image_index == core::VulkanSwapChain::kInvalidImageIndex) {
      vulkan_swap_chain_.RecreateSwapChain(vulkan_device_, window_);
      renderer_.OnSwapChainRecreated(vulkan_swap_chain_);
      continue;
    }

    renderer_.BeginUiFrame();

    ui::MenuBarPanel::Events const menu_events = menu_bar_panel_.Draw();
    if (menu_events.uploaded_texture_path.has_value() &&
        !menu_events.uploaded_texture_path->empty()) {
      pending_elevation_texture_path_ = *menu_events.uploaded_texture_path;
      menu_bar_panel_.SetElevationTexturePath(*menu_events.uploaded_texture_path);
      RecreateElevationTexturePreview(*menu_events.uploaded_texture_path);
      parameters_dirty_ = true;
    }

    if (menu_events.save_simulation_path.has_value() &&
        !menu_events.save_simulation_path->empty()) {
      std::string const save_path = EnsureYamlExtension(*menu_events.save_simulation_path);
      if (SaveSimulationParameters(save_path)) {
        menu_bar_panel_.SetSimulationConfigPath(save_path);
      }
    }

    if (menu_events.load_simulation_path.has_value() &&
        !menu_events.load_simulation_path->empty()) {
      std::string const load_path = EnsureYamlExtension(*menu_events.load_simulation_path);
      if (LoadSimulationParameters(load_path)) {
        menu_bar_panel_.SetSimulationConfigPath(load_path);
      }
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

    renderer_.EndUiFrame();

    std::optional<uint64_t> simulation_signal_value;
    if (simulation_running_ && fluid_simulator_) {
      simulation_signal_value =
          fluid_simulator_->Run(vulkan_device_, frame_sync_, delta_time_);
    }

    renderer_.RenderFrame(vulkan_device_, vulkan_swap_chain_, frame_sync_,
                          fluid_simulator_.get(), image_index, window_,
                          simulation_signal_value);

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

  elevation_preview_texture_id_ = reinterpret_cast<ui::ParametersPanel::TextureId>(
      renderer_.AddUiTexture(*elevation_preview_image_.sampler,
                             *elevation_preview_image_.image_view,
                             vk::ImageLayout::eShaderReadOnlyOptimal));
  parameters_panel_.SetElevationTexturePreview(elevation_preview_texture_id_);
}

void app::RheoSPHApp::DestroyElevationTexturePreview() {
  if (elevation_preview_texture_id_ != nullptr) {
    renderer_.RemoveUiTexture(elevation_preview_texture_id_);
    elevation_preview_texture_id_ = nullptr;
    parameters_panel_.SetElevationTexturePreview(nullptr);
  }

  elevation_preview_image_ = {};
}

bool app::RheoSPHApp::SaveSimulationParameters(std::string const& file_path) const {
  try {
    std::filesystem::path const output_path(file_path);
    if (output_path.has_parent_path()) {
      std::filesystem::create_directories(output_path.parent_path());
    }

    YAML::Node root;
    root["version"] = 1;
    root["elevation_texture_path"] = pending_elevation_texture_path_;
    root["parameters"] = SerializeParametersPanelValues(parameters_panel_.GetValues());

    std::ofstream output_stream(file_path, std::ios::out | std::ios::trunc);
    if (!output_stream.is_open()) {
      return false;
    }

    output_stream << root;
    return output_stream.good();
  } catch (std::exception const&) {
    return false;
  }
}

bool app::RheoSPHApp::LoadSimulationParameters(std::string const& file_path) {
  try {
    YAML::Node const root = YAML::LoadFile(file_path);
    YAML::Node const parameters_node = root["parameters"];
    if (!parameters_node || !parameters_node.IsMap()) {
      return false;
    }

    ui::ParametersPanel::Values values =
        DeserializeParametersPanelValues(parameters_node);

    std::string const texture_path =
        root["elevation_texture_path"]
            ? root["elevation_texture_path"].as<std::string>()
            : std::string{};

    parameters_panel_.SetValues(values);
    pending_elevation_texture_path_ = texture_path;
    menu_bar_panel_.SetElevationTexturePath(texture_path);

    if (!texture_path.empty()) {
      RecreateElevationTexturePreview(texture_path);
    } else {
      DestroyElevationTexturePreview();
    }

    simulation_running_ = false;
    parameters_dirty_ = true;
    return true;
  } catch (std::exception const&) {
    return false;
  }
}
