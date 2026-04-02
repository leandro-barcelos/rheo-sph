#ifndef RHEOSPH_RENDERER_H
#define RHEOSPH_RENDERER_H

#include <unordered_set>
#include <vulkan/vulkan_raii.hpp>

namespace render {

constexpr std::array kValidationLayers{"VK_LAYER_KHRONOS_validation"};

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
  uint32_t graphics_queue_index_ = ~0;
  vk::raii::Queue compute_queue_ = nullptr;
  uint32_t compute_queue_index_ = ~0;
  vk::raii::Queue transfer_queue_ = nullptr;
  uint32_t transfer_queue_index_ = ~0;

  void CreateInstance();
  void PickPhysicalDevice();
  void CreateLogicalDevice();

  [[nodiscard]] uint32_t FindQueue(
      vk::QueueFlags flags,
      const std::unordered_set<uint32_t>& exclude = {}) const;
};
}  // namespace render

#endif  // !RHEOSPH_RENDERER_H
