#include "gps_receiver.hpp"

#include <algorithm>
#include <limits>

namespace smart_shovel {
namespace firmware {
namespace {

uint32_t finite_age(uint32_t age) noexcept {
  return age == std::numeric_limits<uint32_t>::max() ? std::numeric_limits<uint32_t>::max() : age;
}

}  // namespace

void GpsReceiver::begin(uint32_t baud) noexcept {
  Serial1.begin(baud);
}

void GpsReceiver::poll(uint32_t now_ms) noexcept {
  while (Serial1.available() > 0) {
    const int value = Serial1.read();
    if (value >= 0) {
      stream_seen_ = true;
      last_byte_ms_ = now_ms;
      (void)parser_.encode(static_cast<char>(value));
    }
  }
}

GpsSnapshot GpsReceiver::snapshot(uint32_t now_ms) noexcept {
  GpsSnapshot result{};
  result.stream_seen = stream_seen_;
  result.last_byte_ms = last_byte_ms_;

  result.fix.has_location = parser_.location.isValid();
  if (result.fix.has_location) {
    result.fix.latitude = parser_.location.lat();
    result.fix.longitude = parser_.location.lng();
    result.fix.reported_location_age_ms = finite_age(parser_.location.age());
    result.fix.location_updated_ms = now_ms - result.fix.reported_location_age_ms;
  }

  result.fix.has_altitude = parser_.altitude.isValid();
  if (result.fix.has_altitude) {
    result.fix.altitude_meters = parser_.altitude.meters();
    result.fix.reported_altitude_age_ms = finite_age(parser_.altitude.age());
    result.fix.altitude_updated_ms = now_ms - result.fix.reported_altitude_age_ms;
  }

  result.fix.has_date = parser_.date.isValid();
  result.fix.has_time = parser_.time.isValid();
  if (result.fix.has_date) {
    result.fix.utc.year = parser_.date.year();
    result.fix.utc.month = parser_.date.month();
    result.fix.utc.day = parser_.date.day();
  }
  if (result.fix.has_time) {
    result.fix.utc.hour = parser_.time.hour();
    result.fix.utc.minute = parser_.time.minute();
    result.fix.utc.second = parser_.time.second();
  }
  if (result.fix.has_date && result.fix.has_time) {
    const uint32_t date_age = finite_age(parser_.date.age());
    const uint32_t time_age = finite_age(parser_.time.age());
    result.fix.reported_datetime_age_ms = std::max(date_age, time_age);
    result.fix.datetime_updated_ms = now_ms - result.fix.reported_datetime_age_ms;
  }

  result.satellites_valid = parser_.satellites.isValid();
  if (result.satellites_valid) {
    result.satellites = parser_.satellites.value();
    result.fix.has_satellites = true;
    result.fix.satellites = result.satellites;
    result.fix.reported_satellites_age_ms = finite_age(parser_.satellites.age());
    result.fix.satellites_updated_ms = now_ms - result.fix.reported_satellites_age_ms;
  }
  return result;
}

}  // namespace firmware
}  // namespace smart_shovel
