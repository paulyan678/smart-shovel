#include "smart_shovel/pipeline.hpp"

#include "smart_shovel/time_utils.hpp"

namespace smart_shovel {

CollectionPipeline::CollectionPipeline(const CollectionPipelineConfig& config) noexcept
    : config_(config) {
  if (config_.maximum_storage_failures == 0U) {
    config_.maximum_storage_failures = 1U;
  }
}

void CollectionPipeline::reset() noexcept {
  state_ = CollectionPipelineState::ready;
  state_started_ms_ = 0U;
  storage_failure_count_ = 0U;
  pending_record_has_gps_ = false;
  gps_timed_out_ = false;
  gps_degraded_ = false;
  storage_degraded_ = false;
}

CollectionPipelineUpdate CollectionPipeline::result(
    CollectionPipelineAction action) const noexcept {
  return CollectionPipelineUpdate{state_, action};
}

CollectionPipelineUpdate CollectionPipeline::begin_event(std::uint32_t now_ms) noexcept {
  if (state_ != CollectionPipelineState::ready) {
    return result(CollectionPipelineAction::none);
  }
  state_ = CollectionPipelineState::pending_gps;
  state_started_ms_ = now_ms;
  storage_failure_count_ = 0U;
  pending_record_has_gps_ = false;
  gps_timed_out_ = false;
  return result(CollectionPipelineAction::request_gps);
}

CollectionPipelineUpdate CollectionPipeline::provide_gps_result(bool fresh_valid_fix,
                                                                std::uint32_t now_ms) noexcept {
  if (state_ != CollectionPipelineState::pending_gps || !fresh_valid_fix) {
    return result(CollectionPipelineAction::none);
  }
  pending_record_has_gps_ = true;
  gps_timed_out_ = false;
  gps_degraded_ = false;
  state_ = CollectionPipelineState::ready_to_store;
  state_started_ms_ = now_ms;
  return result(CollectionPipelineAction::write_record);
}

CollectionPipelineUpdate CollectionPipeline::tick(std::uint32_t now_ms) noexcept {
  if (state_ == CollectionPipelineState::pending_gps &&
      interval_elapsed(now_ms, state_started_ms_, config_.gps_timeout_ms)) {
    pending_record_has_gps_ = false;
    gps_timed_out_ = true;
    gps_degraded_ = true;
    state_ = CollectionPipelineState::ready_to_store;
    state_started_ms_ = now_ms;
    return result(CollectionPipelineAction::write_record);
  }

  if (state_ == CollectionPipelineState::waiting_storage_retry &&
      interval_elapsed(now_ms, state_started_ms_, config_.storage_retry_interval_ms)) {
    state_ = CollectionPipelineState::ready_to_store;
    state_started_ms_ = now_ms;
    return result(CollectionPipelineAction::write_record);
  }
  return result(CollectionPipelineAction::none);
}

CollectionPipelineUpdate CollectionPipeline::provide_storage_result(bool success,
                                                                    std::uint32_t now_ms) noexcept {
  if (state_ != CollectionPipelineState::ready_to_store) {
    return result(CollectionPipelineAction::none);
  }

  if (success) {
    state_ = CollectionPipelineState::ready;
    state_started_ms_ = now_ms;
    storage_failure_count_ = 0U;
    pending_record_has_gps_ = false;
    gps_timed_out_ = false;
    storage_degraded_ = false;
    return result(CollectionPipelineAction::event_completed);
  }

  ++storage_failure_count_;
  state_started_ms_ = now_ms;
  if (storage_failure_count_ >= config_.maximum_storage_failures) {
    state_ = CollectionPipelineState::degraded_storage;
    storage_degraded_ = true;
    return result(CollectionPipelineAction::entered_storage_degraded);
  }

  state_ = CollectionPipelineState::waiting_storage_retry;
  return result(CollectionPipelineAction::none);
}

CollectionPipelineUpdate CollectionPipeline::retry_degraded_storage() noexcept {
  if (state_ != CollectionPipelineState::degraded_storage) {
    return result(CollectionPipelineAction::none);
  }
  storage_failure_count_ = 0U;
  state_ = CollectionPipelineState::ready_to_store;
  return result(CollectionPipelineAction::write_record);
}

void CollectionPipeline::note_gps_recovered() noexcept {
  gps_degraded_ = false;
}

SystemHealth CollectionPipeline::health() const noexcept {
  if (gps_degraded_ && storage_degraded_) {
    return SystemHealth::gps_and_storage_degraded;
  }
  if (gps_degraded_) {
    return SystemHealth::gps_degraded;
  }
  if (storage_degraded_) {
    return SystemHealth::storage_degraded;
  }
  return SystemHealth::healthy;
}

}  // namespace smart_shovel
