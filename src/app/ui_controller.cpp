#include "ui_controller.h"

#include <yaml-cpp/yaml.h>

#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <ios>
#include <optional>
#include <string>

#include "../renderer/ui_texture_handle.h"
#include "../simulation/fluid_simulator.h"
#include "../ui/panels/menu_bar_panel.h"
#include "../ui/panels/parameters_panel.h"
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
  YAML::Node const child = node
      [key];  // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
  if (!child || child.IsNull()) {
    return std::nullopt;
  }
  return child.as<T>();
}

YAML::Node SerializeParametersPanelValues(
    ui::ParametersPanel::Values const& values,
    std::string const& elevation_texture_path) {
  auto set_optional = [](YAML::Node& node, char const* key,
                         auto const& optional_value) {
    if (optional_value.has_value()) {
      node[key] =
          *optional_value;  // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    } else {
      node[key] = YAML::
          Node{};  // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    }
  };

  YAML::Node root;
  root["version"] =
      1;  // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
  root["elevation_texture_path"] =
      elevation_texture_path;  // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)

  YAML::Node parameters;
  set_optional(parameters, "total_fluid_volume", values.total_fluid_volume);
  set_optional(parameters, "min_elevation", values.min_elevation);
  set_optional(parameters, "max_elevation", values.max_elevation);
  set_optional(parameters, "initial_particle_spacing",
               values.initial_particle_spacing);
  set_optional(parameters, "voxel_max_particles", values.voxel_max_particles);
  set_optional(parameters, "viscosity", values.viscosity);
  set_optional(parameters, "rest_density", values.rest_density);
  set_optional(parameters, "gas_constant", values.gas_constant);
  set_optional(parameters, "coefficient_of_restitution",
               values.coefficient_of_restitution);
  set_optional(parameters, "friction", values.friction);
  set_optional(parameters, "yield_stress", values.yield_stress);

  root["parameters"] =
      parameters;  // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
  return root;
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
      !values.voxel_max_particles.has_value() ||
      !values.viscosity.has_value() || !values.rest_density.has_value() ||
      !values.gas_constant.has_value() ||
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

namespace app {

UiIntent UiController::Draw(bool simulation_running) {
  UiIntent intent;

  ui::MenuBarPanel::Events const menu_events = menu_bar_panel_.Draw();
  if (menu_events.uploaded_texture_path.has_value() &&
      !menu_events.uploaded_texture_path->empty()) {
    pending_elevation_texture_path_ = *menu_events.uploaded_texture_path;
    menu_bar_panel_.SetElevationTexturePath(*menu_events.uploaded_texture_path);
    intent.new_texture_path = *menu_events.uploaded_texture_path;
    intent.parameters_changed = true;
  }

  if (menu_events.save_simulation_path.has_value() &&
      !menu_events.save_simulation_path->empty()) {
    std::string const save_path =
        EnsureYamlExtension(*menu_events.save_simulation_path);
    intent.save_path = save_path;
  }

  if (menu_events.load_simulation_path.has_value() &&
      !menu_events.load_simulation_path->empty()) {
    std::string const load_path =
        EnsureYamlExtension(*menu_events.load_simulation_path);
    intent.load_path = load_path;
  }

  bool const parameters_changed = parameters_panel_.Draw();
  if (parameters_changed) {
    intent.parameters_changed = true;
  }

  intent.built_parameters = BuildSimulationParameters(
      parameters_panel_.GetValues(), pending_elevation_texture_path_);
  bool const can_play = intent.built_parameters.has_value();

  ui::TopBarPanel::Events const top_bar_events =
      ui::TopBarPanel::Draw(simulation_running, can_play);
  if (top_bar_events.play_pressed) {
    intent.sim_action = UiIntent::SimAction::kPlay;
  } else if (top_bar_events.pause_pressed) {
    intent.sim_action = UiIntent::SimAction::kPause;
  } else if (top_bar_events.reset_pressed) {
    intent.sim_action = UiIntent::SimAction::kReset;
  }

  return intent;
}

bool UiController::SaveSimulationConfig(std::string const& path) {
  try {
    std::filesystem::path const output_path(path);
    if (output_path.has_parent_path()) {
      std::filesystem::create_directories(output_path.parent_path());
    }

    YAML::Node const root = SerializeParametersPanelValues(
        parameters_panel_.GetValues(), pending_elevation_texture_path_);

    std::ofstream output_stream(path, std::ios::out | std::ios::trunc);
    if (!output_stream.is_open()) {
      return false;
    }

    output_stream << root;
    if (!output_stream.good()) {
      return false;
    }

    menu_bar_panel_.SetSimulationConfigPath(path);
    return true;
  } catch (std::exception const&) {
    return false;
  }
}

bool UiController::LoadSimulationConfig(std::string const& path) {
  try {
    YAML::Node const root = YAML::LoadFile(path);
    YAML::Node const parameters_node = root
        ["parameters"];  // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    if (!parameters_node || !parameters_node.IsMap()) {
      return false;
    }

    ui::ParametersPanel::Values const values =
        DeserializeParametersPanelValues(parameters_node);

    std::string const texture_path =
        root["elevation_texture_path"]  // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
            ? root["elevation_texture_path"]
                  .as<std::
                          string>()  // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
            : std::string{};

    parameters_panel_.SetValues(values);
    pending_elevation_texture_path_ = texture_path;
    menu_bar_panel_.SetElevationTexturePath(texture_path);
    menu_bar_panel_.SetSimulationConfigPath(path);

    return true;
  } catch (std::exception const&) {
    return false;
  }
}

std::optional<simulation::FluidSimulator::Parameters>
UiController::BuildParameters() const {
  return BuildSimulationParameters(parameters_panel_.GetValues(),
                                   pending_elevation_texture_path_);
}

std::string const& UiController::GetElevationTexturePath() const {
  return pending_elevation_texture_path_;
}

void UiController::NotifyTextureLoaded(renderer::UiTextureHandle handle,
                                       void* imgui_id) {}

void UiController::ClearTexturePreview() {
  pending_elevation_texture_path_.clear();
}

ui::ParametersPanel::Values const& UiController::GetParameterValues() const {
  return parameters_panel_.GetValues();
}

}  // namespace app
