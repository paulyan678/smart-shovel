#include "smart_shovel/event_detector.hpp"

#include <cmath>

#include "smart_shovel/time_utils.hpp"

namespace smart_shovel {

EventDetector::EventDetector(const EventDetectorConfig& config) noexcept : config_(config) {
  if (config_.release_threshold_grams > config_.trigger_threshold_grams) {
    config_.release_threshold_grams = config_.trigger_threshold_grams;
  }
  if (!std::isfinite(config_.maximum_stable_delta_grams) ||
      config_.maximum_stable_delta_grams < 0.0) {
    config_.maximum_stable_delta_grams = 0.0;
  }
  if (config_.required_stable_samples == 0U) {
    config_.required_stable_samples = 1U;
  }
  if (config_.required_release_samples == 0U) {
    config_.required_release_samples = 1U;
  }
}

void EventDetector::reset() noexcept {
  state_ = EventDetectorState::idle;
  candidate_count_ = 0U;
  release_count_ = 0U;
  previous_mass_grams_ = 0.0;
  have_previous_mass_ = false;
  cooldown_started_ms_ = 0U;
}

EventDetectorUpdate EventDetector::update(double filtered_mass_grams, bool motion_stable,
                                          std::uint32_t now_ms) noexcept {
  EventDetectorUpdate result{};
  if (!std::isfinite(filtered_mass_grams)) {
    candidate_count_ = 0U;
    release_count_ = 0U;
    have_previous_mass_ = false;
    if (state_ == EventDetectorState::candidate) {
      state_ = EventDetectorState::idle;
    } else if (state_ == EventDetectorState::release_candidate) {
      state_ = EventDetectorState::latched;
    }
    result.state = state_;
    return result;
  }

  const bool delta_stable =
      !have_previous_mass_ ||
      std::fabs(filtered_mass_grams - previous_mass_grams_) <= config_.maximum_stable_delta_grams;
  const bool sample_stable = motion_stable && delta_stable;
  previous_mass_grams_ = filtered_mass_grams;
  have_previous_mass_ = true;
  result.sample_was_stable = sample_stable;

  switch (state_) {
    case EventDetectorState::idle:
    case EventDetectorState::candidate:
      if (sample_stable && filtered_mass_grams >= config_.trigger_threshold_grams) {
        ++candidate_count_;
        state_ = EventDetectorState::candidate;
        if (candidate_count_ >= config_.required_stable_samples) {
          candidate_count_ = 0U;
          state_ = EventDetectorState::latched;
          result.event_emitted = true;
        }
      } else {
        candidate_count_ = 0U;
        state_ = EventDetectorState::idle;
      }
      break;

    case EventDetectorState::latched:
    case EventDetectorState::release_candidate:
      if (sample_stable && filtered_mass_grams <= config_.release_threshold_grams) {
        ++release_count_;
        state_ = EventDetectorState::release_candidate;
        if (release_count_ >= config_.required_release_samples) {
          release_count_ = 0U;
          state_ = EventDetectorState::cooldown;
          cooldown_started_ms_ = now_ms;
        }
      } else {
        release_count_ = 0U;
        state_ = EventDetectorState::latched;
      }
      break;

    case EventDetectorState::cooldown:
      // Release was already confirmed before cooldown began. Re-arm when the
      // bounded interval expires even if a new load arrived in the meantime;
      // otherwise that load could remain invisible until it is removed again.
      if (interval_elapsed(now_ms, cooldown_started_ms_, config_.cooldown_ms)) {
        state_ = EventDetectorState::idle;
        candidate_count_ = 0U;
      }
      break;
  }

  result.state = state_;
  return result;
}

}  // namespace smart_shovel
