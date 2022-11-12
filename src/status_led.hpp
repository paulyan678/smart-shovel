#pragma once

#include <Arduino.h>
#include <cstdint>

namespace smart_shovel {
namespace firmware {

enum class LedMode : uint8_t {
  booting,
  calibrating,
  ready,
  waiting_for_gps,
  gps_degraded,
  storage_degraded,
  sensor_fault,
};

class StatusLed {
 public:
  void begin() noexcept;
  void set_mode(LedMode mode) noexcept;
  void update(uint32_t now_ms) noexcept;

 private:
  void write(bool on) noexcept;

  LedMode mode_{LedMode::booting};
  LedMode rendered_mode_{LedMode::sensor_fault};
  bool last_on_{false};
};

}  // namespace firmware
}  // namespace smart_shovel
