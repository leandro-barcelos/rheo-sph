#ifndef RHEOSPH_RENDERER_H
#define RHEOSPH_RENDERER_H

#include <vulkan/vulkan_raii.hpp>

namespace render {

const std::array kValidationLayers{"VK_LAYER_KHRONOS_validation"};

#ifdef NDEBUG
constexpr bool kEnableValidationLayers = false;
#else
constexpr bool kEnableValidationLayers = true;
#endif

class Renderer {
 public:
  void Init();

 private:
  vk::raii::Context context_;
  vk::raii::Instance instance_ = nullptr;
  vk::raii::PhysicalDevice physical_device_ = nullptr;
  vk::raii::Device device_ = nullptr;
  vk::raii::Queue graphics_queue_ = nullptr;
  vk::raii::Queue compute_queue_ = nullptr;

  void CreateInstance();
  void PickPhysicalDevice();
  void CreateLogicalDevice();
};
}  // namespace render

#endif  // !RHEOSPH_RENDERER_H
