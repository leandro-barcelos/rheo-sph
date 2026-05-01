#ifndef RHEOSPH_CAMERA_H
#define RHEOSPH_CAMERA_H

#include <glm/ext/vector_float3.hpp>
#include <glm/glm.hpp>

#include "../core/window.h"
#include "rheo-sph/src/core/input_events.h"

namespace renderer {

class Camera {
 public:
  explicit Camera(glm::vec3 position = glm::vec3(0.0F, 0.0F, 0.0F))
      : position_(position),
        front_(0.0F, -1.0F, 0.0F),
        up_(0.0F, 0.0F, -1.0F),
        right_(1.0F, 0.0F, 0.0F),
        world_up_(0.0F, 1.0F, 0.0F) {}

  void ProcessInput(core::WindowSize const& window_size,
                    core::InputEvent const& events, bool ignore_mouse_events);

  [[nodiscard]] glm::mat4 ViewMatrix() const;
  [[nodiscard]] glm::mat4 ProjectionMatrix(float aspect_ratio,
                                           float near_plane = 0.1F) const;
  void InitTopView(glm::vec3 const& bounds_min, glm::vec3 const& bounds_max);
  [[nodiscard]] glm::vec3 Position() const { return position_; }
  [[nodiscard]] glm::vec3 Front() const { return front_; }
  [[nodiscard]] float Zoom() const { return zoom_; }

 private:
  glm::vec3 position_;
  glm::vec3 front_;
  glm::vec3 up_;
  glm::vec3 right_;
  glm::vec3 world_up_;
  float movement_speed_{1.0F};
  float mouse_sensitivity_{1.0F};
  float zoom_{45.0F};
  float far_plane_{100.0F};

  void ProcessPan(float x_offset = 0.0F, float y_offset = 0.0F);
  void ProcessZoom(float z_offset = 0.0F);
  [[nodiscard]] static glm::vec3 ScreenToWorldSpace(
      core::WindowSize const& window_size, double xpos, double ypos);
};

}  // namespace renderer

#endif  // !RHEOSPH_CAMERA_H
