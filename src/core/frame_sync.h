#ifndef RHEOSPH_FRAME_SYNC_H
#define RHEOSPH_FRAME_SYNC_H

#include <cstdint>
#include <vulkan/vulkan_raii.hpp>

#include "vulkan_device.h"

namespace core {

class FrameSync {
 public:
  void Init(core::VulkanDevice const& vulkan_device,
            uint32_t max_frames_in_flight = 1);
  void WaitForFence(core::VulkanDevice const& vulkan_device) const;
  void WaitSemaphore(core::VulkanDevice const& vulkan_device,
                     uint64_t wait_value) const;

  [[nodiscard]] vk::raii::Semaphore const& Semaphore() const {
    return semaphore_;
  }
  [[nodiscard]] vk::raii::Fence const& Fence() const {
    return in_flight_fences_.at(frame_index_);
  }
  [[nodiscard]] uint64_t CurrentTimelineValue() const { return timeline_value_; }
  [[nodiscard]] uint64_t GetNextTimelineValue() { return ++timeline_value_; }

 private:
  uint32_t max_frames_in_flight_ = 1;
  vk::raii::Semaphore semaphore_ = nullptr;
  std::vector<vk::raii::Fence> in_flight_fences_;
  uint64_t timeline_value_ = 0;
  uint32_t frame_index_ = 0;
};

}  // namespace core

#endif  // !RHEOSPH_FRAME_SYNC_H
