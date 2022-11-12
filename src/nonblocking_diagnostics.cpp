#include "nonblocking_diagnostics.hpp"

#include <USB/PluggableUSBSerial.h>

#include <limits>

namespace smart_shovel {
namespace firmware {

void NonBlockingDiagnostics::begin(unsigned long baud) noexcept {
  (void)baud;
  // Arduino Mbed initializes _SerialUSB before setup(). No blocking connection
  // wait or DTR handshake is required here.
}

size_t NonBlockingDiagnostics::write(uint8_t value) {
  if (count_ >= kCapacity) {
    note_dropped(1U);
    return 0U;
  }
  buffer_[(head_ + count_) % kCapacity] = value;
  ++count_;
  return 1U;
}

size_t NonBlockingDiagnostics::write(const uint8_t* values, size_t length) {
  if (values == nullptr) {
    note_dropped(length);
    return 0U;
  }
  size_t accepted = 0U;
  while (accepted < length && count_ < kCapacity) {
    buffer_[(head_ + count_) % kCapacity] = values[accepted];
    ++accepted;
    ++count_;
  }
  note_dropped(length - accepted);
  return accepted;
}

void NonBlockingDiagnostics::poll() noexcept {
  if (count_ == 0U) {
    return;
  }
  const size_t contiguous = count_ < (kCapacity - head_) ? count_ : (kCapacity - head_);
  uint32_t sent = 0U;
  _SerialUSB.send_nb(buffer_.data() + head_, static_cast<uint32_t>(contiguous), &sent, true);
  if (sent > contiguous) {
    sent = static_cast<uint32_t>(contiguous);
  }
  head_ = (head_ + sent) % kCapacity;
  count_ -= sent;
}

void NonBlockingDiagnostics::note_dropped(size_t count) noexcept {
  const uint32_t maximum = std::numeric_limits<uint32_t>::max();
  if (count >= maximum - dropped_byte_count_) {
    dropped_byte_count_ = maximum;
  } else {
    dropped_byte_count_ += static_cast<uint32_t>(count);
  }
}

}  // namespace firmware
}  // namespace smart_shovel
