#ifndef SMART_SHOVEL_TIME_UTILS_HPP
#define SMART_SHOVEL_TIME_UTILS_HPP

#include <cstdint>

namespace smart_shovel {

// Unsigned subtraction is intentionally wrap-safe for intervals shorter than
// half of the uint32_t range, which comfortably covers all firmware timeouts.
constexpr std::uint32_t elapsed_ms(std::uint32_t now_ms, std::uint32_t since_ms) noexcept {
  return now_ms - since_ms;
}

constexpr bool interval_elapsed(std::uint32_t now_ms, std::uint32_t since_ms,
                                std::uint32_t interval_ms) noexcept {
  return elapsed_ms(now_ms, since_ms) >= interval_ms;
}

}  // namespace smart_shovel

#endif
