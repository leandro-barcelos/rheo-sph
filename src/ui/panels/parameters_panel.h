#ifndef RHEOSPH_UI_PARAMETERS_PANEL_H
#define RHEOSPH_UI_PARAMETERS_PANEL_H

#include <cstdint>
#include <optional>
#include <string>

#include "rheo-sph/src/events/event_handler.h"
#include "rheo-sph/src/events/keyboard_event.h"

namespace ui {

class ParametersPanel {
 public:
  ParametersPanel(ParametersPanel const&) = delete;
  ParametersPanel(ParametersPanel&&) = delete;
  ParametersPanel& operator=(ParametersPanel const&) = delete;
  ParametersPanel& operator=(ParametersPanel&&) = delete;

  ParametersPanel();
  ~ParametersPanel();

  struct Values {
    std::optional<float> total_fluid_volume;
    std::optional<float> initial_particle_spacing;
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

  [[nodiscard]] Values const& GetValues() const { return values_; }
  void SetValues(Values const& values) { values_ = values; }

 private:
  Values values_{};
  std::string dem_file_path_;
  std::string terrain_texture_path_;
  std::string simulation_config_path_;
  float dem_resolution_{10.0F};
  bool help_modal_opened_ = false;
  bool menu_changed_ = false;
  bool is_control_pressed_ = false;

  events::EventHandler<events::KeyPressedEvent> key_pressed_handler_;
  events::EventHandler<events::KeyReleasedEvent> key_released_handler_;

  void OnKeyPressedEvent(events::KeyPressedEvent const& event);
  void OnKeyReleasedEvent(events::KeyReleasedEvent const& event);
  void MenuBar();
  bool TabBar();
  bool TerrainTab();
  bool ParametersTab();
  void SaveFileDialog();
  void LoadFileDialog();
  void DisplayFileDialogs();
  void DisplayModals();
};

}  // namespace ui

#endif  // !RHEOSPH_UI_PARAMETERS_PANEL_H
