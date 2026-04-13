#ifndef RHEOSPH_VULKAN_SWAP_CHAIN_H
#define RHEOSPH_VULKAN_SWAP_CHAIN_H

#include <limits>
#include <vulkan/vulkan_raii.hpp>

#include "frame_sync.h"
#include "vulkan/vulkan.hpp"
#include "window.h"

namespace core {
class VulkanSwapChain {
 public:
  static constexpr uint32_t kInvalidImageIndex =
      std::numeric_limits<uint32_t>::max();

  void Init(Window const& window, VulkanDevice const& vulkan_device);
  [[nodiscard]] uint32_t AcquireNextImage(
      core::VulkanDevice const& vulkan_device,
      core::FrameSync const& frame_sync) const;
  void RecreateSwapChain(Window const& window,
                         VulkanDevice const& vulkan_device);
  [[nodiscard]] vk::raii::SwapchainKHR const& SwapChain() const {
    return swap_chain_;
  }
  [[nodiscard]] vk::SurfaceFormatKHR const& SurfaceFormat() const {
    return surface_format_;
  }
  [[nodiscard]] vk::Image const& GetImage(uint32_t index) const {
    return images_.at(index);
  }
  [[nodiscard]] vk::ImageLayout const& GetImageLayout(uint32_t index) const {
    return image_layouts_.at(index);
  }
  void SetImageLayout(uint32_t index, vk::ImageLayout image_layout) {
    image_layouts_.at(index) = image_layout;
  }
  [[nodiscard]] vk::raii::ImageView const& GetImageView(uint32_t index) const {
    return image_views_.at(index);
  }
  [[nodiscard]] vk::Extent2D const& Extent() const { return extent_; }

 private:
  vk::raii::SwapchainKHR swap_chain_ = nullptr;
  std::vector<vk::Image> images_;
  std::vector<vk::ImageLayout> image_layouts_;
  vk::Extent2D extent_;
  vk::SurfaceFormatKHR surface_format_;
  std::vector<vk::raii::ImageView> image_views_;

  void CreateSwapChain(Window const& window, VulkanDevice const& vulkan_device);
  void CreateImageViews(VulkanDevice const& vulkan_device);
  static vk::SurfaceFormatKHR ChooseSurfaceFormat(
      std::vector<vk::SurfaceFormatKHR> const& available_formats);
  static vk::Extent2D ChooseExtent(
      core::WindowSize window_size,
      vk::SurfaceCapabilitiesKHR const& surface_capabilities);
  static uint32_t ChooseMinImageCount(
      vk::SurfaceCapabilitiesKHR const& surface_capabilities);
  static vk::PresentModeKHR ChoosePresentMode(
      std::vector<vk::PresentModeKHR> const& available_present_modes);
  void CleanupSwapChain();
};
}  // namespace core

#endif  // !RHEOSPH_VULKAN_SWAP_CHAIN_H
