#pragma once

#include <cstdint>

namespace renderer {

struct UiTextureHandle {
  uint32_t id = 0;  // NOLINT(misc-non-private-member-variables-in-classes)
  [[nodiscard]] bool IsValid() const { return id != 0; }
  bool operator==(UiTextureHandle const&) const = default;
};

inline constexpr UiTextureHandle kNullUiTexture{};

}  // namespace renderer
