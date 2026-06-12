#ifndef RHEOSPH_FILE_EVENT_H
#define RHEOSPH_FILE_EVENT_H

#include <format>

#include "rheo-sph/src/events/event.h"

namespace events {

class DEMFileUploadedEvent : public Event {
 public:
  EVENT_TYPE("DEMFileUploadedEvent")

  explicit DEMFileUploadedEvent(std::string dem_file_path)
      : dem_file_path_(std::move(dem_file_path)) {}

  [[nodiscard]] std::string ToString() const override {
    return std::format("DEMFileUploadedEvent: {}", dem_file_path_);
  }

  [[nodiscard]] std::string const& GetDEMFilePath() const {
    return dem_file_path_;
  }

 private:
  std::string dem_file_path_;
};

class TerrainTextureUploadedEvent : public Event {
 public:
  EVENT_TYPE("TerrainTextureUploadedEvent")

  explicit TerrainTextureUploadedEvent(
      std::string terrain_texture_path)
      : terrain_texture_path_(std::move(terrain_texture_path)) {}

  [[nodiscard]] std::string ToString() const override {
    return std::format("TerrainTextureUploadedEvent: {}",
                       terrain_texture_path_);
  }

 private:
  std::string terrain_texture_path_;
};

class SimulationFileLoadedEvent : public Event {
 public:
  EVENT_TYPE("SimulationFileLoadedEvent")

  explicit SimulationFileLoadedEvent(std::string load_simulation_path)
      : load_simulation_path_(std::move(load_simulation_path)) {}

  [[nodiscard]] std::string ToString() const override {
    return std::format("SimulationFileLoadedEvent: {}", load_simulation_path_);
  }

 private:
  std::string load_simulation_path_;
};

class SimulationSaveRequestedEvent : public Event {
 public:
  EVENT_TYPE("SimulationSaveRequestedEvent")

  explicit SimulationSaveRequestedEvent(std::string save_simulation_path)
      : save_simulation_path_(std::move(save_simulation_path)) {}

  [[nodiscard]] std::string ToString() const override {
    return std::format("SimulationSaveRequestedEvent: {}",
                       save_simulation_path_);
  }

 private:
  std::string save_simulation_path_;
};

}  // namespace events

#endif  // !RHEOSPH_FILE_EVENT_H
