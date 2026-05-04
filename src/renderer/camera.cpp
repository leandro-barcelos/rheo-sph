#include "camera.h"

#include <algorithm>
#include <cmath>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/matrix.hpp>
#include <numbers>
#include <optional>

#include "rheo-sph/src/core/input_events.h"

void renderer::Camera::ProcessMouseDragEvent(
    core::WindowSize const& window_size,
    core::MouseDragEvent const& mouse_drag) {
  if (!mouse_drag.current_position.has_value()) {
    pan_anchor_world_ =
        RaycastToPlane(window_size, mouse_drag.anchor_position.x,
                       mouse_drag.anchor_position.y);
    return;
  }

  if (!pan_anchor_world_.has_value()) {
    return;
  }

  auto current_position_world =
      RaycastToPlane(window_size, mouse_drag.current_position->x,
                     mouse_drag.current_position->y);

  if (!current_position_world.has_value()) {
    return;
  }

  auto delta = *pan_anchor_world_ - *current_position_world;
  position_ += delta;
}

void renderer::Camera::ProcessScrollEvent(
    core::ScrollEvent const& scroll, core::InputModifiers const& modifiers) {
  if (modifiers.control) {
    ProcessZoom(static_cast<float>(scroll.delta_y));
  }
}

void renderer::Camera::ProcessInput(core::WindowSize const& window_size,
                                    core::InputState const& input_state,
                                    bool ignore_mouse_events) {
  if (ignore_mouse_events) {
    return;
  }

  if (input_state.mouse_drag_event.has_value()) {
    ProcessMouseDragEvent(window_size, *input_state.mouse_drag_event);
  } else {
    pan_anchor_world_ = std::nullopt;
    if (input_state.scroll_event.has_value()) {
      ProcessScrollEvent(*input_state.scroll_event, input_state.modifiers);
    }
  }
}

glm::mat4 renderer::Camera::ViewMatrix() const {
  return glm::lookAt(position_, position_ + front_, up_);
}

glm::mat4 renderer::Camera::ProjectionMatrix(float aspect_ratio,
                                             float near_plane) const {
  return glm::perspective(glm::radians(zoom_), aspect_ratio, near_plane,
                          far_plane_);
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

  far_plane_ = std::max(position_[1] - bounds_min[1] + max_dimension, 100.0F);
}

void renderer::Camera::ProcessScrollPan(float x_offset, float y_offset) {
  position_ -= (right_ * x_offset + up_ * -y_offset) * movement_speed_;
}

void renderer::Camera::ProcessZoom(float z_offset) {
  constexpr float kMinZoom = 5.0F;
  constexpr float kMaxZoom = 120.0F;

  zoom_ -= z_offset * mouse_sensitivity_;
  zoom_ = std::clamp(zoom_, kMinZoom, kMaxZoom);
}

glm::vec3 renderer::Camera::ScreenToWorldPosition(
    core::WindowSize const& window_size, double xpos, double ypos,
    float depth_ndc) const {
  float const aspect = static_cast<float>(window_size.width) /
                       static_cast<float>(window_size.height);

  // Reconstruct NDC — Vulkan is [-1,1] x [-1,1] x [0,1]
  float const ndc_x = ((static_cast<float>(std::floor(xpos)) + 0.5F) /
                       static_cast<float>(window_size.width) * 2.0F) -
                      1.0F;
  float const ndc_y = 1.0F - ((static_cast<float>(std::floor(ypos)) + 0.5F) /
                              static_cast<float>(window_size.height) * 2.0F);

  glm::mat4 const inv = glm::inverse(ProjectionMatrix(aspect) * ViewMatrix());

  glm::vec4 const clip{ndc_x, ndc_y, depth_ndc, 1.0F};
  glm::vec4 world = inv * clip;
  world /= world[3];

  return glm::vec3{world};
}

std::optional<glm::vec3> renderer::Camera::RaycastToPlane(
    core::WindowSize const& window_size, double xpos, double ypos,
    float plane_y) const {
  auto near_point = ScreenToWorldPosition(window_size, xpos, ypos, 0.0F);
  auto far_point = ScreenToWorldPosition(window_size, xpos, ypos, 1.0F);

  glm::vec3 const ray_dir = glm::normalize(far_point - near_point);

  // Quit if ray and plane are parallel
  if (std::abs(ray_dir[1]) < 1e-6F) {
    return std::nullopt;
  }

  float const plane_depth = (plane_y - near_point[1]) / ray_dir[1];

  // Intersection is behind the camera
  if (plane_depth < 0.0F) {
    return std::nullopt;
  }

  return near_point + plane_depth * ray_dir;
}
