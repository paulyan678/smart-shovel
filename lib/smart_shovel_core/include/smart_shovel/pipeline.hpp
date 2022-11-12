#ifndef SMART_SHOVEL_PIPELINE_HPP
#define SMART_SHOVEL_PIPELINE_HPP

#include <cstddef>
#include <cstdint>

#include "smart_shovel/health.hpp"

namespace smart_shovel {

struct CollectionPipelineConfig {
  std::uint32_t gps_timeout_ms{3000U};
  std::uint32_t storage_retry_interval_ms{500U};
  std::size_t maximum_storage_failures{3U};
};

enum class CollectionPipelineState {
  ready,
  pending_gps,
  ready_to_store,
  waiting_storage_retry,
  degraded_storage,
};

enum class CollectionPipelineAction {
  none,
  request_gps,
  write_record,
  event_completed,
  entered_storage_degraded,
};

struct CollectionPipelineUpdate {
  CollectionPipelineState state{CollectionPipelineState::ready};
  CollectionPipelineAction action{CollectionPipelineAction::none};
};

class CollectionPipeline {
 public:
  explicit CollectionPipeline(
      const CollectionPipelineConfig& config = CollectionPipelineConfig{}) noexcept;

  void reset() noexcept;
  [[nodiscard]] CollectionPipelineUpdate begin_event(std::uint32_t now_ms) noexcept;
  [[nodiscard]] CollectionPipelineUpdate provide_gps_result(bool fresh_valid_fix,
                                                            std::uint32_t now_ms) noexcept;
  [[nodiscard]] CollectionPipelineUpdate tick(std::uint32_t now_ms) noexcept;
  [[nodiscard]] CollectionPipelineUpdate provide_storage_result(bool success,
                                                                std::uint32_t now_ms) noexcept;
  [[nodiscard]] CollectionPipelineUpdate retry_degraded_storage() noexcept;
  void note_gps_recovered() noexcept;

  [[nodiscard]] CollectionPipelineState state() const noexcept {
    return state_;
  }
  [[nodiscard]] SystemHealth health() const noexcept;
  [[nodiscard]] bool has_pending_event() const noexcept {
    return state_ != CollectionPipelineState::ready;
  }
  [[nodiscard]] bool pending_record_has_gps() const noexcept {
    return pending_record_has_gps_;
  }
  [[nodiscard]] bool gps_timed_out() const noexcept {
    return gps_timed_out_;
  }
  [[nodiscard]] std::size_t storage_failure_count() const noexcept {
    return storage_failure_count_;
  }

 private:
  [[nodiscard]] CollectionPipelineUpdate result(CollectionPipelineAction action) const noexcept;

  CollectionPipelineConfig config_{};
  CollectionPipelineState state_{CollectionPipelineState::ready};
  std::uint32_t state_started_ms_{0U};
  std::size_t storage_failure_count_{0U};
  bool pending_record_has_gps_{false};
  bool gps_timed_out_{false};
  bool gps_degraded_{false};
  bool storage_degraded_{false};
};

}  // namespace smart_shovel

#endif
