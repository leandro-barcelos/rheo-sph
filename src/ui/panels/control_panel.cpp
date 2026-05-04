#include "control_panel.h"

#include "IconsFontAwesome6.h"
#include "imgui.h"

ui::ControlPanel::Events ui::ControlPanel::Draw(bool simulation_running,
                                                bool can_play) {
  Events events{};

  ImGui::SetNextWindowPos(ImVec2(12.0F, 36.0F), ImGuiCond_FirstUseEver);

  ImGuiWindowFlags const window_flags =
      ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_AlwaysAutoResize;

  if (ImGui::Begin("Controls", nullptr, window_flags)) {
    ImGui::BeginDisabled(simulation_running || !can_play);
    if (ImGui::Button(ICON_FA_PLAY)) {
      events.play_pressed = true;
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
      if (can_play) {
        ImGui::SetTooltip("Play");
      } else {
        ImGui::SetTooltip("Define all parameters and upload a texture first");
      }
    }

    ImGui::SameLine();
    ImGui::BeginDisabled(!simulation_running);
    if (ImGui::Button(ICON_FA_PAUSE)) {
      events.pause_pressed = true;
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
      ImGui::SetTooltip("Pause");
    }

    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_ROTATE_RIGHT)) {
      events.reset_pressed = true;
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
      ImGui::SetTooltip("Reset");
    }

    ImGui::Separator();

    if (ImGui::CollapsingHeader("Keybinds")) {
      ImGui::TextColored(
          {0.8784313725490196, 0.6588235294117647, 0.09803921568627451, 1},
          "LeftClick + Drag = Pan");

      ImGui::TextColored(
          {0.8784313725490196, 0.6588235294117647, 0.09803921568627451, 1},
          "Ctrl + Scroll = Zoom");
    }
  }
  ImGui::End();

  return events;
}
