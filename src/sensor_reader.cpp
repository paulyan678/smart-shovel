#include "sensor_reader.hpp"

#include <cmath>

namespace smart_shovel {
namespace firmware {

void SensorReader::begin(uint8_t adc_bits) noexcept {
  analogReadResolution(adc_bits);
}

bool SensorReader::begin_imu() noexcept {
  if (imu_ready_) {
    return true;
  }
  imu_ready_ = IMU.begin() != 0;
  return imu_ready_;
}

void SensorReader::mark_imu_unavailable() noexcept {
  if (imu_ready_) {
    IMU.end();
  }
  imu_ready_ = false;
}

SensorSample SensorReader::read(uint8_t load_cell_pin, uint16_t adc_sample_count,
                                double acceleration_magnitude_tolerance_g,
                                double gyroscope_stable_limit_dps) noexcept {
  SensorSample sample{};
  const uint16_t count = adc_sample_count == 0U ? 1U : adc_sample_count;
  uint64_t adc_total = 0U;
  for (uint16_t index = 0U; index < count; ++index) {
    adc_total += static_cast<uint32_t>(analogRead(load_cell_pin));
  }
  sample.raw_adc = static_cast<uint32_t>(adc_total / count);

  if (!imu_ready_) {
    return sample;
  }
  if (IMU.accelerationAvailable()) {
    sample.acceleration_valid =
        IMU.readAcceleration(sample.acceleration_x_g, sample.acceleration_y_g,
                             sample.acceleration_z_g) != 0;
  }
  if (IMU.gyroscopeAvailable()) {
    sample.gyroscope_valid = IMU.readGyroscope(sample.gyroscope_x_dps, sample.gyroscope_y_dps,
                                               sample.gyroscope_z_dps) != 0;
  }

  if (sample.acceleration_valid && sample.gyroscope_valid) {
    const double acceleration_magnitude =
        std::sqrt(static_cast<double>(sample.acceleration_x_g) * sample.acceleration_x_g +
                  static_cast<double>(sample.acceleration_y_g) * sample.acceleration_y_g +
                  static_cast<double>(sample.acceleration_z_g) * sample.acceleration_z_g);
    const double gyroscope_magnitude =
        std::sqrt(static_cast<double>(sample.gyroscope_x_dps) * sample.gyroscope_x_dps +
                  static_cast<double>(sample.gyroscope_y_dps) * sample.gyroscope_y_dps +
                  static_cast<double>(sample.gyroscope_z_dps) * sample.gyroscope_z_dps);
    sample.motion_stable =
        std::fabs(acceleration_magnitude - 1.0) <= acceleration_magnitude_tolerance_g &&
        gyroscope_magnitude <= gyroscope_stable_limit_dps;
  }
  return sample;
}

}  // namespace firmware
}  // namespace smart_shovel
