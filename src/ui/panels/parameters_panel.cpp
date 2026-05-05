#include "parameters_panel.h"

#include <filesystem>

#include "ImGuiFileDialog.h"
#include "imgui.h"
#include "rheo-sph/src/core/input_events.h"

namespace {
constexpr const char* kUploadDialogKey = "UploadElevationTextureDialog";
constexpr const char* kUploadTerrainDialogKey = "UploadTerrainTextureDialog";
constexpr const char* kSaveSimulationDialogKey = "SaveSimulationDialog";
constexpr const char* kLoadSimulationDialogKey = "LoadSimulationDialog";
constexpr const char* kHelpModalKey = "Help";
}  // namespace

bool ui::ParametersPanel::Draw() {
  ImGui::SetNextWindowPos(ImVec2(12.0F, 90.0F), ImGuiCond_FirstUseEver);

  ImGuiWindowFlags const window_flags =
      ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_MenuBar;

  bool opened = true;
  if (!ImGui::Begin("RheoSPH - Simulation Settings", &opened, window_flags)) {
    ImGui::End();
    return false;
  }

  MenuBar();

  char* loaded_simulation = simulation_config_path_.data();
  ImGui::InputText("Loaded simulation", loaded_simulation, 255,
                   ImGuiInputTextFlags_ReadOnly);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Path to the loaded simulation file");
  }

  ImGui::Spacing();

  bool changed = TabBar();
  if (menu_changed_) {
    changed = true;
    menu_changed_ = false;
  }

  ImGui::End();

  DisplayFileDialogs();
  DisplayModals();

  return changed;
}

bool ui::ParametersPanel::AreAllRequiredDefined() const {
  return values_.total_fluid_volume.has_value() &&
         values_.initial_particle_spacing.has_value() &&
         values_.voxel_max_particles.has_value() &&
         values_.viscosity.has_value() && values_.rest_density.has_value() &&
         values_.gas_constant.has_value() &&
         values_.coefficient_of_restitution.has_value() &&
         values_.friction.has_value() && values_.yield_stress.has_value();
}

void ui::ParametersPanel::ProcessInput(core::InputState const& input_state) {
  for (auto const& key : input_state.pressed_keys) {
    switch (key) {
      case core::Key::kF1:
        help_modal_opened_ = true;
        ImGui::OpenPopup(kHelpModalKey);
        break;
      case core::Key::kO:
        if (input_state.modifiers.control) {
          LoadFileDialog();
        }
        break;
      case core::Key::kS:
        if (input_state.modifiers.control) {
          SaveFileDialog();
        }
        break;
      default:
        break;
    }
  }
}

void ui::ParametersPanel::MenuBar() {
  if (!ImGui::BeginMenuBar()) {
    ImGui::EndMenuBar();
    return;
  }

  if (ImGui::BeginMenu("File")) {
    if (ImGui::MenuItem("New", "")) {
      values_ = Values{};
      events_ = Events{};
      dem_texture_path_.clear();
      visualization_texture_path_.clear();
      simulation_config_path_.clear();
      help_modal_opened_ = false;
      menu_changed_ = true;
    }

    if (ImGui::MenuItem("Open", "CTRL+O")) {
      LoadFileDialog();
    }

    if (ImGui::MenuItem("Save", "CTRL+S")) {
      SaveFileDialog();
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Select DEM")) {
      IGFD::FileDialogConfig config{};
      config.path = std::filesystem::current_path().string();
      config.filePathName = dem_texture_path_;
      config.flags = ImGuiFileDialogFlags_Modal;

      ImGuiFileDialog::Instance()->OpenDialog(
          kUploadDialogKey, "Select Digital Elevation Model",
          "GeoTiff files{.tif,.tiff},.*", config);
    }

    if (ImGui::MenuItem("Select visualization texture")) {
      IGFD::FileDialogConfig config{};
      config.path = std::filesystem::current_path().string();
      config.filePathName = visualization_texture_path_;
      config.flags = ImGuiFileDialogFlags_Modal;

      ImGuiFileDialog::Instance()->OpenDialog(
          kUploadTerrainDialogKey, "Upload Visualization Texture",
          "Image files{.png,.jpg,.jpeg,.bmp,.tga,.tif,.tiff},.*", config);
    }

    ImGui::Separator();

    bool debug = true;
    if (ImGui::MenuItem("Debug", "", debug)) {
      // TODO
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Quit", "ALT+F4")) {
      events_.quit_requested = true;
    }
    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("Help")) {
    if (ImGui::MenuItem("Help", "F1")) {
      help_modal_opened_ = true;
      ImGui::OpenPopup(kHelpModalKey);
    }

    ImGui::EndMenu();
  }

  ImGui::EndMenuBar();
}

bool ui::ParametersPanel::TabBar() {
  bool changed = false;

  if (ImGui::BeginTabBar("TabBar")) {
    changed |= TerrainTab();
    changed |= ParametersTab();

    ImGui::EndTabBar();
  }

  return changed;
}

bool ui::ParametersPanel::TerrainTab() {
  bool changed = false;

  if (ImGui::BeginTabItem("Terrain")) {
    char* dem = dem_texture_path_.data();
    ImGui::InputText("DEM", dem, 255, ImGuiInputTextFlags_ReadOnly);
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Path to the DEM file");
    }

    float dem_resolution = values_.dem_resolution.value_or(10.0F);
    if (ImGui::InputFloat("DEM Resolution (m)", &dem_resolution, 1, 10)) {
      values_.dem_resolution = dem_resolution;
      changed = true;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("m/pixel");
    }

    ImGui::Spacing();

    char* visualization_texture = visualization_texture_path_.data();
    ImGui::InputText("Visualization Texture", visualization_texture, 255,
                     ImGuiInputTextFlags_ReadOnly);
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Path to the visualization texture");
    }

    ImGui::EndTabItem();
  }
  return changed;
}

bool ui::ParametersPanel::ParametersTab() {
  bool changed = false;

  if (!ImGui::BeginTabItem("Parameters")) {
    return changed;
  }

  ImGui::AlignTextToFramePadding();
  ImGui::Text("Initialization");

  float total_fluid_volume = values_.total_fluid_volume.value_or(0.00F);
  if (ImGui::InputFloat("Total Tailings Volume (m³)", &total_fluid_volume,
                        500000.0F, 1000000.0F, "%.2f")) {
    values_.total_fluid_volume = total_fluid_volume;
    changed = true;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Total volume of tailings to simulate");
  }

  float initial_particle_spacing =
      values_.initial_particle_spacing.value_or(0.010F);
  if (ImGui::InputFloat("Initial Particle Spacing (m)",
                        &initial_particle_spacing, 0.5F, 1.0F, "%.3f")) {
    values_.initial_particle_spacing = initial_particle_spacing;
    changed = true;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Distance between particles in the initial state");
  }

  int voxel_max_particles =
      static_cast<int>(values_.voxel_max_particles.value_or(16));
  if (ImGui::SliderInt("Max Particles Per Voxel", &voxel_max_particles, 1, 64,
                       "%d")) {
    values_.voxel_max_particles = static_cast<uint32_t>(voxel_max_particles);
    changed = true;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Maximum particles per bucket cell");
  }

  ImGui::Spacing();

  ImGui::Text("Fluid Properties");

  float rest_density = values_.rest_density.value_or(500.00F);
  if (ImGui::SliderFloat("Rest Density (kg/m³)", &rest_density, 500.0F,
                         10000.0F, "%.2f")) {
    values_.rest_density = rest_density;
    changed = true;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Reference density of the fluid");
  }

  float gas_constant = values_.gas_constant.value_or(1.00F);
  if (ImGui::SliderFloat("Gas Constant (Pa·m³/kg)", &gas_constant, 1.0F,
                         1000.0F, "%.2f")) {
    values_.gas_constant = gas_constant;
    changed = true;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Parameter k in the equation of state: p = k(ρ - ρ0)");
  }

  float friction = values_.friction.value_or(0.0F);
  if (ImGui::SliderFloat("Friction Coefficient", &friction, 0.0F, 0.1F,
                         "%.4f")) {
    values_.friction = friction;
    changed = true;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Boundary friction coefficient");
  }

  float viscosity = values_.viscosity.value_or(1.00F);
  if (ImGui::SliderFloat("Plastic Viscosity (cP)", &viscosity, 1.0F, 10000.0F,
                         "%.2f")) {
    values_.viscosity = viscosity;
    changed = true;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Dynamic viscosity of the fluid (Bingham model)");
  }

  float yield_stress = values_.yield_stress.value_or(0.000F);
  if (ImGui::SliderFloat("Yield Stress (Pa)", &yield_stress, 0.0F, 10000.0F,
                         "%.3f")) {
    values_.yield_stress = yield_stress;
    changed = true;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Minimum stress for flow (Bingham model)");
  }

  float coefficient_of_restitution =
      values_.coefficient_of_restitution.value_or(0.067F);
  if (ImGui::SliderFloat("Coefficient of Restitution",
                         &coefficient_of_restitution, 0.0F, 1.0F, "%.3f")) {
    values_.coefficient_of_restitution = coefficient_of_restitution;
    changed = true;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Elasticity of contact (0 = perfectly inelastic)");
  }

  ImGui::EndTabItem();

  return changed;
}

void ui::ParametersPanel::SaveFileDialog() {
  IGFD::FileDialogConfig config{};
  config.path = std::filesystem::current_path().string();
  config.filePathName = simulation_config_path_;
  config.flags =
      ImGuiFileDialogFlags_ConfirmOverwrite | ImGuiFileDialogFlags_Modal;

  ImGuiFileDialog::Instance()->OpenDialog(kSaveSimulationDialogKey,
                                          "Save Simulation Settings",
                                          "YAML files{.yaml,.yml},.*", config);
}

void ui::ParametersPanel::LoadFileDialog() {
  IGFD::FileDialogConfig config{};
  config.path = std::filesystem::current_path().string();
  config.filePathName = simulation_config_path_;
  config.flags = ImGuiFileDialogFlags_Modal;

  ImGuiFileDialog::Instance()->OpenDialog(kLoadSimulationDialogKey,
                                          "Open Simulation Settings",
                                          "YAML files{.yaml,.yml},.*", config);
}

void ui::ParametersPanel::DisplayFileDialogs() {
  if (ImGuiFileDialog::Instance()->Display(
          kUploadDialogKey, ImGuiWindowFlags_NoCollapse, ImVec2(600.0F, 450.0F),
          ImVec2(700.0F, 525.0F))) {
    if (ImGuiFileDialog::Instance()->IsOk()) {
      events_.uploaded_dem_texture_path =
          ImGuiFileDialog::Instance()->GetFilePathName();
    }
    ImGuiFileDialog::Instance()->Close();
  }

  if (ImGuiFileDialog::Instance()->Display(
          kUploadTerrainDialogKey, ImGuiWindowFlags_NoCollapse,
          ImVec2(600.0F, 450.0F), ImVec2(700.0F, 525.0F))) {
    if (ImGuiFileDialog::Instance()->IsOk()) {
      events_.uploaded_visualization_texture_path =
          ImGuiFileDialog::Instance()->GetFilePathName();
    }
    ImGuiFileDialog::Instance()->Close();
  }

  if (ImGuiFileDialog::Instance()->Display(
          kSaveSimulationDialogKey, ImGuiWindowFlags_NoCollapse,
          ImVec2(600.0F, 450.0F), ImVec2(700.0F, 525.0F))) {
    if (ImGuiFileDialog::Instance()->IsOk()) {
      events_.save_simulation_path =
          ImGuiFileDialog::Instance()->GetFilePathName();
    }
    ImGuiFileDialog::Instance()->Close();
  }

  if (ImGuiFileDialog::Instance()->Display(
          kLoadSimulationDialogKey, ImGuiWindowFlags_NoCollapse,
          ImVec2(600.0F, 450.0F), ImVec2(700.0F, 525.0F))) {
    if (ImGuiFileDialog::Instance()->IsOk()) {
      events_.load_simulation_path =
          ImGuiFileDialog::Instance()->GetFilePathName();
    }
    ImGuiFileDialog::Instance()->Close();
  }
}

void ui::ParametersPanel::DisplayModals() {
  if (ImGui ::BeginPopupModal(kHelpModalKey, &help_modal_opened_)) {
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Keybinds");

    ImGui::PushStyleColor(
        ImGuiCol_Text,
        {0.8784313725490196, 0.6588235294117647, 0.09803921568627451, 1});
    ImGui::Bullet();
    ImGui::PopStyleColor();
    ImGui::TextColored(
        {0.8784313725490196, 0.6588235294117647, 0.09803921568627451, 1},
        "Left Click + Drag = Camera pan");

    ImGui::PushStyleColor(
        ImGuiCol_Text,
        {0.8784313725490196, 0.6588235294117647, 0.09803921568627451, 1});
    ImGui::Bullet();
    ImGui::PopStyleColor();
    ImGui::TextColored(
        {0.8784313725490196, 0.6588235294117647, 0.09803921568627451, 1},
        "Ctrl + Scroll = Zoom");

    ImGui::EndPopup();
  }
}
