#include "camera.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <variant>

#include "rheo-sph/src/core/input_events.h"

void renderer::Camera::ProcessInput(core::WindowSize const& window_size,
                                    core::InputEvent const& events,
                                    bool ignore_mouse_events) {
  if (ignore_mouse_events) {
    return;
  }

  for (auto const& event : events.events) {
    if (std::holds_alternative<core::MouseMovedEvent>(event)) {
      auto move_event = std::get<core::MouseMovedEvent>(event);
      auto move_vector =
          ScreenToWorldSpace(window_size, move_event.dx, move_event.dy);

      if (events.is_holding_mouse_left_click) {
        // Zoom = Ctrl + Left click + Move cursor
        if (events.is_holding_control) {
          float zoom = std::sqrt((move_vector[0] * move_vector[0]) +
                                 (move_vector[1] * move_vector[1]));
          // ProcessZoom(zoom);
        } else if (events.is_holding_shift) {
        } else {
          // Pan = Left click + Move cursor
          ProcessPan(move_vector[0], move_vector[1]);
        }
      }
    } else if (std::holds_alternative<core::MouseScrollEvent>(event)) {
      auto scroll_event = std::get<core::MouseScrollEvent>(event);

      // Zoom = Ctrl + Scroll
      if (events.is_holding_control) {
        // ProcessZoom(static_cast<float>(scroll_event.dy));
      }
      // Horizontal Pan = Shift + Scroll
      else if (events.is_holding_shift) {
        ProcessPan(static_cast<float>(scroll_event.dy) * 0.05F, 0.0F);
      }
      // Vertical Pan = Scroll
      else {
        ProcessPan(0.0F, static_cast<float>(scroll_event.dy) * 0.05F);
      }
    }
  }
}

glm::mat4 renderer::Camera::ViewMatrix() const {
  return glm::lookAt(position_, position_ + front_, up_);
}

glm::mat4 renderer::Camera::ProjectionMatrix(float aspect_ratio,
                                             float near_plane,
                                             float far_plane) const {
  return glm::perspective(glm::radians(zoom_), aspect_ratio, near_plane,
                          far_plane);
}

void renderer::Camera::InitTopView(glm::vec3 const& bounds_min,
                   glm::vec3 const& bounds_max) {
  constexpr float kHeightMultiplier = 1.1F;
  constexpr float kFramePadding = 0.55F;

  glm::vec3 const bounds_center = (bounds_min + bounds_max) * 0.5F;
  glm::vec3 const bounds_size = bounds_max - bounds_min;
  float const max_dimension = std::max(bounds_size[0], bounds_size[2]);

  position_ = bounds_center;
  position_[1] = bounds_max[1] + (max_dimension * kHeightMultiplier);

  // Align axes for a straight top-down view.
  front_ = {0.0F, -1.0F, 0.0F};
  up_ = {0.0F, 0.0F, -1.0F};
  right_ = {1.0F, 0.0F, 0.0F};

  float const distance = std::max(position_[1] - bounds_center[1], 1e-4F);
  float const required_fov =
    2.0F * std::atan((max_dimension * kFramePadding) / distance) *
    (180.0F / std::numbers::pi_v<float>);
  zoom_ = std::clamp(required_fov, 10.0F, 120.0F);
}

void renderer::Camera::ProcessPan(float x_offset, float y_offset) {
  position_ -= (right_ * x_offset + up_ * -y_offset) * movement_speed_;
}

// void renderer::Camera::ProcessZoom(glm::vec3 mouse_world_pos, float z_offset)
// {
//   std::cout << std::format("Zoom = {}", z_offset) << '\n';
// }

glm::vec3 renderer::Camera::ScreenToWorldSpace(
    core::WindowSize const& window_size, double xpos, double ypos) {
  auto width = static_cast<float>(window_size.width);
  auto height = static_cast<float>(window_size.height);

  if (width <= 0.0F || height <= 0.0F) {
    return {};
  }

  return {static_cast<float>(xpos) / width, static_cast<float>(ypos) / height,
          0.0F};
}
