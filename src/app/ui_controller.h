#ifndef UI_CONTROLLER_H
#define UI_CONTROLLER_H

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../resources/elevation.h"
#include "../simulation/fluid_simulator.h"
#include "../ui/panels/parameters_panel.h"

// X11 headers (pulled in by GLFW on Linux) may define `None` as a macro.
// Undefine it so we can use `SimAction::None` safely.
#ifdef None
#undef None
#endif

namespace app {

struct LoadedConfig {
  ui::ParametersPanel::Values panel_values;
  std::shared_ptr<const std::vector<resources::Elevation>> elevation_samples;
  std::array<uint32_t, 2> elevation_dimensions{0U, 0U};
  std::string elevation_texture_path;
  std::string terrain_texture_path;
} __attribute__((packed));

// NOLINTNEXTLINE(altera-struct-pack-align) - packing is not desirable here.
struct UiIntent {
  enum class SimAction : std::uint8_t { kNone, kPlay, kPause, kReset };
  std::shared_ptr<const std::vector<resources::Elevation>> elevation_samples;
  std::array<uint32_t, 2> elevation_dimensions{0U, 0U};
  std::string dem_texture_path;
  std::string visualization_texture_path;

  std::optional<simulation::FluidSimulator::Parameters> built_parameters;
  std::optional<std::string> save_path;
  std::optional<std::string> load_path;
  SimAction sim_action = SimAction::kNone;
  bool parameters_changed = false;
  bool elevation_changed = false;
  bool terrain_texture_changed = false;
};

class UiController {
 public:
  [[nodiscard]] UiIntent Draw(bool simulation_running);

  [[nodiscard]] bool SaveSimulationConfig(std::string const& path);
  // Returns nullopt on failure
  [[nodiscard]] static std::optional<LoadedConfig> LoadSimulationConfig(
      std::string const& path);

 private:
  ui::ParametersPanel parameters_panel_;
  std::string pending_elevation_texture_path_;
  std::string pending_terrain_texture_path_;
  std::shared_ptr<const std::vector<resources::Elevation>>
      pending_elevation_samples_;
  std::array<uint32_t, 2> pending_elevation_dimensions_{0U, 0U};
};

}  // namespace app

#endif  // UI_CONTROLLER_H
