#ifndef UI_CONTROLLER_H
#define UI_CONTROLLER_H

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../resources/elevation.h"
#include "../renderer/ui_texture_handle.h"
#include "../simulation/fluid_simulator.h"
#include "../ui/panels/menu_bar_panel.h"
#include "../ui/panels/parameters_panel.h"

// X11 headers (pulled in by GLFW on Linux) may define `None` as a macro.
// Undefine it so we can use `SimAction::None` safely.
#ifdef None
#undef None
#endif

namespace app {

// NOLINTNEXTLINE(altera-struct-pack-align) - packing is not desirable here.
struct UiIntent {
  enum class SimAction : std::uint8_t { kNone, kPlay, kPause, kReset };

  std::optional<simulation::FluidSimulator::Parameters> built_parameters;
  std::optional<std::string> new_texture_path;
  std::optional<std::string> new_terrain_texture_path;
  std::optional<std::string> save_path;
  std::optional<std::string> load_path;
  SimAction sim_action = SimAction::kNone;
  bool parameters_changed = false;
  // True when elevation data changed (new texture uploaded or config loaded)
  // and the terrain renderer should be re-initialized.
  bool elevation_changed = false;
  // True when terrain preview texture changed and the terrain renderer should
  // be re-initialized to show the texture.
  bool terrain_texture_changed = false;
};

class UiController {
 public:
  [[nodiscard]] UiIntent Draw(bool simulation_running);

  [[nodiscard]] bool SaveSimulationConfig(std::string const& path);
  [[nodiscard]] bool LoadSimulationConfig(std::string const& path);
  [[nodiscard]] std::optional<simulation::FluidSimulator::Parameters> BuildParameters() const;
  [[nodiscard]] std::string const& GetElevationTexturePath() const;
  [[nodiscard]] std::string const& GetTerrainTexturePath() const;

  void NotifyTextureLoaded(renderer::UiTextureHandle handle, void* imgui_id);
  void ClearTexturePreview();
  void ClearTerrainPreviewTexture();
  [[nodiscard]] ui::ParametersPanel::Values const& GetParameterValues() const;

 private:
  ui::ParametersPanel parameters_panel_;
  std::string pending_elevation_texture_path_;
  std::string pending_terrain_texture_path_;
  std::shared_ptr<const std::vector<resources::Elevation>> pending_elevation_samples_;
  std::array<uint32_t, 2> pending_elevation_dimensions_{0U, 0U};
  ui::MenuBarPanel menu_bar_panel_;
};

}  // namespace app

#endif  // UI_CONTROLLER_H
