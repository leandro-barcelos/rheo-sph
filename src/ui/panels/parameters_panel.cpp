#include "parameters_panel.h"

#include <cstdint>
#include <string>

#include "imgui.h"

namespace {

void DrawFieldLabel(char const* label, bool is_set) {
  if (is_set) {
    ImGui::TextUnformatted(label);
    return;
  }

  ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(220, 80, 80, 255));
  ImGui::TextUnformatted(label);
  ImGui::PopStyleColor();
}

}  // namespace

bool ui::ParametersPanel::DrawOptionalInputFloat(char const* label,
                                                 std::optional<float>& value,
                                                 float step) {
  std::string const input_id = std::string("##") + label;
  DrawFieldLabel(label, value.has_value());
  float current_value = value.value_or(0.0F);
  bool const changed = ImGui::InputFloat(input_id.c_str(), &current_value, step);
  if (changed) {
    value = current_value;
  }
  return changed;
}

bool ui::ParametersPanel::DrawOptionalSliderFloat(char const* label,
                                                  std::optional<float>& value,
                                                  float min_value,
                                                  float max_value) {
  std::string const input_id = std::string("##") + label;
  DrawFieldLabel(label, value.has_value());
  float current_value = value.value_or(min_value);
  bool const changed = ImGui::SliderFloat(input_id.c_str(), &current_value,
                                          min_value, max_value, "%.3f");
  if (changed || ImGui::IsItemActivated()) {
    value = current_value;
  }
  return changed;
}

bool ui::ParametersPanel::DrawOptionalSliderUInt(char const* label,
                                                 std::optional<uint32_t>& value,
                                                 uint32_t min_value,
                                                 uint32_t max_value) {
  std::string const input_id = std::string("##") + label;
  DrawFieldLabel(label, value.has_value());
  int current_value = static_cast<int>(value.value_or(min_value));
  bool const changed =
      ImGui::SliderInt(input_id.c_str(), &current_value,
                       static_cast<int>(min_value),
                       static_cast<int>(max_value));
  if (changed || ImGui::IsItemActivated()) {
    value = static_cast<uint32_t>(current_value);
  }
  return changed;
}

bool ui::ParametersPanel::Draw() {
  bool changed = false;

  ImGui::SetNextWindowPos(ImVec2(12.0F, 90.0F), ImGuiCond_FirstUseEver);

  ImGuiWindowFlags const window_flags =
      ImGuiWindowFlags_NoDocking;

  if (ImGui::Begin("Parameters", nullptr, window_flags)) {
    changed |= DrawOptionalInputFloat("Total fluid volume",
                                      values_.total_fluid_volume, 1000.0F);
    changed |= DrawOptionalInputFloat("Minimum elevation", values_.min_elevation,
                                      1.0F);
    changed |= DrawOptionalInputFloat("Maximum elevation", values_.max_elevation,
                                      1.0F);

    ImGui::Separator();

    changed |= DrawOptionalSliderFloat("Initial particle spacing",
                                       values_.initial_particle_spacing, 0.01F,
                                       20.0F);
    changed |= DrawOptionalSliderUInt("Particles per voxel",
                                      values_.voxel_max_particles, 1, 32);
    changed |= DrawOptionalSliderFloat("Viscosity", values_.viscosity, 0.0F,
                                       5000.0F);
    changed |= DrawOptionalSliderFloat("Rest density", values_.rest_density,
                                       0.0F, 5000.0F);
    changed |= DrawOptionalSliderFloat("Gas constant", values_.gas_constant,
                                       1.0F, 1000.0F);
    changed |= DrawOptionalSliderFloat("Coefficient of restitution",
                                       values_.coefficient_of_restitution,
                                       0.2F / 3.0F, 1.0F);
    changed |= DrawOptionalSliderFloat("Friction", values_.friction, 0.001F,
                                       5.0F);
    changed |= DrawOptionalSliderFloat("Yield-stress", values_.yield_stress,
                                       0.0F, 1000.0F);
  }
  ImGui::End();

  return changed;
}

bool ui::ParametersPanel::AreAllRequiredDefined() const {
  return values_.total_fluid_volume.has_value() &&
         values_.min_elevation.has_value() && values_.max_elevation.has_value() &&
         values_.initial_particle_spacing.has_value() &&
         values_.voxel_max_particles.has_value() && values_.viscosity.has_value() &&
         values_.rest_density.has_value() && values_.gas_constant.has_value() &&
         values_.coefficient_of_restitution.has_value() &&
         values_.friction.has_value() && values_.yield_stress.has_value();
}

ui::ParametersPanel::Values const& ui::ParametersPanel::GetValues() const {
  return values_;
}

void ui::ParametersPanel::SetValues(Values const& values) {
  values_ = values;
}

