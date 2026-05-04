#ifndef RHEOSPH_WINDOW_H
#define RHEOSPH_WINDOW_H

#include "input_events.h"
#include "vulkan/vulkan.hpp"
#include "vulkan_context.h"
#include "vulkan_device.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>
#include <vulkan/vulkan_raii.hpp>

namespace core {

struct WindowSize {
  int width;
  int height;
} __attribute__((aligned(8)));

struct WindowProperties {
  int width;
  int height;
  const char* title;
} __attribute__((aligned(16)));

class Window {
 public:
  Window(const Window&) = delete;
  Window(Window&&) = delete;
  Window& operator=(const Window&) = delete;
  Window& operator=(Window&&) = delete;
  explicit Window(WindowProperties properties);
  ~Window();

  void CreateSurface(core::VulkanContext const& vulkan_context);
  [[nodiscard]] bool ShouldClose() const;
  static void PollEvents();
  [[nodiscard]] static std::vector<const char*> GetRequiredExtensions();
  [[nodiscard]] WindowSize Size() const;
  static void WaitEvents();
  [[nodiscard]] InputState DrainInputEvents();

  [[nodiscard]] vk::SurfaceCapabilitiesKHR Capabilities(
      VulkanDevice const& vulkan_device) const;
  [[nodiscard]] std::vector<vk::SurfaceFormatKHR> Formats(
      VulkanDevice const& vulkan_device) const;
  [[nodiscard]] std::vector<vk::PresentModeKHR> PresentModes(
      VulkanDevice const& vulkan_device) const;
  [[nodiscard]] vk::raii::SurfaceKHR const& Surface() const { return surface_; }
  [[nodiscard]] GLFWwindow* Handle() const { return window_; }

 private:
  GLFWwindow* window_;
  vk::raii::SurfaceKHR surface_ = nullptr;
  InputState input_events_;

  void SetEventCallbacks();
  static void KeyCallback(GLFWwindow* window, int key, int scancode, int action,
                          int mods);
  static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);
  static void MouseButtonCallback(GLFWwindow* window, int button, int action,
                                  int mods);
  static void ScrollCallback(GLFWwindow* window, double xoffset,
                             double yoffset);
  static void ErrorCallback(int error, const char* description);
};
}  // namespace core

#endif  // !RHEOSPH_WINDOW_H
