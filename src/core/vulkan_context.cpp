#include "vulkan_context.h"

#include <vector>

void core::VulkanContext::Init(
    std::vector<const char*> const& required_extensions) {
  CreateInstance(required_extensions);
}

void core::VulkanContext::CreateInstance(
    std::vector<const char*> const& required_extensions) {
  constexpr vk::ApplicationInfo kAppInfo{
      .pApplicationName = "Rheo SPH",
      .applicationVersion = VK_MAKE_VERSION(0, 0, 0),
      .pEngineName = "No Engine",
      .engineVersion = VK_MAKE_VERSION(0, 0, 0),
      .apiVersion = vk::ApiVersion14};

  auto extension_properties = context_.enumerateInstanceExtensionProperties();
  for (const auto* extension : required_extensions) {
    if (std::ranges::none_of(
            extension_properties, [extension](auto const& extension_property) {
              return strcmp(extension_property.extensionName, extension) == 0;
            })) {
      throw std::runtime_error(
          "[ERROR] Vulkan: required window extension not supported: " +
          std::string(extension));
    }
  }

  std::vector<const char*> layers{};
  if (kEnableValidationLayers) {
    layers.assign(kValidationLayers.begin(), kValidationLayers.end());
  }

  auto layer_properties = context_.enumerateInstanceLayerProperties();
  for (const auto* layer : layers) {
    if (std::ranges::none_of(
            layer_properties, [layer](auto const& layer_property) {
              return strcmp(layer_property.layerName, layer) == 0;
            })) {
      throw std::runtime_error(
          "[ERROR] Vulkan: required layer not supported: " +
          std::string(layer));
    }
  }

  const vk::InstanceCreateInfo create_info{
      .pApplicationInfo = &kAppInfo,
      .enabledLayerCount = static_cast<uint32_t>(layers.size()),
      .ppEnabledLayerNames = layers.data(),
      .enabledExtensionCount =
          static_cast<uint32_t>(required_extensions.size()),
      .ppEnabledExtensionNames = required_extensions.data(),
  };

  instance_ = vk::raii::Instance(context_, create_info);
}
