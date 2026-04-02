#include <cstdlib>
#include <iostream>

#include "renderer.h"
#include "window.h"

int main() {
  constexpr window::Properties kWindowProperties{
      .width = 800, .height = 600, .title = "Rheo SPH"};

  const window::Window window{kWindowProperties};
  try {
    render::Renderer vulkan;
    vulkan.Init();
  } catch (std::runtime_error &error) {
    std::cerr << error.what() << '\n';
    return EXIT_FAILURE;
  }

  while (!window.ShouldClose()) {  // NOLINT(*-id-dependent-backward-branch)
    window::Window::PollEvents();
  }

  return EXIT_SUCCESS;
}
