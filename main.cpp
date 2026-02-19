#include <cstdlib>
#include <vulkan/vulkan_raii.hpp>

#include "renderer.h"
#include "window.h"

int main() {
  constexpr window::Properties kWindowProperties{
      .width = 800, .height = 600, .title = "Rheo SPH"};

  window::Window window{kWindowProperties};
  render::Renderer vulkan;
  vulkan.Init();

  while (!window.ShouldClose()) {
    window::Window::PollEvents();
  }

  return EXIT_SUCCESS;
}
