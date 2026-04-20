#pragma once

#include <cstdint>
#include <memory>
#include <optional>

#include "../core/command_pool.h"
#include "../core/frame_sync.h"
#include "../core/vulkan_device.h"
#include "../simulation/fluid_simulator.h"

namespace app {

class SimulationSession {
 public:
  void ApplyParameters(simulation::FluidSimulator::Parameters const& params,
                       core::VulkanDevice const& vulkan_device,
                       core::CommandPools const& command_pools);
  std::optional<uint64_t> Tick(core::VulkanDevice const& vulkan_device,
                               core::FrameSync& frame_sync,
                               double delta_ms);
    void Play();
  void Pause();
  void Reset(core::VulkanDevice const& vulkan_device,
             core::CommandPools const& command_pools);

  [[nodiscard]] bool IsRunning() const { return running_; }
  [[nodiscard]] bool HasSimulator() const { return fluid_simulator_ != nullptr; }
  [[nodiscard]] simulation::FluidSimulator const* Simulator() const;

 private:
  std::unique_ptr<simulation::FluidSimulator> fluid_simulator_;
  std::optional<simulation::FluidSimulator::Parameters> parameters_;
  bool running_ = false;
  bool parameters_dirty_ = false;

    core::VulkanDevice const* vulkan_device_ = nullptr;
    core::CommandPools const* command_pools_ = nullptr;

  void Recreate(core::VulkanDevice const& vulkan_device,
                core::CommandPools const& command_pools);
};

}  // namespace app
