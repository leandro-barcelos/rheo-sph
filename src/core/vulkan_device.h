#ifndef RHEOSPH_VULKAN_DEVICE_H
#define RHEOSPH_VULKAN_DEVICE_H

#include <optional>
#include <vulkan/vulkan_raii.hpp>

#include "vulkan_context.h"

namespace core {
struct QueueFamilyIndices {
  [[nodiscard]] bool IsComplete() const;
  [[nodiscard]] bool HasAsyncCompute() const;

  [[nodiscard]] std::optional<uint32_t> const& Graphics() const {
    return graphics_;
  }
  [[nodiscard]] std::optional<uint32_t> const& Compute() const {
    return compute_;
  }
  [[nodiscard]] std::optional<uint32_t> const& Present() const {
    return present_;
  }

  void SetGraphics(uint32_t family_index) { graphics_ = family_index; }
  void SetCompute(uint32_t family_index) { compute_ = family_index; }
  void SetPresent(uint32_t family_index) { present_ = family_index; }

 private:
  std::optional<uint32_t> graphics_;
  std::optional<uint32_t> compute_;
  std::optional<uint32_t> present_;
} __attribute__((aligned(32)));

class VulkanDevice {
 public:
  void Init(VulkanContext const& vulkan_context, vk::SurfaceKHR surface);

  [[nodiscard]] vk::raii::PhysicalDevice const& PhysicalDevice() const {
    return physical_device_;
  }

  [[nodiscard]] vk::raii::Device const& Device() const { return device_; }

  [[nodiscard]] uint32_t GraphicsQueueFamilyIndex() const {
    return queue_indices_.Graphics().value();
  }
  [[nodiscard]] uint32_t ComputeQueueFamilyIndex() const {
    return queue_indices_.Compute().value();
  }
  [[nodiscard]] uint32_t PresentQueueFamilyIndex() const {
    return queue_indices_.Present().value();
  }

  [[nodiscard]] vk::raii::Queue const& GraphicsQueue() const {
    return graphics_queue_;
  }
  [[nodiscard]] vk::raii::Queue const& ComputeQueue() const {
    return compute_queue_;
  }
  [[nodiscard]] vk::raii::Queue const& PresentQueue() const {
    return present_queue_;
  }

 private:
  vk::raii::PhysicalDevice physical_device_ = nullptr;
  vk::raii::Device device_ = nullptr;
  QueueFamilyIndices queue_indices_;
  vk::raii::Queue graphics_queue_ = nullptr;
  vk::raii::Queue compute_queue_ = nullptr;
  vk::raii::Queue present_queue_ = nullptr;

  std::vector<const char*> required_device_extensions_ = {
      vk::KHRSwapchainExtensionName};

  void PickPhysicalDevice(core::VulkanContext const& vulkan_context,
                          vk::SurfaceKHR surface);
  void CreateLogicalDevice(vk::SurfaceKHR surface);
  void FindQueues(vk::SurfaceKHR surface);
  void RetrieveQueues();

  [[nodiscard]] bool IsDeviceSuitable(
      vk::raii::PhysicalDevice const& physical_device,
      vk::SurfaceKHR surface) const;
};
}  // namespace core

#endif  // !RHEOSPH_VULKAN_DEVICE_H
