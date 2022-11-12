#pragma once

#include <Arduino.h>
#include <Arduino_LSM6DSOX.h>
#include <cstdint>

namespace smart_shovel {
namespace firmware {

struct SensorSample {
  uint32_t raw_adc{0U};
  float acceleration_x_g{0.0F};
  float acceleration_y_g{0.0F};
  float acceleration_z_g{0.0F};
  float gyroscope_x_dps{0.0F};
  float gyroscope_y_dps{0.0F};
  float gyroscope_z_dps{0.0F};
  bool acceleration_valid{false};
  bool gyroscope_valid{false};
  bool motion_stable{false};
};

class SensorReader {
 public:
  void begin(uint8_t adc_bits) noexcept;
  bool begin_imu() noexcept;
  void mark_imu_unavailable() noexcept;
  SensorSample read(uint8_t load_cell_pin, uint16_t adc_sample_count,
                    double acceleration_magnitude_tolerance_g,
                    double gyroscope_stable_limit_dps) noexcept;

  [[nodiscard]] bool imu_ready() const noexcept {
    return imu_ready_;
  }

 private:
  bool imu_ready_{false};
};

}  // namespace firmware
}  // namespace smart_shovel
