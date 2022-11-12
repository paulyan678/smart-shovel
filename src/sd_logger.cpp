#include "sd_logger.hpp"

#include <cstring>
#include <limits>

namespace smart_shovel {
namespace firmware {
namespace {

// SD 1.3.0's File::flush()/close() discard the underlying SdFile::sync()
// result. O_SYNC makes the one write call itself fail if data, FAT, or the
// directory entry cannot be synchronized; the written/getWriteError checks
// below can therefore decide whether it is safe to complete the event.
constexpr uint8_t kDurableWriteMode = static_cast<uint8_t>(FILE_WRITE | O_SYNC);

bool header_matches(File& file) noexcept {
  const char* expected = csv_header();
  std::size_t offset = 0U;
  while (expected[offset] != '\0') {
    if (file.available() <= 0) {
      return false;
    }
    const int value = file.read();
    if (value < 0 || static_cast<char>(value) != expected[offset]) {
      return false;
    }
    ++offset;
  }
  return true;
}

}  // namespace

bool SdLogger::begin(uint8_t chip_select, const char* filename, const char* device_id,
                     const char* boot_session_id) noexcept {
  available_ = false;
  filename_ = filename;
  if (!valid_device_id(device_id) || !valid_boot_session_id(boot_session_id)) {
    last_error_ = StorageError::invalid_identity;
    return false;
  }
  if (!identity_initialized_ || std::strcmp(boot_session_id_, boot_session_id) != 0) {
    std::memcpy(boot_session_id_, boot_session_id, kBootSessionIdLength + 1U);
    next_sequence_ = 1U;
    identity_initialized_ = true;
  }
  if (!SD.begin(chip_select)) {
    last_error_ = StorageError::card_initialization;
    return false;
  }
  if (!inspect_and_prepare(filename)) {
    return false;
  }
  available_ = true;
  last_error_ = StorageError::none;
  return true;
}

bool SdLogger::inspect_and_prepare(const char* filename) noexcept {
  if (!SD.exists(filename)) {
    return write_header(filename);
  }

  File file = SD.open(filename, FILE_READ);
  if (!file) {
    last_error_ = StorageError::open_failed;
    return false;
  }
  if (file.size() == 0U) {
    file.close();
    return write_header(filename);
  }
  if (!header_matches(file)) {
    file.close();
    last_error_ = StorageError::schema_mismatch;
    return false;
  }
  file.close();
  return ensure_line_boundary(filename);
}

bool SdLogger::write_header(const char* filename) noexcept {
  File file = SD.open(filename, kDurableWriteMode);
  if (!file) {
    last_error_ = StorageError::open_failed;
    return false;
  }
  const char* header = csv_header();
  const std::size_t length = std::strlen(header);
  const std::size_t written = file.write(reinterpret_cast<const uint8_t*>(header), length);
  const bool success = written == length && file.getWriteError() == 0;
  file.close();
  if (!success) {
    last_error_ = StorageError::write_failed;
  }
  return success;
}

bool SdLogger::ensure_line_boundary(const char* filename) noexcept {
  File file = SD.open(filename, FILE_READ);
  if (!file) {
    last_error_ = StorageError::open_failed;
    return false;
  }
  const uint32_t size = file.size();
  if (size == 0U || !file.seek(size - 1U)) {
    file.close();
    last_error_ = StorageError::open_failed;
    return false;
  }
  const int last_byte = file.read();
  file.close();
  if (last_byte == '\n') {
    return true;
  }

  file = SD.open(filename, kDurableWriteMode);
  if (!file) {
    last_error_ = StorageError::open_failed;
    return false;
  }
  const std::size_t written = file.write(static_cast<uint8_t>('\n'));
  const bool success = written == 1U && file.getWriteError() == 0;
  file.close();
  if (!success) {
    last_error_ = StorageError::write_failed;
  }
  return success;
}

bool SdLogger::reserve_sequence(uint32_t& sequence) noexcept {
  if (!available_) {
    return false;
  }
  if (next_sequence_ == std::numeric_limits<uint32_t>::max()) {
    available_ = false;
    last_error_ = StorageError::sequence_exhausted;
    return false;
  }
  sequence = next_sequence_++;
  return true;
}

bool SdLogger::append(const CollectionRecord& record, char* work_buffer,
                      std::size_t work_buffer_size) noexcept {
  if (!available_ || filename_ == nullptr || work_buffer == nullptr) {
    return false;
  }
  const CsvFormatResult formatted = format_csv_record(record, work_buffer, work_buffer_size);
  if (!formatted.success) {
    available_ = false;
    last_error_ = StorageError::write_failed;
    return false;
  }

  File file = SD.open(filename_, kDurableWriteMode);
  if (!file) {
    available_ = false;
    last_error_ = StorageError::open_failed;
    return false;
  }
  const std::size_t written =
      file.write(reinterpret_cast<const uint8_t*>(work_buffer), formatted.length);
  const bool success = written == formatted.length && file.getWriteError() == 0;
  file.close();
  if (!success) {
    available_ = false;
    last_error_ = StorageError::write_failed;
  }
  return success;
}

const char* SdLogger::last_error_text() const noexcept {
  switch (last_error_) {
    case StorageError::none:
      return "none";
    case StorageError::invalid_identity:
      return "invalid_identity";
    case StorageError::card_initialization:
      return "card_initialization";
    case StorageError::open_failed:
      return "open_failed";
    case StorageError::schema_mismatch:
      return "schema_mismatch";
    case StorageError::sequence_exhausted:
      return "sequence_exhausted";
    case StorageError::write_failed:
    default:
      return "write_failed";
  }
}

}  // namespace firmware
}  // namespace smart_shovel
