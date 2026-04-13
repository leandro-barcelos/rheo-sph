#include "window.h"

#include <GLFW/glfw3.h>
#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <array>
#include <format>
#include <iostream>
#include <stdexcept>
#include <vulkan/vulkan_raii.hpp>

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

void core::Window::ErrorCallback(int error, const char* description) {
  std::cerr << std::format(
                   "[ERROR] GLFW: [{}] {}", error,
                   description != nullptr ? description : "<no description>")
            << '\n';
}
