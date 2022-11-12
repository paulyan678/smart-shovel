#include "smart_shovel/sequence_scanner.hpp"

#include <cstring>
#include <limits>

namespace smart_shovel {
namespace {

bool parse_uint32(const char* text, std::size_t length, std::uint32_t& output) noexcept {
  if (text == nullptr || length == 0U) {
    return false;
  }
  std::uint32_t parsed = 0U;
  for (std::size_t index = 0U; index < length; ++index) {
    const char value = text[index];
    if (value < '0' || value > '9') {
      return false;
    }
    const std::uint32_t digit = static_cast<std::uint32_t>(value - '0');
    if (parsed > (std::numeric_limits<std::uint32_t>::max() - digit) / 10U) {
      return false;
    }
    parsed = parsed * 10U + digit;
  }
  output = parsed;
  return true;
}

}  // namespace

SequenceScanner::SequenceScanner(const char* device_id, const char* boot_session_id,
                                 std::uint32_t initial_sequence) noexcept
    : initial_sequence_(initial_sequence) {
  valid_target_ = valid_device_id(device_id) && valid_boot_session_id(boot_session_id);
  if (valid_target_) {
    const std::size_t device_length = std::strlen(device_id);
    std::memcpy(target_device_id_, device_id, device_length + 1U);
    std::memcpy(target_boot_session_id_, boot_session_id, kBootSessionIdLength + 1U);
  }
  reset();
}

void SequenceScanner::reset() noexcept {
  maximum_sequence_ = 0U;
  have_sequence_ = false;
  trailing_partial_record_ = false;
  valid_records_ = 0U;
  malformed_records_ = 0U;
  reset_line();
}

void SequenceScanner::reset_line() noexcept {
  schema_field_[0] = '\0';
  device_field_[0] = '\0';
  boot_session_field_[0] = '\0';
  sequence_field_[0] = '\0';
  schema_length_ = 0U;
  device_length_ = 0U;
  boot_session_length_ = 0U;
  sequence_length_ = 0U;
  phase_ = Phase::schema_field;
  line_has_data_ = false;
}

void SequenceScanner::feed(const char* data, std::size_t length) noexcept {
  if (data == nullptr) {
    return;
  }
  for (std::size_t index = 0U; index < length; ++index) {
    consume(data[index]);
  }
}

void SequenceScanner::consume(char value) noexcept {
  if (value == '\r') {
    return;
  }
  if (value == '\n') {
    finish_line();
    reset_line();
    return;
  }

  line_has_data_ = true;
  if (value == ',') {
    if (phase_ == Phase::schema_field) {
      schema_field_[schema_length_] = '\0';
      phase_ = Phase::device_field;
    } else if (phase_ == Phase::device_field) {
      device_field_[device_length_] = '\0';
      phase_ = Phase::boot_session_field;
    } else if (phase_ == Phase::boot_session_field) {
      boot_session_field_[boot_session_length_] = '\0';
      phase_ = Phase::sequence_field;
    } else if (phase_ == Phase::sequence_field) {
      sequence_field_[sequence_length_] = '\0';
      phase_ = Phase::remainder;
    }
    return;
  }

  if (phase_ == Phase::schema_field) {
    if (schema_length_ >= sizeof(schema_field_) - 1U) {
      phase_ = Phase::invalid;
      return;
    }
    schema_field_[schema_length_++] = value;
    schema_field_[schema_length_] = '\0';
  } else if (phase_ == Phase::device_field) {
    if (device_length_ >= kMaximumDeviceIdLength) {
      phase_ = Phase::invalid;
      return;
    }
    device_field_[device_length_++] = value;
    device_field_[device_length_] = '\0';
  } else if (phase_ == Phase::boot_session_field) {
    if (boot_session_length_ >= kBootSessionIdLength) {
      phase_ = Phase::invalid;
      return;
    }
    boot_session_field_[boot_session_length_++] = value;
    boot_session_field_[boot_session_length_] = '\0';
  } else if (phase_ == Phase::sequence_field) {
    if (sequence_length_ >= sizeof(sequence_field_) - 1U) {
      phase_ = Phase::invalid;
      return;
    }
    sequence_field_[sequence_length_++] = value;
    sequence_field_[sequence_length_] = '\0';
  }
}

bool SequenceScanner::parse_schema_version(std::uint32_t& output) const noexcept {
  return parse_uint32(schema_field_, schema_length_, output);
}

bool SequenceScanner::parse_sequence(std::uint32_t& output) const noexcept {
  return parse_uint32(sequence_field_, sequence_length_, output);
}

void SequenceScanner::reserve_identity_if_valid() noexcept {
  const bool identity_fields_complete =
      phase_ == Phase::sequence_field || phase_ == Phase::remainder;
  if (!identity_fields_complete || !valid_target_ || !valid_device_id(device_field_) ||
      !valid_boot_session_id(boot_session_field_) ||
      std::strcmp(device_field_, target_device_id_) != 0 ||
      std::strcmp(boot_session_field_, target_boot_session_id_) != 0) {
    return;
  }

  std::uint32_t schema_version = 0U;
  std::uint32_t sequence = 0U;
  if (!parse_schema_version(schema_version) || schema_version != kCurrentCsvSchemaVersion ||
      !parse_sequence(sequence)) {
    return;
  }

  if (!have_sequence_ || sequence > maximum_sequence_) {
    maximum_sequence_ = sequence;
    have_sequence_ = true;
  }
}

void SequenceScanner::finish_line() noexcept {
  if (!line_has_data_) {
    return;
  }
  if (phase_ != Phase::remainder) {
    reserve_identity_if_valid();
    ++malformed_records_;
    return;
  }

  if (std::strcmp(schema_field_, "schema_version") == 0 &&
      std::strcmp(device_field_, "device_id") == 0 &&
      std::strcmp(boot_session_field_, "boot_session_id") == 0 &&
      std::strcmp(sequence_field_, "event_sequence") == 0) {
    return;
  }

  std::uint32_t schema_version = 0U;
  std::uint32_t sequence = 0U;
  if (!parse_schema_version(schema_version) || schema_version != kCurrentCsvSchemaVersion ||
      !valid_device_id(device_field_) || !valid_boot_session_id(boot_session_field_) ||
      !parse_sequence(sequence)) {
    ++malformed_records_;
    return;
  }

  ++valid_records_;
  if (valid_target_ && std::strcmp(device_field_, target_device_id_) == 0 &&
      std::strcmp(boot_session_field_, target_boot_session_id_) == 0 &&
      (!have_sequence_ || sequence > maximum_sequence_)) {
    maximum_sequence_ = sequence;
    have_sequence_ = true;
  }
}

void SequenceScanner::finish() noexcept {
  if (line_has_data_) {
    trailing_partial_record_ = true;
    reserve_identity_if_valid();
    reset_line();
  }
}

bool SequenceScanner::next_sequence(std::uint32_t& output) const noexcept {
  if (!valid_target_) {
    return false;
  }
  if (!have_sequence_) {
    output = initial_sequence_;
    return true;
  }
  if (maximum_sequence_ == std::numeric_limits<std::uint32_t>::max()) {
    return false;
  }
  const std::uint32_t scanned_next = maximum_sequence_ + 1U;
  output = scanned_next < initial_sequence_ ? initial_sequence_ : scanned_next;
  return true;
}

}  // namespace smart_shovel
