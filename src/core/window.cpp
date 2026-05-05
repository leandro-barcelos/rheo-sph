#include "window.h"

#include <GLFW/glfw3.h>
#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <array>
#include <format>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <vulkan/vulkan_raii.hpp>

#include "rheo-sph/src/core/input_events.h"
#include "vulkan_device.h"

#if defined(__linux__)
#include <dlfcn.h>
#endif

namespace {
[[nodiscard]] bool IsRenderDocInjected() {
#if defined(__linux__) && defined(RTLD_NOLOAD)
  // Detect RenderDoc injection without reading environment variables.
  // RTLD_NOLOAD returns a handle only if already loaded.
  constexpr std::array<const char*, 2> kRenderDocLibNames = {
      "librenderdoc.so", "librenderdoc.so.1"};
  return std::ranges::any_of(kRenderDocLibNames, [](const char* lib_name) {
    if (void* handle = dlopen(lib_name, RTLD_NOW | RTLD_NOLOAD);
        handle != nullptr) {
      dlclose(handle);
      return true;
    }
    return false;
  });
#endif
  return false;
}
}  // namespace

core::Window::Window(const WindowProperties properties) {
  glfwSetErrorCallback(ErrorCallback);

#if defined(GLFW_PLATFORM) && defined(GLFW_PLATFORM_X11)
  if (IsRenderDocInjected()) {
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
  }
#endif

  if (glfwInit() == 0) {
    const char* description = nullptr;
    const int error_code = glfwGetError(&description);
    throw std::runtime_error(
        std::format("[ERROR] GLFW: failed to initialize ({}): {}", error_code,
                    description != nullptr ? description : "<no details>"));
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  window_ = glfwCreateWindow(properties.width, properties.height,
                             properties.title, nullptr, nullptr);
  if (window_ == nullptr) {
    throw std::runtime_error("[ERROR] GLFW: failed to create window");
  }

  glfwSetWindowUserPointer(window_, static_cast<void*>(&input_events_));
  glfwSetKeyCallback(window_, KeyCallback);
  glfwSetCursorPosCallback(window_, CursorPosCallback);
  glfwSetMouseButtonCallback(window_, MouseButtonCallback);
  glfwSetScrollCallback(window_, ScrollCallback);
}

core::Window::~Window() {
  surface_ = nullptr;
  glfwDestroyWindow(window_);
  glfwTerminate();
}

void core::Window::CreateSurface(VulkanContext const& vulkan_context) {
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  const VkResult result = glfwCreateWindowSurface(*vulkan_context.Instance(),
                                                  window_, nullptr, &surface);
  if (result != VK_SUCCESS) {
    const char* description = nullptr;
    const int error_code = glfwGetError(&description);
    throw std::runtime_error(std::format(
        "[ERROR] GLFW: failed to create window surface (VkResult={} GLFW={}): "
        "{}",
        static_cast<int>(result), error_code,
        description != nullptr ? description : "<no details>"));
  }
  surface_ = vk::raii::SurfaceKHR(vulkan_context.Instance(), surface);
}

bool core::Window::ShouldClose() const {
  return glfwWindowShouldClose(window_) != 0;
}

void core::Window::PollEvents() { glfwPollEvents(); }

std::vector<const char*> core::Window::GetRequiredExtensions() {
  uint32_t extension_count = 0;
  auto* extensions = glfwGetRequiredInstanceExtensions(&extension_count);

  std::vector<const char*> extensions_vec(
      extensions,
      extensions + extension_count);  // NOLINT

  return extensions_vec;
}

core::WindowSize core::Window::Size() const {
  int width = 0;
  int height = 0;
  glfwGetFramebufferSize(window_, &width, &height);

  return {.width = width, .height = height};
}

void core::Window::WaitEvents() { glfwWaitEvents(); }

core::InputState core::Window::DrainInputEvents() {
  InputState drained;
  drained.modifiers = input_events_.modifiers;
  drained.mouse_drag_event = input_events_.mouse_drag_event;
  drained.scroll_event = input_events_.scroll_event;
  drained.pressed_keys = input_events_.pressed_keys;

  input_events_.scroll_event = std::nullopt;
  input_events_.pressed_keys.clear();

  return drained;
}

vk::SurfaceCapabilitiesKHR core::Window::Capabilities(
    VulkanDevice const& vulkan_device) const {
  return vulkan_device.PhysicalDevice().getSurfaceCapabilitiesKHR(*surface_);
}

std::vector<vk::SurfaceFormatKHR> core::Window::Formats(
    VulkanDevice const& vulkan_device) const {
  return vulkan_device.PhysicalDevice().getSurfaceFormatsKHR(*surface_);
}

std::vector<vk::PresentModeKHR> core::Window::PresentModes(
    VulkanDevice const& vulkan_device) const {
  return vulkan_device.PhysicalDevice().getSurfacePresentModesKHR(*surface_);
}

void core::Window::SetEventCallbacks() {
  glfwSetKeyCallback(window_, KeyCallback);
}

namespace {
void HandleModifierKey(core::InputState* input_events, int key, int action) {
  switch (key) {
    case GLFW_KEY_LEFT_SHIFT:
    case GLFW_KEY_RIGHT_SHIFT:
      if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        input_events->modifiers.shift = true;
      } else if (action == GLFW_RELEASE) {
        input_events->modifiers.shift = false;
      }
      break;
    case GLFW_KEY_LEFT_CONTROL:
    case GLFW_KEY_RIGHT_CONTROL:
      if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        input_events->modifiers.control = true;
      } else if (action == GLFW_RELEASE) {
        input_events->modifiers.control = false;
      }
      break;
    default:
      break;
  }
}

void HandleActionKey(GLFWwindow* window, core::InputState* input_events,
                     int key, int action, int mods) {
  switch (key) {
    case GLFW_KEY_F1:
      if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        input_events->pressed_keys.push_back(core::Key::kF1);
      }
      break;
    case GLFW_KEY_F4:
      if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        if ((mods & GLFW_MOD_ALT) != 0) {
          glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
      }
      break;
    case GLFW_KEY_O:
      if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        input_events->pressed_keys.push_back(core::Key::kO);
      }
      break;
    case GLFW_KEY_S:
      if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        input_events->pressed_keys.push_back(core::Key::kS);
      }
      break;
    default:
      break;
  }
}
}  // namespace

void core::Window::KeyCallback(GLFWwindow* window, int key, int /*scancode*/,
                               int action, int mods) {
  InputState* input_events = nullptr;
  input_events = static_cast<InputState*>(glfwGetWindowUserPointer(window));
  HandleModifierKey(input_events, key, action);
  HandleActionKey(window, input_events, key, action, mods);
}

void core::Window::CursorPosCallback(GLFWwindow* window, double xpos,
                                     double ypos) {
  InputState* input_events = nullptr;
  input_events = static_cast<InputState*>(glfwGetWindowUserPointer(window));

  auto mouse_drag_opt = input_events->mouse_drag_event;
  if (!mouse_drag_opt.has_value()) {
    return;
  }

  mouse_drag_opt->current_position = CursorPosition{.x = xpos, .y = ypos};
  input_events->mouse_drag_event = mouse_drag_opt;
}

void core::Window::MouseButtonCallback(GLFWwindow* window,
                                       int button,  // NOLINT
                                       int action, int /*mods*/) {
  InputState* input_events = nullptr;
  input_events = static_cast<InputState*>(glfwGetWindowUserPointer(window));
  if (button == GLFW_MOUSE_BUTTON_LEFT) {
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
      // Operate on a local copy of the optional to avoid dereferencing a
      // disengaged optional if it changes concurrently between checks.
      auto mouse_drag_opt = input_events->mouse_drag_event;
      if (mouse_drag_opt.has_value()) {
        if (mouse_drag_opt->current_position.has_value()) {
          mouse_drag_opt->anchor_position = *mouse_drag_opt->current_position;
          mouse_drag_opt->current_position = std::nullopt;
        }
        input_events->mouse_drag_event = mouse_drag_opt;
        return;
      }

      double xpos = 0;
      double ypos = 0;
      glfwGetCursorPos(window, &xpos, &ypos);
      input_events->mouse_drag_event = core::MouseDragEvent{
          .anchor_position = CursorPosition{.x = xpos, .y = ypos},
          .current_position = std::nullopt};
    } else if (action == GLFW_RELEASE) {
      input_events->mouse_drag_event = std::nullopt;
    }
  }
}

void core::Window::ScrollCallback(GLFWwindow* window, double xoffset,
                                  double yoffset) {
  InputState* input_events = nullptr;
  input_events = static_cast<InputState*>(glfwGetWindowUserPointer(window));

  double xpos = 0;
  double ypos = 0;
  glfwGetCursorPos(window, &xpos, &ypos);
  input_events->scroll_event = ScrollEvent{
      .cursor_position = CursorPosition{.x = xpos, .y = ypos},
      .delta_x = xoffset,
      .delta_y = yoffset,
  };
}

void core::Window::ErrorCallback(int error, const char* description) {
  std::cerr << std::format(
                   "[ERROR] GLFW: [{}] {}", error,
                   description != nullptr ? description : "<no description>")
            << '\n';
}
