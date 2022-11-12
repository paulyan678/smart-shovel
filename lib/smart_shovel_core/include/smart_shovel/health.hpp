#ifndef SMART_SHOVEL_HEALTH_HPP
#define SMART_SHOVEL_HEALTH_HPP

namespace smart_shovel {

enum class SystemHealth {
  healthy,
  gps_degraded,
  storage_degraded,
  gps_and_storage_degraded,
};

[[nodiscard]] const char* system_health_text(SystemHealth health) noexcept;

}  // namespace smart_shovel

#endif
