#include "boot_session.hpp"

#include <cstdint>

#include <pico/rand.h>

#include "smart_shovel/csv.hpp"

namespace smart_shovel {
namespace firmware {

bool create_boot_session_id(char* output, std::size_t capacity) noexcept {
  if (output == nullptr || capacity <= kBootSessionIdLength) {
    return false;
  }
  static constexpr char kHexDigits[] = "0123456789ABCDEF";
  std::uint64_t value = get_rand_64();
  for (std::size_t index = 0U; index < kBootSessionIdLength; ++index) {
    output[kBootSessionIdLength - index - 1U] = kHexDigits[value & 0x0fU];
    value >>= 4U;
  }
  output[kBootSessionIdLength] = '\0';
  return valid_boot_session_id(output);
}

}  // namespace firmware
}  // namespace smart_shovel
