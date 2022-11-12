#pragma once

#include <Arduino.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace smart_shovel {
namespace firmware {

// Print-compatible USB diagnostics that never wait for a host. Bytes are
// buffered in fixed RAM, sent through the target core's nonblocking CDC API,
// and deliberately dropped when the queue is full.
class NonBlockingDiagnostics : public Print {
 public:
  void begin(unsigned long baud) noexcept;
  size_t write(uint8_t value) override;
  size_t write(const uint8_t* values, size_t length) override;
  void poll() noexcept;

  [[nodiscard]] uint32_t dropped_byte_count() const noexcept {
    return dropped_byte_count_;
  }

  using Print::write;

 private:
  void note_dropped(size_t count) noexcept;

  static constexpr size_t kCapacity = 512U;
  std::array<uint8_t, kCapacity> buffer_{};
  size_t head_{0U};
  size_t count_{0U};
  uint32_t dropped_byte_count_{0U};
};

}  // namespace firmware
}  // namespace smart_shovel
