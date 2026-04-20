#pragma once

#include <unordered_map>

#include <vulkan/vulkan_raii.hpp>

#include "ui_texture_handle.h"

namespace renderer {

class UiTextureRegistry {
 public:
  UiTextureHandle Add(vk::Sampler, vk::ImageView, vk::ImageLayout);
  void Remove(UiTextureHandle handle);
  // Called by ImGuiLayer::Render — never exposed outside renderer/
  [[nodiscard]] void* ResolveImGuiId(UiTextureHandle handle) const;
  void Clear();  // called from ImGuiLayer::Shutdown

 private:
  std::unordered_map<uint32_t, VkDescriptorSet> entries_;
  uint32_t next_id_ = 1;
};

}  // namespace renderer
