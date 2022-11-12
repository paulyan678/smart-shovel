#include "smart_shovel/gps.hpp"

#include <cmath>

#include "smart_shovel/time_utils.hpp"

namespace smart_shovel {
namespace {

std::uint8_t days_in_month(std::uint16_t year, std::uint8_t month) noexcept {
  static constexpr std::uint8_t days[] = {31U, 28U, 31U, 30U, 31U, 30U,
                                          31U, 31U, 30U, 31U, 30U, 31U};
  if (month == 0U || month > 12U) {
    return 0U;
  }
  if (month == 2U && is_leap_year(year)) {
    return 29U;
  }
  return days[month - 1U];
}

std::uint32_t effective_age(std::uint32_t now_ms, std::uint32_t updated_ms,
                            std::uint32_t reported_age_ms) noexcept {
  const std::uint32_t local_age = elapsed_ms(now_ms, updated_ms);
  return local_age > reported_age_ms ? local_age : reported_age_ms;
}

std::uint8_t location_rank(GpsLocationStatus status) noexcept {
  switch (status) {
    case GpsLocationStatus::valid:
      return 3U;
    case GpsLocationStatus::stale:
      return 2U;
    case GpsLocationStatus::invalid:
      return 1U;
    case GpsLocationStatus::no_fix:
    default:
      return 0U;
  }
}

std::uint8_t datetime_rank(GpsDateTimeStatus status) noexcept {
  switch (status) {
    case GpsDateTimeStatus::valid:
      return 3U;
    case GpsDateTimeStatus::stale:
      return 2U;
    case GpsDateTimeStatus::invalid:
      return 1U;
    case GpsDateTimeStatus::missing:
    default:
      return 0U;
  }
}

bool stronger_component(std::uint8_t candidate_rank, std::uint32_t candidate_age,
                        std::uint8_t retained_rank, std::uint32_t retained_age) noexcept {
  return candidate_rank > retained_rank ||
         (candidate_rank == retained_rank && candidate_rank > 0U && candidate_age < retained_age);
}

GpsStatus overall_status(GpsLocationStatus location, GpsDateTimeStatus datetime) noexcept {
  switch (location) {
    case GpsLocationStatus::no_fix:
      return GpsStatus::no_fix;
    case GpsLocationStatus::invalid:
      return GpsStatus::invalid;
    case GpsLocationStatus::stale:
      return GpsStatus::stale;
    case GpsLocationStatus::valid:
      if (datetime == GpsDateTimeStatus::valid) {
        return GpsStatus::valid;
      }
      if (datetime == GpsDateTimeStatus::missing) {
        return GpsStatus::location_only;
      }
      return datetime == GpsDateTimeStatus::stale ? GpsStatus::stale : GpsStatus::invalid;
  }
  return GpsStatus::no_fix;
}

}  // namespace

bool is_leap_year(std::uint16_t year) noexcept {
  return (year % 4U == 0U && year % 100U != 0U) || year % 400U == 0U;
}

bool valid_utc_datetime(const UtcDateTime& value) noexcept {
  if (value.year < 2000U || value.month == 0U || value.month > 12U || value.day == 0U ||
      value.day > days_in_month(value.year, value.month)) {
    return false;
  }
  return value.hour <= 23U && value.minute <= 59U && value.second <= 59U;
}

GpsAssessment assess_gps(const GpsFix& fix, std::uint32_t now_ms,
                         const GpsFreshnessPolicy& policy) noexcept {
  GpsAssessment result{};

  if (fix.has_location) {
    result.location_age_ms =
        effective_age(now_ms, fix.location_updated_ms, fix.reported_location_age_ms);
  }
  if (fix.has_date && fix.has_time) {
    result.datetime_age_ms =
        effective_age(now_ms, fix.datetime_updated_ms, fix.reported_datetime_age_ms);
  }

  if (!fix.has_location) {
    result.location = GpsLocationStatus::no_fix;
  } else if (!std::isfinite(fix.latitude) || !std::isfinite(fix.longitude) ||
             fix.latitude < -90.0 || fix.latitude > 90.0 || fix.longitude < -180.0 ||
             fix.longitude > 180.0) {
    result.location = GpsLocationStatus::invalid;
  } else if (result.location_age_ms > policy.maximum_location_age_ms) {
    result.location = GpsLocationStatus::stale;
  } else {
    result.location = GpsLocationStatus::valid;
  }

  if (!fix.has_date || !fix.has_time) {
    result.datetime = GpsDateTimeStatus::missing;
  } else if (!valid_utc_datetime(fix.utc)) {
    result.datetime = GpsDateTimeStatus::invalid;
  } else if (result.datetime_age_ms > policy.maximum_datetime_age_ms) {
    result.datetime = GpsDateTimeStatus::stale;
  } else {
    result.datetime = GpsDateTimeStatus::valid;
  }

  result.altitude_age_ms =
      effective_age(now_ms, fix.altitude_updated_ms, fix.reported_altitude_age_ms);
  result.satellites_age_ms =
      effective_age(now_ms, fix.satellites_updated_ms, fix.reported_satellites_age_ms);
  result.altitude_valid = fix.has_altitude && std::isfinite(fix.altitude_meters) &&
                          result.altitude_age_ms <= policy.maximum_location_age_ms;
  result.satellites_valid =
      fix.has_satellites && result.satellites_age_ms <= policy.maximum_location_age_ms;

  result.overall = overall_status(result.location, result.datetime);
  return result;
}

GpsAssessment merge_best_gps_fix(GpsFix& retained, const GpsAssessment& retained_assessment,
                                 const GpsFix& candidate, std::uint32_t now_ms,
                                 const GpsFreshnessPolicy& policy) noexcept {
  GpsAssessment merged = retained_assessment;
  const GpsAssessment candidate_assessment = assess_gps(candidate, now_ms, policy);

  if (stronger_component(location_rank(candidate_assessment.location),
                         candidate_assessment.location_age_ms, location_rank(merged.location),
                         merged.location_age_ms)) {
    retained.has_location = candidate.has_location;
    retained.latitude = candidate.latitude;
    retained.longitude = candidate.longitude;
    retained.location_updated_ms = candidate.location_updated_ms;
    retained.reported_location_age_ms = candidate.reported_location_age_ms;
    merged.location = candidate_assessment.location;
    merged.location_age_ms = candidate_assessment.location_age_ms;
  }

  if (stronger_component(datetime_rank(candidate_assessment.datetime),
                         candidate_assessment.datetime_age_ms, datetime_rank(merged.datetime),
                         merged.datetime_age_ms)) {
    retained.has_date = candidate.has_date;
    retained.has_time = candidate.has_time;
    retained.utc = candidate.utc;
    retained.datetime_updated_ms = candidate.datetime_updated_ms;
    retained.reported_datetime_age_ms = candidate.reported_datetime_age_ms;
    merged.datetime = candidate_assessment.datetime;
    merged.datetime_age_ms = candidate_assessment.datetime_age_ms;
  }

  if (candidate_assessment.altitude_valid &&
      (!merged.altitude_valid || candidate_assessment.altitude_age_ms < merged.altitude_age_ms)) {
    retained.has_altitude = true;
    retained.altitude_meters = candidate.altitude_meters;
    retained.altitude_updated_ms = candidate.altitude_updated_ms;
    retained.reported_altitude_age_ms = candidate.reported_altitude_age_ms;
    merged.altitude_valid = true;
    merged.altitude_age_ms = candidate_assessment.altitude_age_ms;
  }

  if (candidate_assessment.satellites_valid &&
      (!merged.satellites_valid ||
       candidate_assessment.satellites_age_ms < merged.satellites_age_ms)) {
    retained.has_satellites = true;
    retained.satellites = candidate.satellites;
    retained.satellites_updated_ms = candidate.satellites_updated_ms;
    retained.reported_satellites_age_ms = candidate.reported_satellites_age_ms;
    merged.satellites_valid = true;
    merged.satellites_age_ms = candidate_assessment.satellites_age_ms;
  }

  merged.overall = overall_status(merged.location, merged.datetime);
  return merged;
}

const char* gps_status_text(GpsStatus status) noexcept {
  switch (status) {
    case GpsStatus::valid:
      return "valid";
    case GpsStatus::location_only:
      return "location_only";
    case GpsStatus::invalid:
      return "invalid";
    case GpsStatus::stale:
      return "stale";
    case GpsStatus::timeout:
      return "timeout";
    case GpsStatus::no_fix:
    default:
      return "no_fix";
  }
}

}  // namespace smart_shovel
