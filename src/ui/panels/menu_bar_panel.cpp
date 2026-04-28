#include "menu_bar_panel.h"

#include <filesystem>

#include "ImGuiFileDialog.h"
#include "imgui.h"

namespace {
constexpr const char* kUploadDialogKey = "UploadElevationTextureDialog";
constexpr const char* kSaveSimulationDialogKey = "SaveSimulationDialog";
constexpr const char* kLoadSimulationDialogKey = "LoadSimulationDialog";
}  // namespace

ui::MenuBarPanel::MenuBarPanel() {
  SetElevationTexturePath("");
  SetSimulationConfigPath("");
}

ui::MenuBarPanel::Events ui::MenuBarPanel::Draw() {
  Events events{};

  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Upload elevation texture...")) {
        IGFD::FileDialogConfig config{};
        config.path = std::filesystem::current_path().string();
        config.filePathName = elevation_texture_path_;
        config.flags = ImGuiFileDialogFlags_Modal;

        ImGuiFileDialog::Instance()->OpenDialog(
            kUploadDialogKey, "Upload Elevation Texture",
            "GeoTiff files{.tif,.tiff},.*", config);
      }

      if (ImGui::MenuItem("Save simulation...")) {
        IGFD::FileDialogConfig config{};
        config.path = std::filesystem::current_path().string();
        config.filePathName = simulation_config_path_;
        config.flags =
            ImGuiFileDialogFlags_ConfirmOverwrite | ImGuiFileDialogFlags_Modal;

        ImGuiFileDialog::Instance()->OpenDialog(
            kSaveSimulationDialogKey, "Save Simulation Parameters",
            "YAML files{.yaml,.yml},.*", config);
      }

      if (ImGui::MenuItem("Load simulation...")) {
        IGFD::FileDialogConfig config{};
        config.path = std::filesystem::current_path().string();
        config.filePathName = simulation_config_path_;
        config.flags = ImGuiFileDialogFlags_Modal;

        ImGuiFileDialog::Instance()->OpenDialog(
            kLoadSimulationDialogKey, "Load Simulation Parameters",
            "YAML files{.yaml,.yml},.*", config);
      }

      ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
  }

  if (ImGuiFileDialog::Instance()->Display(
          kUploadDialogKey, ImGuiWindowFlags_NoCollapse, ImVec2(600.0F, 450.0F),
          ImVec2(700.0F, 525.0F))) {
    if (ImGuiFileDialog::Instance()->IsOk()) {
      events.uploaded_texture_path =
          ImGuiFileDialog::Instance()->GetFilePathName();
    }
    ImGuiFileDialog::Instance()->Close();
  }

  if (ImGuiFileDialog::Instance()->Display(
          kSaveSimulationDialogKey, ImGuiWindowFlags_NoCollapse,
          ImVec2(600.0F, 450.0F), ImVec2(700.0F, 525.0F))) {
    if (ImGuiFileDialog::Instance()->IsOk()) {
      events.save_simulation_path =
          ImGuiFileDialog::Instance()->GetFilePathName();
    }
    ImGuiFileDialog::Instance()->Close();
  }

  if (ImGuiFileDialog::Instance()->Display(
          kLoadSimulationDialogKey, ImGuiWindowFlags_NoCollapse,
          ImVec2(600.0F, 450.0F), ImVec2(700.0F, 525.0F))) {
    if (ImGuiFileDialog::Instance()->IsOk()) {
      events.load_simulation_path =
          ImGuiFileDialog::Instance()->GetFilePathName();
    }
    ImGuiFileDialog::Instance()->Close();
  }

  return events;
}

void ui::MenuBarPanel::SetElevationTexturePath(std::string const& path) {
  elevation_texture_path_ = path;
}

void ui::MenuBarPanel::SetSimulationConfigPath(std::string const& path) {
  simulation_config_path_ = path;
}
