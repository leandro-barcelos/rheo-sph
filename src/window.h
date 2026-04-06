#ifndef RHEOSPH_WINDOW_H
#define RHEOSPH_WINDOW_H

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>

namespace window {

struct Size {
  int width;
  int height;
} __attribute__((aligned(8)));

struct Properties {
  int width;
  int height;
  const char* title;
} __attribute__((aligned(16)));

class Window {
 private:
  GLFWwindow* window_;

  static void ErrorCallback(int error, const char* description);

 public:
  Window(const Window&) = default;
  Window(Window&&) = delete;
  Window& operator=(const Window&) = default;
  Window& operator=(Window&&) = delete;
  explicit Window(Properties properties);
  ~Window();

  [[nodiscard]] bool ShouldClose() const;
  static void PollEvents();
  static std::vector<const char*> GetRequiredExtensions();
  [[nodiscard]] Size GetSize() const;
  static void WaitEvents();
  void CreateSurface(VkInstance instance, VkSurfaceKHR& surface) const;
};
}  // namespace window

#endif  // !RHEOSPH_WINDOW_H
