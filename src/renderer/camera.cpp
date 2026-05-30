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

#include "rheo-sph/src/core/mouse_codes.h"
#include "rheo-sph/src/events/event_manager.h"
#include "rheo-sph/src/events/keyboard_event.h"
#include "rheo-sph/src/events/mouse_event.h"
#include "rheo-sph/src/events/ui_event.h"
#include "rheo-sph/src/events/window_event.h"

renderer::Camera::Camera(glm::vec3 position,
                         core::WindowSize initial_window_size)
    : position_(position),
      front_(0.0F, -1.0F, 0.0F),
      up_(0.0F, 0.0F, -1.0F),
      right_(1.0F, 0.0F, 0.0F),
      world_up_(0.0F, 1.0F, 0.0F),
      window_size_(initial_window_size) {
  mouse_button_pressed_handler_ =
      [this](events::MouseButtonPressedEvent const& event) {
        OnMouseButtonPressedEvent(event);
      };
  mouse_button_released_handler_ =
      [this](events::MouseButtonReleasedEvent const& event) {
        OnMouseButtonReleasedEvent(event);
      };
  mouse_scrolled_handler_ = [this](events::MouseScrolledEvent const& event) {
    OnMouseScrolledEvent(event);
  };
  mouse_moved_handler_ = [this](events::MouseMovedEvent const& event) {
    OnMouseMovedEvent(event);
  };

  key_pressed_handler_ = [this](events::KeyPressedEvent const& event) {
    OnKeyPressedEvent(event);
  };
  key_released_handler_ = [this](events::KeyReleasedEvent const& event) {
    OnKeyReleasedEvent(event);
  };
  ui_focused_handler_ = [this](events::UiFocusedEvent const& event) {
    OnUiFocusedEvent(event);
  };
  ui_unfocused_handler_ = [this](events::UiUnfocusedEvent const& event) {
    OnUiUnfocusedEvent(event);
  };
  window_resized_handler_ = [this](events::WindowResizedEvent const& event) {
    OnWindowResizedEvent(event);
  };

  events::Subscribe<events::MouseButtonPressedEvent>(
      mouse_button_pressed_handler_);
  events::Subscribe<events::MouseButtonReleasedEvent>(
      mouse_button_released_handler_);
  events::Subscribe<events::MouseScrolledEvent>(mouse_scrolled_handler_);
  events::Subscribe<events::MouseMovedEvent>(mouse_moved_handler_);
  events::Subscribe<events::KeyPressedEvent>(key_pressed_handler_);
  events::Subscribe<events::KeyReleasedEvent>(key_released_handler_);
  events::Subscribe<events::UiFocusedEvent>(ui_focused_handler_);
  events::Subscribe<events::UiUnfocusedEvent>(ui_unfocused_handler_);
  events::Subscribe<events::WindowResizedEvent>(window_resized_handler_);
}

renderer::Camera::~Camera() {
  events::Unsubscribe<events::MouseButtonPressedEvent>(
      mouse_button_pressed_handler_);
  events::Unsubscribe<events::MouseButtonReleasedEvent>(
      mouse_button_released_handler_);
  events::Unsubscribe<events::MouseScrolledEvent>(mouse_scrolled_handler_);
  events::Unsubscribe<events::MouseMovedEvent>(mouse_moved_handler_);
  events::Unsubscribe<events::KeyPressedEvent>(key_pressed_handler_);
  events::Unsubscribe<events::KeyReleasedEvent>(key_released_handler_);
  events::Unsubscribe<events::UiFocusedEvent>(ui_focused_handler_);
  events::Unsubscribe<events::UiUnfocusedEvent>(ui_unfocused_handler_);
  events::Unsubscribe<events::WindowResizedEvent>(window_resized_handler_);
}

void renderer::Camera::OnMouseButtonPressedEvent(
    events::MouseButtonPressedEvent const& event) {
  if (ui_focused_) {
    return;
  }

  if (event.GetButton() == core::MouseCode::kButtonLeft) {
    mouse_left_button_held_ = true;
  }
}

void renderer::Camera::OnMouseButtonReleasedEvent(
    events::MouseButtonReleasedEvent const& event) {
  if (ui_focused_) {
    return;
  }

  if (event.GetButton() == core::MouseCode::kButtonLeft) {
    mouse_left_button_held_ = false;
    current_mouse_screen_position_ = std::nullopt;
  }
}

void renderer::Camera::OnMouseScrolledEvent(
    events::MouseScrolledEvent const& event) {
  if (ui_focused_) {
    return;
  }

  if (!is_control_pressed_) {
    return;
  }

  float scroll_y_offset = event.GetYOffset();

  constexpr float kMinZoom = 5.0F;
  constexpr float kMaxZoom = 120.0F;

  zoom_ -= scroll_y_offset * mouse_sensitivity_;
  zoom_ = std::clamp(zoom_, kMinZoom, kMaxZoom);

  scroll_y_offset = 0.0F;
}

void renderer::Camera::OnMouseMovedEvent(events::MouseMovedEvent const& event) {
  if (ui_focused_) {
    return;
  }

  if (!mouse_left_button_held_) {
    return;
  }

  if (!current_mouse_screen_position_.has_value()) {
    current_mouse_screen_position_ =
        glm::vec2(event.GetMouseX(), event.GetMouseY());
    return;
  }

  auto prev_mouse_screen_position = *current_mouse_screen_position_;

  current_mouse_screen_position_ =
      glm::vec2(event.GetMouseX(), event.GetMouseY());

  auto pan_anchor_world = RaycastToPlane(prev_mouse_screen_position[0],
                                         prev_mouse_screen_position[1]);
  auto current_position_world =
      RaycastToPlane(current_mouse_screen_position_.value()[0],
                     current_mouse_screen_position_.value()[1]);

  if (!pan_anchor_world.has_value() || !current_position_world.has_value()) {
    return;
  }

  auto delta = *pan_anchor_world - *current_position_world;
  position_ += delta;
}

void renderer::Camera::OnWindowResizedEvent(
    events::WindowResizedEvent const& event) {
  if (event.GetWidth() <= 0 || event.GetHeight() <= 0) {
    return;
  }

  window_size_ = {.width = event.GetWidth(), .height = event.GetHeight()};
}

void renderer::Camera::OnUiFocusedEvent(
    events::UiFocusedEvent const& /*event*/) {
  ui_focused_ = true;
  mouse_left_button_held_ = false;
  current_mouse_screen_position_ = std::nullopt;
}

void renderer::Camera::OnUiUnfocusedEvent(
    events::UiUnfocusedEvent const& /*event*/) {
  ui_focused_ = false;
  mouse_left_button_held_ = false;
  current_mouse_screen_position_ = std::nullopt;
}

void renderer::Camera::OnKeyPressedEvent(events::KeyPressedEvent const& event) {
  switch (event.GetKey()) {
    case core::kLeftControl:
    case core::kRightControl:
      is_control_pressed_ = true;
      break;
    default:
      break;
  }
}

void renderer::Camera::OnKeyReleasedEvent(
    events::KeyReleasedEvent const& event) {
  switch (event.GetKey()) {
    case core::kLeftControl:
    case core::kRightControl:
      is_control_pressed_ = false;
      break;
    default:
      break;
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

std::optional<glm::vec3> renderer::Camera::RaycastToTerrain(
    double xpos, double ypos,
    std::vector<resources::Elevation> const& elevation_samples,
    uint32_t dem_width, uint32_t dem_height, float step) const {
  auto near_point = ScreenToWorldPosition(xpos, ypos, 0.0F);
  auto far_point = ScreenToWorldPosition(xpos, ypos, 1.0F);

  if (elevation_samples.empty() || dem_width == 0 || dem_height == 0 ||
      step <= 0.0F) {
    return std::nullopt;
  }

  glm::vec3 const ray_vector = far_point - near_point;
  float const ray_length = glm::length(ray_vector);
  if (ray_length <= 0.0F) {
    return std::nullopt;
  }

  glm::vec3 const ray_dir = ray_vector / ray_length;

  for (uint32_t step_index = 0;; ++step_index) {
    float const travelled = static_cast<float>(step_index) * step;
    if (travelled > ray_length) {
      break;
    }

    glm::vec3 const current_point = near_point + travelled * ray_dir;
    float const terrain_height = resources::Elevation::GetElevation(
        elevation_samples, dem_width, dem_height, current_point);

    if (current_point[1] <= terrain_height) {
      return current_point;
    }
  }

  return std::nullopt;
}

glm::vec3 renderer::Camera::ScreenToWorldPosition(double xpos, double ypos,
                                                  float depth_ndc) const {
  int const window_width = std::max(window_size_.width, 1);
  int const window_height = std::max(window_size_.height, 1);
  float const aspect =
      static_cast<float>(window_width) / static_cast<float>(window_height);

  // Reconstruct NDC — Vulkan is [-1,1] x [-1,1] x [0,1]
  float const ndc_x = ((static_cast<float>(std::floor(xpos)) + 0.5F) /
                       static_cast<float>(window_width) * 2.0F) -
                      1.0F;
  float const ndc_y = 1.0F - ((static_cast<float>(std::floor(ypos)) + 0.5F) /
                              static_cast<float>(window_height) * 2.0F);

  glm::mat4 const inv = glm::inverse(ProjectionMatrix(aspect) * ViewMatrix());

  glm::vec4 const clip{ndc_x, ndc_y, depth_ndc, 1.0F};
  glm::vec4 world = inv * clip;
  world /= world[3];

  return glm::vec3{world};
}

std::optional<glm::vec3> renderer::Camera::RaycastToPlane(double xpos,
                                                          double ypos,
                                                          float plane_y) const {
  auto near_point = ScreenToWorldPosition(xpos, ypos, 0.0F);
  auto far_point = ScreenToWorldPosition(xpos, ypos, 1.0F);

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
