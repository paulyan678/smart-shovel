#pragma once

#include <cstddef>

namespace smart_shovel {
namespace firmware {

// Creates a per-boot 64-bit hexadecimal nonce from the RP2040 entropy source.
// Device ID + boot session + sequence is the aggregation identity, so sequence
// numbers can safely restart without scanning or rewriting a growing SD log.
bool create_boot_session_id(char* output, std::size_t capacity) noexcept;

}  // namespace firmware
}  // namespace smart_shovel
