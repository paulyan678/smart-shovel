#include "smart_shovel/csv.hpp"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace smart_shovel {
namespace {

constexpr const char* kCsvHeader =
    "schema_version,device_id,boot_session_id,event_sequence,event_uptime_ms,timestamp_utc,"
    "mass_g,mass_calibration_status,corrected_signal_mv,raw_adc,accel_z_g,"
    "latitude,longitude,altitude_m,gps_age_ms,satellites,gps_status,"
    "gps_wait_timed_out,system_health\n";

class FixedWriter {
 public:
  FixedWriter(char* output, std::size_t capacity) noexcept : output_(output), capacity_(capacity) {
    if (output_ != nullptr && capacity_ > 0U) {
      output_[0] = '\0';
    } else {
      valid_ = false;
    }
  }

  bool append(const char* value) noexcept {
    if (!valid_ || value == nullptr) {
      valid_ = false;
      return false;
    }
    const std::size_t value_length = std::strlen(value);
    if (value_length >= capacity_ - length_) {
      valid_ = false;
      return false;
    }
    std::memcpy(output_ + length_, value, value_length + 1U);
    length_ += value_length;
    return true;
  }

  bool append_format(const char* format, ...) noexcept {
    if (!valid_ || format == nullptr || length_ >= capacity_) {
      valid_ = false;
      return false;
    }
    va_list arguments;
    va_start(arguments, format);
    const int written = std::vsnprintf(output_ + length_, capacity_ - length_, format, arguments);
    va_end(arguments);
    if (written < 0 || static_cast<std::size_t>(written) >= capacity_ - length_) {
      valid_ = false;
      return false;
    }
    length_ += static_cast<std::size_t>(written);
    return true;
  }

  [[nodiscard]] CsvFormatResult finish() noexcept {
    if (!valid_) {
      if (output_ != nullptr && capacity_ > 0U) {
        output_[0] = '\0';
      }
      return CsvFormatResult{};
    }
    return CsvFormatResult{true, length_};
  }

 private:
  char* output_{nullptr};
  std::size_t capacity_{0U};
  std::size_t length_{0U};
  bool valid_{true};
};

bool valid_location(double latitude, double longitude) noexcept {
  return std::isfinite(latitude) && std::isfinite(longitude) && latitude >= -90.0 &&
         latitude <= 90.0 && longitude >= -180.0 && longitude <= 180.0;
}

}  // namespace

bool valid_device_id(const char* device_id) noexcept {
  if (device_id == nullptr || device_id[0] == '\0') {
    return false;
  }
  std::size_t length = 0U;
  for (; device_id[length] != '\0'; ++length) {
    if (length >= kMaximumDeviceIdLength) {
      return false;
    }
    const char value = device_id[length];
    const bool alphanumeric = (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
                              (value >= '0' && value <= '9');
    if (!alphanumeric && value != '-' && value != '_' && value != '.') {
      return false;
    }
  }
  return length > 0U;
}

bool valid_boot_session_id(const char* boot_session_id) noexcept {
  if (boot_session_id == nullptr) {
    return false;
  }
  for (std::size_t index = 0U; index < kBootSessionIdLength; ++index) {
    const char value = boot_session_id[index];
    if (value == '\0') {
      return false;
    }
    const bool decimal_digit = value >= '0' && value <= '9';
    const bool lowercase_hex = value >= 'a' && value <= 'f';
    const bool uppercase_hex = value >= 'A' && value <= 'F';
    if (!decimal_digit && !lowercase_hex && !uppercase_hex) {
      return false;
    }
  }
  return boot_session_id[kBootSessionIdLength] == '\0';
}

bool same_identity(const RecordIdentity& left, const RecordIdentity& right) noexcept {
  return left.event_sequence == right.event_sequence && valid_device_id(left.device_id) &&
         valid_device_id(right.device_id) && valid_boot_session_id(left.boot_session_id) &&
         valid_boot_session_id(right.boot_session_id) &&
         std::strcmp(left.device_id, right.device_id) == 0 &&
         std::strcmp(left.boot_session_id, right.boot_session_id) == 0;
}

const char* csv_header() noexcept {
  return kCsvHeader;
}

CsvFormatResult format_csv_header(char* output, std::size_t capacity) noexcept {
  FixedWriter writer(output, capacity);
  writer.append(kCsvHeader);
  return writer.finish();
}

CsvFormatResult format_csv_record(const CollectionRecord& record, char* output,
                                  std::size_t capacity) noexcept {
  FixedWriter writer(output, capacity);
  if (record.schema_version != kCurrentCsvSchemaVersion ||
      !valid_device_id(record.identity.device_id) ||
      !valid_boot_session_id(record.identity.boot_session_id)) {
    if (output != nullptr && capacity > 0U) {
      output[0] = '\0';
    }
    return CsvFormatResult{};
  }

  writer.append_format("%u,", static_cast<unsigned>(record.schema_version));
  writer.append(record.identity.device_id);
  writer.append(",");
  writer.append(record.identity.boot_session_id);
  writer.append_format(",%lu,%lu,", static_cast<unsigned long>(record.identity.event_sequence),
                       static_cast<unsigned long>(record.event_uptime_ms));

  if (record.timestamp_valid) {
    if (!valid_utc_datetime(record.timestamp_utc)) {
      if (output != nullptr && capacity > 0U) {
        output[0] = '\0';
      }
      return CsvFormatResult{};
    }
    writer.append_format("%04u-%02u-%02uT%02u:%02u:%02uZ",
                         static_cast<unsigned>(record.timestamp_utc.year),
                         static_cast<unsigned>(record.timestamp_utc.month),
                         static_cast<unsigned>(record.timestamp_utc.day),
                         static_cast<unsigned>(record.timestamp_utc.hour),
                         static_cast<unsigned>(record.timestamp_utc.minute),
                         static_cast<unsigned>(record.timestamp_utc.second));
  }
  writer.append(",");

  if (record.mass_valid) {
    if (!std::isfinite(record.mass_grams)) {
      if (output != nullptr && capacity > 0U) {
        output[0] = '\0';
      }
      return CsvFormatResult{};
    }
    writer.append_format("%.3f", record.mass_grams);
  }
  writer.append(",");
  writer.append(mass_calibration_status_text(record.mass_calibration_status));
  writer.append(",");

  if (record.corrected_signal_valid) {
    if (!std::isfinite(record.corrected_signal_millivolts)) {
      if (output != nullptr && capacity > 0U) {
        output[0] = '\0';
      }
      return CsvFormatResult{};
    }
    writer.append_format("%.3f", record.corrected_signal_millivolts);
  }
  writer.append(",");

  if (record.raw_adc_valid) {
    writer.append_format("%lu", static_cast<unsigned long>(record.raw_adc));
  }
  writer.append(",");

  if (record.acceleration_z_valid) {
    if (!std::isfinite(record.acceleration_z_g)) {
      if (output != nullptr && capacity > 0U) {
        output[0] = '\0';
      }
      return CsvFormatResult{};
    }
    writer.append_format("%.6f", record.acceleration_z_g);
  }
  writer.append(",");

  if (record.location_valid) {
    if (!valid_location(record.latitude, record.longitude)) {
      if (output != nullptr && capacity > 0U) {
        output[0] = '\0';
      }
      return CsvFormatResult{};
    }
    writer.append_format("%.7f,%.7f", record.latitude, record.longitude);
  } else {
    writer.append(",");
  }
  writer.append(",");

  if (record.altitude_valid) {
    if (!std::isfinite(record.altitude_meters)) {
      if (output != nullptr && capacity > 0U) {
        output[0] = '\0';
      }
      return CsvFormatResult{};
    }
    writer.append_format("%.2f", record.altitude_meters);
  }
  writer.append(",");

  if (record.gps_age_valid) {
    writer.append_format("%lu", static_cast<unsigned long>(record.gps_age_ms));
  }
  writer.append(",");

  if (record.satellites_valid) {
    writer.append_format("%lu", static_cast<unsigned long>(record.satellites));
  }
  writer.append(",");
  writer.append(gps_status_text(record.gps_status));
  writer.append(",");
  writer.append(record.gps_wait_timed_out ? "true" : "false");
  writer.append(",");
  writer.append(system_health_text(record.system_health));
  writer.append("\n");
  return writer.finish();
}

}  // namespace smart_shovel
