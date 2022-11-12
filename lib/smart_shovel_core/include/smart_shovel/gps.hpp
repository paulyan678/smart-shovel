#ifndef SMART_SHOVEL_GPS_HPP
#define SMART_SHOVEL_GPS_HPP

#include <cstdint>

namespace smart_shovel {

struct UtcDateTime {
  std::uint16_t year{0U};
  std::uint8_t month{0U};
  std::uint8_t day{0U};
  std::uint8_t hour{0U};
  std::uint8_t minute{0U};
  std::uint8_t second{0U};
};

[[nodiscard]] bool is_leap_year(std::uint16_t year) noexcept;
[[nodiscard]] bool valid_utc_datetime(const UtcDateTime& value) noexcept;

struct GpsFix {
  bool has_location{false};
  double latitude{0.0};
  double longitude{0.0};
  std::uint32_t location_updated_ms{0U};
  std::uint32_t reported_location_age_ms{0U};

  bool has_altitude{false};
  double altitude_meters{0.0};
  std::uint32_t altitude_updated_ms{0U};
  std::uint32_t reported_altitude_age_ms{0U};

  bool has_satellites{false};
  std::uint32_t satellites{0U};
  std::uint32_t satellites_updated_ms{0U};
  std::uint32_t reported_satellites_age_ms{0U};

  bool has_date{false};
  bool has_time{false};
  UtcDateTime utc{};
  std::uint32_t datetime_updated_ms{0U};
  std::uint32_t reported_datetime_age_ms{0U};
};

struct GpsFreshnessPolicy {
  std::uint32_t maximum_location_age_ms{5000U};
  std::uint32_t maximum_datetime_age_ms{5000U};
};

enum class GpsLocationStatus {
  valid,
  no_fix,
  invalid,
  stale,
};

enum class GpsDateTimeStatus {
  valid,
  missing,
  invalid,
  stale,
};

enum class GpsStatus {
  valid,
  location_only,
  no_fix,
  invalid,
  stale,
  timeout,
};

struct GpsAssessment {
  GpsLocationStatus location{GpsLocationStatus::no_fix};
  GpsDateTimeStatus datetime{GpsDateTimeStatus::missing};
  GpsStatus overall{GpsStatus::no_fix};
  bool altitude_valid{false};
  bool satellites_valid{false};
  std::uint32_t location_age_ms{0U};
  std::uint32_t datetime_age_ms{0U};
  std::uint32_t altitude_age_ms{0U};
  std::uint32_t satellites_age_ms{0U};
};

[[nodiscard]] GpsAssessment assess_gps(
    const GpsFix& fix, std::uint32_t now_ms,
    const GpsFreshnessPolicy& policy = GpsFreshnessPolicy{}) noexcept;

// Retain the strongest independently observed location, UTC, altitude, and
// satellite components. The retained assessment freezes validity and age at
// selection time so event-valid evidence does not decay while waiting for a
// complementary NMEA component within the bounded event window.
[[nodiscard]] GpsAssessment merge_best_gps_fix(
    GpsFix& retained, const GpsAssessment& retained_assessment, const GpsFix& candidate,
    std::uint32_t now_ms, const GpsFreshnessPolicy& policy = GpsFreshnessPolicy{}) noexcept;

[[nodiscard]] const char* gps_status_text(GpsStatus status) noexcept;

}  // namespace smart_shovel

#endif
