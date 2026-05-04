#ifndef RHEOSPH_UI_PARAMETERS_PANEL_H
#define RHEOSPH_UI_PARAMETERS_PANEL_H

#include <cstdint>
#include <optional>
#include <string>

#include "rheo-sph/src/core/input_events.h"

namespace ui {

class ParametersPanel {
 public:
  struct Events {
    std::optional<std::string> uploaded_dem_texture_path;
    std::optional<std::string> uploaded_visualization_texture_path;
    std::optional<std::string> save_simulation_path;
    std::optional<std::string> load_simulation_path;
    bool quit_requested = false;
  } __attribute__((aligned(128)));

  struct Values {
    std::optional<float> total_fluid_volume;
    std::optional<float> initial_particle_spacing;
    std::optional<float> dem_resolution;
    std::optional<uint32_t> voxel_max_particles;
    std::optional<float> viscosity;
    std::optional<float> rest_density;
    std::optional<float> gas_constant;
    std::optional<float> coefficient_of_restitution;
    std::optional<float> friction;
    std::optional<float> yield_stress;
  } __attribute__((aligned(128)));

  static constexpr float kPanelWidth = 340.0F;

  [[nodiscard]] bool Draw();
  [[nodiscard]] bool AreAllRequiredDefined() const;
  void ProcessInput(core::InputState const& input_state);

  [[nodiscard]] Values const& GetValues() const { return values_; }
  void SetValues(Values const& values) { values_ = values; }
  [[nodiscard]] Events const& GetEvents() const { return events_; }
  void SetEvents(Events const& events) { events_ = events; }
  void ClearEvents() { events_ = Events{}; }
  void SetDEMTexturePath(std::string const& path) { dem_texture_path_ = path; }
  void SetSimulationConfigPath(std::string const& path) {
    simulation_config_path_ = path;
  }
  void SetVisualizationTexturePath(std::string const& path) {
    visualization_texture_path_ = path;
  }

 private:
  Events events_{};
  Values values_{};
  std::string dem_texture_path_;
  std::string visualization_texture_path_;
  std::string simulation_config_path_;
  bool help_modal_opened_ = false;
  bool menu_changed_ = false;

  void MenuBar();
  void SaveFileDialog();
  void LoadFileDialog();
  [[nodiscard]] bool ParametersInput();
  void DisplayFileDialogs();
  void DisplayModals();
};

}  // namespace ui

#endif  // !RHEOSPH_UI_PARAMETERS_PANEL_H
