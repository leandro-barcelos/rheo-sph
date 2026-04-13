#ifndef RHEOSPH_VULKAN_CONTEXT_H
#define RHEOSPH_VULKAN_CONTEXT_H

#include <array>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

namespace core {

constexpr std::array kValidationLayers{"VK_LAYER_KHRONOS_validation"};

#ifdef NDEBUG
constexpr bool kEnableValidationLayers = false;
#else
constexpr bool kEnableValidationLayers = true;
#endif

class VulkanContext {
 public:
  void Init(std::vector<const char*> const& required_extensions);

  [[nodiscard]] vk::raii::Context const& Context() const { return context_; }
  [[nodiscard]] vk::raii::Instance const& Instance() const { return instance_; }

 private:
  vk::raii::Context context_;
  vk::raii::Instance instance_ = nullptr;

  void CreateInstance(std::vector<const char*> const& required_extensions);
};
}  // namespace core

#endif  // !RHEOSPH_VULKAN_CONTEXT_H
