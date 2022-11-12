#ifndef SMART_SHOVEL_CSV_HPP
#define SMART_SHOVEL_CSV_HPP

#include <cstddef>
#include <cstdint>

#include "smart_shovel/calibration.hpp"
#include "smart_shovel/gps.hpp"
#include "smart_shovel/health.hpp"

namespace smart_shovel {

constexpr std::size_t kMaximumDeviceIdLength = 24U;
constexpr std::size_t kBootSessionIdLength = 16U;
constexpr std::uint16_t kCurrentCsvSchemaVersion = 2U;

struct RecordIdentity {
  const char* device_id{nullptr};
  const char* boot_session_id{nullptr};
  std::uint32_t event_sequence{0U};
};

[[nodiscard]] bool valid_device_id(const char* device_id) noexcept;
[[nodiscard]] bool valid_boot_session_id(const char* boot_session_id) noexcept;
[[nodiscard]] bool same_identity(const RecordIdentity& left, const RecordIdentity& right) noexcept;

struct CollectionRecord {
  std::uint16_t schema_version{kCurrentCsvSchemaVersion};
  RecordIdentity identity{};
  std::uint32_t event_uptime_ms{0U};

  bool timestamp_valid{false};
  UtcDateTime timestamp_utc{};

  bool mass_valid{false};
  double mass_grams{0.0};
  MassCalibrationStatus mass_calibration_status{MassCalibrationStatus::unavailable};

  bool corrected_signal_valid{false};
  double corrected_signal_millivolts{0.0};

  bool raw_adc_valid{false};
  std::uint32_t raw_adc{0U};
  bool acceleration_z_valid{false};
  double acceleration_z_g{0.0};

  bool location_valid{false};
  double latitude{0.0};
  double longitude{0.0};
  bool altitude_valid{false};
  double altitude_meters{0.0};

  bool gps_age_valid{false};
  std::uint32_t gps_age_ms{0U};
  bool satellites_valid{false};
  std::uint32_t satellites{0U};

  GpsStatus gps_status{GpsStatus::no_fix};
  bool gps_wait_timed_out{false};
  SystemHealth system_health{SystemHealth::healthy};
};

struct CsvFormatResult {
  bool success{false};
  std::size_t length{0U};
};

[[nodiscard]] const char* csv_header() noexcept;
[[nodiscard]] CsvFormatResult format_csv_header(char* output, std::size_t capacity) noexcept;
[[nodiscard]] CsvFormatResult format_csv_record(const CollectionRecord& record, char* output,
                                                std::size_t capacity) noexcept;

}  // namespace smart_shovel

#endif
