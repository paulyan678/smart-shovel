#include "smart_shovel/health.hpp"

namespace smart_shovel {

const char* system_health_text(SystemHealth health) noexcept {
  switch (health) {
    case SystemHealth::gps_degraded:
      return "gps_degraded";
    case SystemHealth::storage_degraded:
      return "storage_degraded";
    case SystemHealth::gps_and_storage_degraded:
      return "gps_and_storage_degraded";
    case SystemHealth::healthy:
    default:
      return "healthy";
  }
}

}  // namespace smart_shovel
