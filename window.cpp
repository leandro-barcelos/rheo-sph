#include "window.h"

#include <GLFW/glfw3.h>
#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <iostream>
#include <stdexcept>

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
    void* handle = dlopen(lib_name, RTLD_NOW | RTLD_NOLOAD);
    if (handle != nullptr) {
      dlclose(handle);
      return true;
    }
    return false;
  });
#endif
  return false;
}
}  // namespace

window::Window::Window(const Properties properties) {
  glfwSetErrorCallback(window::Window::ErrorCallback);

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

window::Window::~Window() {
  glfwDestroyWindow(window_);
  glfwTerminate();
}

void window::Window::ErrorCallback(int error, const char* description) {
  std::cerr << std::format(
                   "[ERROR] GLFW: [{}] {}", error,
                   description != nullptr ? description : "<no description>")
            << '\n';
}

bool window::Window::ShouldClose() const {
  return glfwWindowShouldClose(window_) != 0;
}

void window::Window::PollEvents() { glfwPollEvents(); }

std::vector<const char*> window::Window::GetRequiredExtensions() {
  uint32_t extension_count = 0;
  auto* extensions = glfwGetRequiredInstanceExtensions(&extension_count);

  std::vector<const char*> extensions_vec(
      extensions,
      extensions + extension_count);  // NOLINT

  return extensions_vec;
}

window::Size window::Window::GetSize() const {
  int width = 0;
  int height = 0;
  glfwGetFramebufferSize(window_, &width, &height);

  return {.width = width, .height = height};
}

void window::Window::WaitEvents() { glfwWaitEvents(); }

void window::Window::CreateSurface(VkInstance instance,
                                   VkSurfaceKHR& surface) const {
  const VkResult result =
      glfwCreateWindowSurface(instance, window_, nullptr, &surface);
  if (result != VK_SUCCESS) {
    const char* description = nullptr;
    const int error_code = glfwGetError(&description);
    throw std::runtime_error(std::format(
        "[ERROR] GLFW: failed to create window surface (VkResult={} GLFW={}): "
        "{}",
        static_cast<int>(result), error_code,
        description != nullptr ? description : "<no details>"));
  }
}
