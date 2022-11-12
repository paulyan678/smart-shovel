#pragma once

#include <Arduino.h>
#include <TinyGPSPlus.h>
#include <cstdint>

#include "smart_shovel/gps.hpp"

namespace smart_shovel {
namespace firmware {

struct GpsSnapshot {
  GpsFix fix{};
  bool satellites_valid{false};
  uint32_t satellites{0U};
  bool stream_seen{false};
  uint32_t last_byte_ms{0U};
};

class GpsReceiver {
 public:
  void begin(uint32_t baud) noexcept;
  void poll(uint32_t now_ms) noexcept;
  [[nodiscard]] GpsSnapshot snapshot(uint32_t now_ms) noexcept;

 private:
  TinyGPSPlus parser_{};
  bool stream_seen_{false};
  uint32_t last_byte_ms_{0U};
};

}  // namespace firmware
}  // namespace smart_shovel
