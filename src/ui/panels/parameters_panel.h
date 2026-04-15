#ifndef RHEOSPH_UI_PARAMETERS_PANEL_H
#define RHEOSPH_UI_PARAMETERS_PANEL_H

#include <cstdint>
#include <optional>

namespace ui {

class ParametersPanel {
 public:
  struct Values {
    std::optional<float> total_fluid_volume;
    std::optional<float> min_elevation;
    std::optional<float> max_elevation;
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
  [[nodiscard]] Values const& GetValues() const;

 private:
  static bool DrawOptionalInputFloat(char const* label,
                                     std::optional<float>& value,
                                     float step = 1.0F);
  static bool DrawOptionalSliderFloat(char const* label,
                                      std::optional<float>& value,
                                      float min_value, float max_value);
  static bool DrawOptionalSliderUInt(char const* label,
                                     std::optional<uint32_t>& value,
                                     uint32_t min_value, uint32_t max_value);

  Values values_{};
};

}  // namespace ui

#endif  // !RHEOSPH_UI_PARAMETERS_PANEL_H
