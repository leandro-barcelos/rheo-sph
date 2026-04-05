#include <cstdlib>
#include <iostream>

#include "renderer.h"
#include "window.h"

int main() {
  constexpr window::Properties kWindowProperties{
      .width = 800, .height = 600, .title = "Rheo SPH"};

  constexpr render::Parameters kParameters{
      .voxel_max_particles = 10,
      .fluid_particle_count = 500,
      .wall_particle_count = 500,
      .total_particle_count = 1000,
      .bucket_size = {50, 50, 50},
      .min_bound = {-1, -1, -1},
      .max_bound = {1, 1, 1},
  };

  const window::Window window{kWindowProperties};
  try {
    render::Renderer vulkan;
    vulkan.Init(kParameters);

    while (!window.ShouldClose()) {  // NOLINT(*-id-dependent-backward-branch)
      vulkan.Update();
      window::Window::PollEvents();
    }
  } catch (std::runtime_error& error) {
    std::cerr << error.what() << '\n';
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
