#include "menu_bar_panel.h"

#include <filesystem>

#include "imgui.h"
#include "ImGuiFileDialog.h"

namespace {
constexpr const char* kUploadDialogKey = "UploadElevationTextureDialog";
}

ui::MenuBarPanel::MenuBarPanel() { SetElevationTexturePath(""); }

std::optional<std::string> ui::MenuBarPanel::Draw() {
  std::optional<std::string> uploaded_texture_path;

  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Upload elevation texture...")) {
        IGFD::FileDialogConfig config{};
        config.path = std::filesystem::current_path().string();
        config.filePathName = elevation_texture_path_;
        config.flags = ImGuiFileDialogFlags_Modal;

        ImGuiFileDialog::Instance()->OpenDialog(
            kUploadDialogKey, "Upload Elevation Texture",
            "Image files{.png,.jpg,.jpeg,.bmp,.tga,.hdr,.exr,.tif,.tiff},.*",
            config);
      }

      ImGui::MenuItem("Save simulation", nullptr, false, false);
      ImGui::MenuItem("Load simulation", nullptr, false, false);

      ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
  }

    if (ImGuiFileDialog::Instance()->Display(
      kUploadDialogKey, ImGuiWindowFlags_NoCollapse, ImVec2(600.0F, 450.0F),
      ImVec2(700.0F, 525.0F))) {
    if (ImGuiFileDialog::Instance()->IsOk()) {
      uploaded_texture_path =
          ImGuiFileDialog::Instance()->GetFilePathName(IGFD_ResultMode_KeepInputFile);
    }
    ImGuiFileDialog::Instance()->Close();
  }

  return uploaded_texture_path;
}

void ui::MenuBarPanel::SetElevationTexturePath(std::string const& path) {
  elevation_texture_path_ = path;
}
