#include "window.h"

#include <GLFW/glfw3.h>

#include <cstdint>
#include <iostream>
#include <stdexcept>

window::Window::Window(window::Properties properties) {
  if (glfwInit() == 0) {
    throw std::runtime_error("GLFW: failed to intialize");
  }

  glfwSetErrorCallback(window::Window::ErrorCallback);

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  window_ = glfwCreateWindow(properties.width, properties.height,
                             properties.title, nullptr, nullptr);
  if (window_ == nullptr) {
    throw std::runtime_error("GLFW: failed to create window");
  }
}

window::Window::~Window() {
  glfwDestroyWindow(window_);
  glfwTerminate();
}

void window::Window::ErrorCallback(int error, const char* description) {
  std::cerr << std::format("GLFW: [%d] %s\n", error, description) << '\n';
}

bool window::Window::ShouldClose() {
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
