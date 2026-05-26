#ifndef RHEOSPH_UI_TOP_BAR_PANEL_H
#define RHEOSPH_UI_TOP_BAR_PANEL_H

#include <cstdint>

namespace ui {

class ControlPanel {
 public:
  enum class DamMode : std::uint8_t {
    kNone,
    kDraw,
    kFill,
  };

  struct alignas(8) Events {
    bool play_pressed = false;
    bool pause_pressed = false;
    bool reset_pressed = false;
    bool draw_dam_pressed = false;
    bool fill_dam_pressed = false;
  };

  [[nodiscard]] Events Draw(bool simulation_running, bool can_play,
                                   bool can_edit_dam);

  [[nodiscard]] DamMode const& DamMode() const {return dam_mode_;}

 private:
  enum DamMode dam_mode_ = DamMode::kNone;

  void DrawDamControls(bool simulation_running, bool can_edit_dam,
                     ui::ControlPanel::Events& events);
};

}  // namespace ui

#endif  // !RHEOSPH_UI_TOP_BAR_PANEL_H
