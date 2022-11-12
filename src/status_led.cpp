#include "status_led.hpp"

#include "smart_shovel/config.hpp"

namespace smart_shovel {
namespace firmware {
namespace {

struct Pattern {
  uint16_t on_ms;
  uint16_t off_ms;
  uint16_t pause_ms;
  uint8_t pulse_count;
  bool solid;
};

Pattern pattern_for(LedMode mode) noexcept {
  switch (mode) {
    case LedMode::calibrating:
      return Pattern{500U, 500U, 0U, 1U, false};
    case LedMode::ready:
      return Pattern{0U, 0U, 0U, 0U, true};
    case LedMode::waiting_for_gps:
      return Pattern{125U, 125U, 0U, 1U, false};
    case LedMode::gps_degraded:
      return Pattern{150U, 150U, 1200U, 1U, false};
    case LedMode::storage_degraded:
      return Pattern{150U, 150U, 600U, 3U, false};
    case LedMode::sensor_fault:
      return Pattern{100U, 100U, 500U, 5U, false};
    case LedMode::booting:
    default:
      return Pattern{100U, 100U, 600U, 2U, false};
  }
}

bool pattern_is_on(const Pattern& pattern, uint32_t now_ms) noexcept {
  if (pattern.solid) {
    return true;
  }
  const uint32_t pulse_period = pattern.on_ms + pattern.off_ms;
  const uint32_t active_period = pulse_period * pattern.pulse_count;
  const uint32_t cycle_period = active_period + pattern.pause_ms;
  if (pulse_period == 0U || cycle_period == 0U) {
    return false;
  }
  const uint32_t phase = now_ms % cycle_period;
  return phase < active_period && (phase % pulse_period) < pattern.on_ms;
}

}  // namespace

void StatusLed::begin() noexcept {
  pinMode(firmware_config::kStatusLedPin, OUTPUT);
  write(false);
  rendered_mode_ = LedMode::sensor_fault;
}

void StatusLed::set_mode(LedMode mode) noexcept {
  mode_ = mode;
}

void StatusLed::update(uint32_t now_ms) noexcept {
  const bool on = pattern_is_on(pattern_for(mode_), now_ms);
  if (mode_ != rendered_mode_ || on != last_on_) {
    write(on);
    rendered_mode_ = mode_;
    last_on_ = on;
  }
}

void StatusLed::write(bool on) noexcept {
  const bool level_high = on == firmware_config::kStatusLedActiveHigh;
  digitalWrite(firmware_config::kStatusLedPin, level_high ? HIGH : LOW);
}

}  // namespace firmware
}  // namespace smart_shovel
