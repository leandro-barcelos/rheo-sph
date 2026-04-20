#ifndef RHEOSPH_IMGUI_LAYER_H
#define RHEOSPH_IMGUI_LAYER_H

#include <cstdint>
#include <vulkan/vulkan_raii.hpp>

#include "../core/vulkan_context.h"
#include "../core/vulkan_device.h"
#include "../core/vulkan_swap_chain.h"
#include "../core/window.h"
#include "../renderer/ui_texture_registry.h"

namespace ui {

class ImGuiLayer {
 public:
  ImGuiLayer() = default;
  ImGuiLayer(const ImGuiLayer&) = delete;
  ImGuiLayer(ImGuiLayer&&) = delete;
  ImGuiLayer& operator=(const ImGuiLayer&) = delete;
  ImGuiLayer& operator=(ImGuiLayer&&) = delete;
  ~ImGuiLayer();

  void Init(core::Window const& window, core::VulkanContext const& context,
            core::VulkanDevice const& vulkan_device,
            core::VulkanSwapChain const& vulkan_swap_chain);
  void BeginFrame() const;
  void EndFrame() const;
  void Render(vk::raii::CommandBuffer const& command_buffer) const;

  [[nodiscard]] renderer::UiTextureHandle AddTexture(vk::Sampler sampler,
                                                     vk::ImageView image_view,
                                                     vk::ImageLayout image_layout);
  void RemoveTexture(renderer::UiTextureHandle handle);

  // Used by Renderer/UI controller to cache the ImGui-facing texture id.
  // Returns nullptr if the handle is invalid or not registered.
  [[nodiscard]] void* ResolveImGuiTextureId(renderer::UiTextureHandle handle) const;

  void OnSwapChainRecreated(core::VulkanSwapChain const& vulkan_swap_chain);
  void Shutdown();

 private:
  static void CheckVkResult(VkResult result);
  static void SetupImGuiStyle();

  vk::raii::DescriptorPool descriptor_pool_ = nullptr;
  renderer::UiTextureRegistry registry_;
  uint32_t min_image_count_ = 0;
  bool initialized_ = false;
};

}  // namespace ui

#endif  // !RHEOSPH_IMGUI_LAYER_H
