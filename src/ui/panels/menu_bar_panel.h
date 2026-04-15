#ifndef RHEOSPH_UI_MENU_BAR_PANEL_H
#define RHEOSPH_UI_MENU_BAR_PANEL_H

#include <optional>
#include <string>

namespace ui {

class MenuBarPanel {
 public:
  struct Events {
    std::optional<std::string> uploaded_texture_path;
    std::optional<std::string> save_simulation_path;
    std::optional<std::string> load_simulation_path;
  } __attribute__((aligned(128)));

  MenuBarPanel();

  [[nodiscard]] Events Draw();
  void SetElevationTexturePath(std::string const& path);
  void SetSimulationConfigPath(std::string const& path);

 private:
  std::string elevation_texture_path_;
  std::string simulation_config_path_;
};

}  // namespace ui

#endif  // !RHEOSPH_UI_MENU_BAR_PANEL_H
