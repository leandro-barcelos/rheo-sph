#include "top_bar_panel.h"

#include "IconsFontAwesome6.h"
#include "imgui.h"

ui::TopBarPanel::Events ui::TopBarPanel::Draw(bool simulation_running,
                                              bool can_play) {
  Events events{};

  ImGuiViewport* viewport = ImGui::GetMainViewport();
  float const menu_bar_height = ImGui::GetFrameHeight();

  ImGui::SetNextWindowPos(
      ImVec2(viewport->Pos.x, viewport->Pos.y + menu_bar_height));
  ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, kToolbarHeight));
  ImGui::SetNextWindowViewport(viewport->ID);

  ImGuiWindowFlags const window_flags =
      ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoDecoration |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
      ImGuiWindowFlags_NoFocusOnAppearing |
      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

  if (ImGui::Begin("TopToolbarWindow", nullptr, window_flags)) {
    float const button_size = ImGui::GetFrameHeight();
    float const y_offset = (kToolbarHeight - button_size) * 0.5F;
    if (y_offset > 0.0F) {
      ImGui::SetCursorPosY(y_offset);
    }
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 6.0F);

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
  }
  ImGui::End();

  return events;
}
