#include "ui_texture_registry.h"

#include "backends/imgui_impl_vulkan.h"

namespace renderer {

UiTextureHandle UiTextureRegistry::Add(vk::Sampler sampler,
                                      vk::ImageView image_view,
                                      vk::ImageLayout image_layout) {
  const uint32_t index = next_id_++;

  VkDescriptorSet descriptor_set = ImGui_ImplVulkan_AddTexture(
      static_cast<VkSampler>(sampler), static_cast<VkImageView>(image_view),
      static_cast<VkImageLayout>(image_layout));

  if (descriptor_set == VK_NULL_HANDLE) {
    return kNullUiTexture;
  }

  entries_.emplace(index, descriptor_set);
  return UiTextureHandle{.id = index};
}

void UiTextureRegistry::Remove(UiTextureHandle handle) {
  if (!handle.IsValid()) {
    return;
  }

  auto iter = entries_.find(handle.id);
  if (iter == entries_.end()) {
    return;
  }

  ImGui_ImplVulkan_RemoveTexture(iter->second);
  entries_.erase(iter);
}

void* UiTextureRegistry::ResolveImGuiId(UiTextureHandle handle) const {
  if (!handle.IsValid()) {
    return nullptr;
  }

  auto iter = entries_.find(handle.id);
  if (iter == entries_.end()) {
    return nullptr;
  }

  return reinterpret_cast<void*>(iter->second);
}

void UiTextureRegistry::Clear() {
  for (auto const& [index, descriptor_set] : entries_) {
    (void)index;
    ImGui_ImplVulkan_RemoveTexture(descriptor_set);
  }

  entries_.clear();
  next_id_ = 1;
}

}  // namespace renderer
