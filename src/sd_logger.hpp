#pragma once

#include <Arduino.h>
#include <SD.h>
#include <cstddef>
#include <cstdint>

#include "smart_shovel/csv.hpp"

namespace smart_shovel {
namespace firmware {

enum class StorageError : uint8_t {
  none,
  invalid_identity,
  card_initialization,
  open_failed,
  schema_mismatch,
  sequence_exhausted,
  write_failed,
};

class SdLogger {
 public:
  bool begin(uint8_t chip_select, const char* filename, const char* device_id,
             const char* boot_session_id) noexcept;
  bool reserve_sequence(uint32_t& sequence) noexcept;
  bool append(const CollectionRecord& record, char* work_buffer,
              std::size_t work_buffer_size) noexcept;

  [[nodiscard]] bool available() const noexcept {
    return available_;
  }
  [[nodiscard]] StorageError last_error() const noexcept {
    return last_error_;
  }
  [[nodiscard]] const char* last_error_text() const noexcept;
  [[nodiscard]] const char* boot_session_id() const noexcept {
    return boot_session_id_;
  }

 private:
  bool inspect_and_prepare(const char* filename) noexcept;
  bool write_header(const char* filename) noexcept;
  bool ensure_line_boundary(const char* filename) noexcept;

  const char* filename_{nullptr};
  char boot_session_id_[kBootSessionIdLength + 1U]{};
  uint32_t next_sequence_{1U};
  bool identity_initialized_{false};
  bool available_{false};
  StorageError last_error_{StorageError::card_initialization};
};

}  // namespace firmware
}  // namespace smart_shovel
