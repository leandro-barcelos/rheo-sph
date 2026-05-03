#ifndef RHEOSPH_UI_TOP_BAR_PANEL_H
#define RHEOSPH_UI_TOP_BAR_PANEL_H

namespace ui {

class ControlPanel {
 public:
  struct alignas(4) Events {
    bool play_pressed = false;
    bool pause_pressed = false;
    bool reset_pressed = false;
  };

  [[nodiscard]] static Events Draw(bool simulation_running, bool can_play);
};

}  // namespace ui

#endif  // !RHEOSPH_UI_TOP_BAR_PANEL_H
