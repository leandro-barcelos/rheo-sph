#ifndef RHEOSPH_UI_MENU_BAR_PANEL_H
#define RHEOSPH_UI_MENU_BAR_PANEL_H

#include <optional>
#include <string>

namespace ui {

class MenuBarPanel {
 public:
  MenuBarPanel();

  [[nodiscard]] std::optional<std::string> Draw();
  void SetElevationTexturePath(std::string const& path);

 private:
  std::string elevation_texture_path_;
};

}  // namespace ui

#endif  // !RHEOSPH_UI_MENU_BAR_PANEL_H
