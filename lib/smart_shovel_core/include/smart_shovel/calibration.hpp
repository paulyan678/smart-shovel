#ifndef SMART_SHOVEL_CALIBRATION_HPP
#define SMART_SHOVEL_CALIBRATION_HPP

#include <cstddef>
#include <cstdint>

namespace smart_shovel {

constexpr double kDefaultAdcReferenceMillivolts = 3300.0;
constexpr std::uint32_t kDefaultAdcLevels = 65536U;
constexpr double kOrientationSlopeMillivoltsPerG = -74.7089168;

struct AdcCalibration {
  double reference_millivolts{kDefaultAdcReferenceMillivolts};
  std::uint32_t levels{kDefaultAdcLevels};
};

[[nodiscard]] double adc_counts_to_millivolts(
    std::uint32_t counts, const AdcCalibration& calibration = AdcCalibration{}) noexcept;
[[nodiscard]] bool adc_counts_plausible(std::uint32_t counts,
                                        const AdcCalibration& calibration = AdcCalibration{},
                                        std::uint32_t saturation_margin_counts = 0U) noexcept;

struct TarePoint {
  double signal_millivolts{0.0};
  double acceleration_z_g{0.0};
  bool valid{false};
};

[[nodiscard]] double orientation_corrected_delta_millivolts(
    double signal_millivolts, double acceleration_z_g, const TarePoint& tare,
    double orientation_slope_millivolts_per_g = kOrientationSlopeMillivoltsPerG) noexcept;

enum class MassCalibrationStatus {
  unavailable,
  provisional,
  verified,
};

struct MassCalibration {
  double grams_per_millivolt{0.0};
  MassCalibrationStatus status{MassCalibrationStatus::unavailable};
};

// Preserved solely for compatibility with the 2022 prototype. Its physical
// provenance is insufficient, so it is explicitly provisional.
constexpr MassCalibration kLegacyProvisionalMassCalibration{-15.0,
                                                            MassCalibrationStatus::provisional};

struct WeightResult {
  double corrected_millivolts{0.0};
  double mass_grams{0.0};
  MassCalibrationStatus calibration_status{MassCalibrationStatus::unavailable};
  bool valid{false};
};

[[nodiscard]] WeightResult calculate_weight(
    double signal_millivolts, double acceleration_z_g, const TarePoint& tare,
    const MassCalibration& mass_calibration,
    double orientation_slope_millivolts_per_g = kOrientationSlopeMillivoltsPerG) noexcept;

[[nodiscard]] const char* mass_calibration_status_text(MassCalibrationStatus status) noexcept;

struct StartupTareConfig {
  std::size_t required_samples{32U};
  double maximum_signal_span_millivolts{2.0};
  double maximum_acceleration_span_g{0.03};
};

enum class StartupTareOutcome {
  collecting,
  ready,
  rejected_not_explicitly_unloaded,
  rejected_motion,
  rejected_invalid_sample,
  restarted_unstable_window,
};

class StartupTareEstimator {
 public:
  explicit StartupTareEstimator(const StartupTareConfig& config = StartupTareConfig{}) noexcept;

  void reset() noexcept;
  [[nodiscard]] StartupTareOutcome add_sample(double signal_millivolts, double acceleration_z_g,
                                              bool explicitly_unloaded,
                                              bool motion_stable) noexcept;

  [[nodiscard]] bool ready() const noexcept {
    return tare_.valid;
  }
  [[nodiscard]] std::size_t sample_count() const noexcept {
    return count_;
  }
  [[nodiscard]] const TarePoint& tare() const noexcept {
    return tare_;
  }

 private:
  void begin_window(double signal_millivolts, double acceleration_z_g) noexcept;

  StartupTareConfig config_{};
  TarePoint tare_{};
  std::size_t count_{0U};
  double signal_sum_{0.0};
  double acceleration_sum_{0.0};
  double signal_min_{0.0};
  double signal_max_{0.0};
  double acceleration_min_{0.0};
  double acceleration_max_{0.0};
};

struct AutoZeroConfig {
  std::size_t required_consecutive_samples{16U};
  double unloaded_window_millivolts{3.0};
  double correction_gain{0.25};
  double maximum_step_millivolts{0.25};
  double maximum_total_adjustment_millivolts{5.0};
};

enum class AutoZeroOutcome {
  collecting,
  adjustment_applied,
  rejected_not_explicitly_unloaded,
  rejected_motion,
  rejected_outside_window,
  rejected_invalid_sample,
  unavailable_without_tare,
};

struct AutoZeroUpdate {
  AutoZeroOutcome outcome{AutoZeroOutcome::collecting};
  double applied_adjustment_millivolts{0.0};
  std::size_t eligible_sample_count{0U};
};

class AutoZeroTracker {
 public:
  AutoZeroTracker(
      const TarePoint& initial_tare, const AutoZeroConfig& config = AutoZeroConfig{},
      double orientation_slope_millivolts_per_g = kOrientationSlopeMillivoltsPerG) noexcept;

  void reset(const TarePoint& initial_tare) noexcept;
  [[nodiscard]] AutoZeroUpdate update(double signal_millivolts, double acceleration_z_g,
                                      bool explicitly_unloaded, bool motion_stable) noexcept;

  [[nodiscard]] const TarePoint& tare() const noexcept {
    return tare_;
  }
  [[nodiscard]] double total_adjustment_millivolts() const noexcept {
    return tare_.signal_millivolts - initial_tare_.signal_millivolts;
  }

 private:
  void clear_candidate() noexcept;

  AutoZeroConfig config_{};
  TarePoint initial_tare_{};
  TarePoint tare_{};
  double orientation_slope_{kOrientationSlopeMillivoltsPerG};
  std::size_t candidate_count_{0U};
  double residual_sum_{0.0};
};

}  // namespace smart_shovel

#endif
