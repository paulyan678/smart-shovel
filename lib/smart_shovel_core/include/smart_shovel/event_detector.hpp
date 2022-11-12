#ifndef SMART_SHOVEL_EVENT_DETECTOR_HPP
#define SMART_SHOVEL_EVENT_DETECTOR_HPP

#include <cstddef>
#include <cstdint>

namespace smart_shovel {

struct EventDetectorConfig {
  double trigger_threshold_grams{100.0};
  double release_threshold_grams{50.0};
  double maximum_stable_delta_grams{10.0};
  std::size_t required_stable_samples{3U};
  std::size_t required_release_samples{3U};
  std::uint32_t cooldown_ms{1000U};
};

enum class EventDetectorState {
  idle,
  candidate,
  latched,
  release_candidate,
  cooldown,
};

struct EventDetectorUpdate {
  EventDetectorState state{EventDetectorState::idle};
  bool event_emitted{false};
  bool sample_was_stable{false};
};

class EventDetector {
 public:
  explicit EventDetector(const EventDetectorConfig& config = EventDetectorConfig{}) noexcept;

  void reset() noexcept;
  [[nodiscard]] EventDetectorUpdate update(double filtered_mass_grams, bool motion_stable,
                                           std::uint32_t now_ms) noexcept;

  [[nodiscard]] EventDetectorState state() const noexcept {
    return state_;
  }

 private:
  EventDetectorConfig config_{};
  EventDetectorState state_{EventDetectorState::idle};
  std::size_t candidate_count_{0U};
  std::size_t release_count_{0U};
  double previous_mass_grams_{0.0};
  bool have_previous_mass_{false};
  std::uint32_t cooldown_started_ms_{0U};
};

}  // namespace smart_shovel

#endif
