#ifndef SMART_SHOVEL_SEQUENCE_SCANNER_HPP
#define SMART_SHOVEL_SEQUENCE_SCANNER_HPP

#include <cstddef>
#include <cstdint>

#include "smart_shovel/csv.hpp"

namespace smart_shovel {

// Streaming scanner for schema_version, device_id, boot_session_id, and
// event_sequence, the first four columns of the Smart Shovel CSV schema. A
// newline-less trailing row is reported as partial. If all four identity fields
// are complete and valid, its sequence is reserved to prevent reuse after an
// interrupted write, including a cut immediately after the sequence digits.
class SequenceScanner {
 public:
  SequenceScanner(const char* device_id, const char* boot_session_id,
                  std::uint32_t initial_sequence = 1U) noexcept;

  void reset() noexcept;
  void feed(const char* data, std::size_t length) noexcept;
  void finish() noexcept;

  [[nodiscard]] bool next_sequence(std::uint32_t& output) const noexcept;
  [[nodiscard]] bool valid_target() const noexcept {
    return valid_target_;
  }
  [[nodiscard]] bool trailing_partial_record() const noexcept {
    return trailing_partial_record_;
  }
  [[nodiscard]] std::size_t valid_record_count() const noexcept {
    return valid_records_;
  }
  [[nodiscard]] std::size_t malformed_record_count() const noexcept {
    return malformed_records_;
  }

 private:
  enum class Phase {
    schema_field,
    device_field,
    boot_session_field,
    sequence_field,
    remainder,
    invalid,
  };

  void consume(char value) noexcept;
  void finish_line() noexcept;
  void reset_line() noexcept;
  void reserve_identity_if_valid() noexcept;
  [[nodiscard]] bool parse_schema_version(std::uint32_t& output) const noexcept;
  [[nodiscard]] bool parse_sequence(std::uint32_t& output) const noexcept;

  char target_device_id_[kMaximumDeviceIdLength + 1U]{};
  char target_boot_session_id_[kBootSessionIdLength + 1U]{};
  std::uint32_t initial_sequence_{1U};
  std::uint32_t maximum_sequence_{0U};
  bool have_sequence_{false};
  bool valid_target_{false};
  bool trailing_partial_record_{false};
  std::size_t valid_records_{0U};
  std::size_t malformed_records_{0U};

  char schema_field_[16U]{};
  char device_field_[kMaximumDeviceIdLength + 1U]{};
  char boot_session_field_[kBootSessionIdLength + 1U]{};
  char sequence_field_[17U]{};
  std::size_t schema_length_{0U};
  std::size_t device_length_{0U};
  std::size_t boot_session_length_{0U};
  std::size_t sequence_length_{0U};
  Phase phase_{Phase::schema_field};
  bool line_has_data_{false};
};

}  // namespace smart_shovel

#endif
