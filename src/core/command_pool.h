#ifndef RHEOAPH_COMMAND_POOL_H
#define RHEOAPH_COMMAND_POOL_H

#include <vulkan/vulkan_raii.hpp>

#include "vulkan_device.h"

namespace core {

class CommandPools {
 public:
  void Init(core::VulkanDevice const& vulkan_device);

  [[nodiscard]] vk::raii::CommandPool const& Graphics() const {
    return graphics_command_pool_;
  }
  [[nodiscard]] vk::raii::CommandPool const& Compute() const {
    return compute_command_pool_;
  }

 private:
  vk::raii::CommandPool graphics_command_pool_ = nullptr;
  vk::raii::CommandPool compute_command_pool_ = nullptr;
};

}  // namespace core

#endif  // !RHEOAPH_COMMAND_POOL_H
