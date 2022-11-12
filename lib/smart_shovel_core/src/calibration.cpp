#include "smart_shovel/calibration.hpp"

#include <cmath>
#include <limits>

namespace smart_shovel {
namespace {

double clamp_value(double value, double minimum, double maximum) noexcept {
  if (value < minimum) {
    return minimum;
  }
  if (value > maximum) {
    return maximum;
  }
  return value;
}

bool finite_sample(double signal_millivolts, double acceleration_z_g) noexcept {
  return std::isfinite(signal_millivolts) && std::isfinite(acceleration_z_g);
}

}  // namespace

double adc_counts_to_millivolts(std::uint32_t counts, const AdcCalibration& calibration) noexcept {
  if (!std::isfinite(calibration.reference_millivolts) || calibration.reference_millivolts <= 0.0 ||
      calibration.levels == 0U) {
    return std::numeric_limits<double>::quiet_NaN();
  }
  return static_cast<double>(counts) * calibration.reference_millivolts /
         static_cast<double>(calibration.levels);
}

bool adc_counts_plausible(std::uint32_t counts, const AdcCalibration& calibration,
                          std::uint32_t saturation_margin_counts) noexcept {
  if (calibration.levels == 0U || counts >= calibration.levels ||
      saturation_margin_counts >= calibration.levels ||
      saturation_margin_counts > (calibration.levels - 1U) / 2U) {
    return false;
  }
  return counts >= saturation_margin_counts &&
         counts <= calibration.levels - 1U - saturation_margin_counts;
}

double orientation_corrected_delta_millivolts(double signal_millivolts, double acceleration_z_g,
                                              const TarePoint& tare,
                                              double orientation_slope_millivolts_per_g) noexcept {
  if (!tare.valid || !finite_sample(signal_millivolts, acceleration_z_g) ||
      !finite_sample(tare.signal_millivolts, tare.acceleration_z_g) ||
      !std::isfinite(orientation_slope_millivolts_per_g)) {
    return std::numeric_limits<double>::quiet_NaN();
  }

  return (signal_millivolts - tare.signal_millivolts) -
         orientation_slope_millivolts_per_g * (acceleration_z_g - tare.acceleration_z_g);
}

WeightResult calculate_weight(double signal_millivolts, double acceleration_z_g,
                              const TarePoint& tare, const MassCalibration& mass_calibration,
                              double orientation_slope_millivolts_per_g) noexcept {
  WeightResult result{};
  result.calibration_status = mass_calibration.status;
  result.corrected_millivolts = orientation_corrected_delta_millivolts(
      signal_millivolts, acceleration_z_g, tare, orientation_slope_millivolts_per_g);

  if (!std::isfinite(result.corrected_millivolts) ||
      !std::isfinite(mass_calibration.grams_per_millivolt) ||
      mass_calibration.status == MassCalibrationStatus::unavailable) {
    return result;
  }

  result.mass_grams = result.corrected_millivolts * mass_calibration.grams_per_millivolt;
  result.valid = std::isfinite(result.mass_grams);
  return result;
}

const char* mass_calibration_status_text(MassCalibrationStatus status) noexcept {
  switch (status) {
    case MassCalibrationStatus::provisional:
      return "provisional";
    case MassCalibrationStatus::verified:
      return "verified";
    case MassCalibrationStatus::unavailable:
    default:
      return "unavailable";
  }
}

StartupTareEstimator::StartupTareEstimator(const StartupTareConfig& config) noexcept
    : config_(config) {
  if (config_.required_samples == 0U) {
    config_.required_samples = 1U;
  }
  if (config_.maximum_signal_span_millivolts < 0.0 ||
      !std::isfinite(config_.maximum_signal_span_millivolts)) {
    config_.maximum_signal_span_millivolts = 0.0;
  }
  if (config_.maximum_acceleration_span_g < 0.0 ||
      !std::isfinite(config_.maximum_acceleration_span_g)) {
    config_.maximum_acceleration_span_g = 0.0;
  }
}

void StartupTareEstimator::reset() noexcept {
  tare_ = TarePoint{};
  count_ = 0U;
  signal_sum_ = 0.0;
  acceleration_sum_ = 0.0;
  signal_min_ = 0.0;
  signal_max_ = 0.0;
  acceleration_min_ = 0.0;
  acceleration_max_ = 0.0;
}

void StartupTareEstimator::begin_window(double signal_millivolts,
                                        double acceleration_z_g) noexcept {
  count_ = 1U;
  signal_sum_ = signal_millivolts;
  acceleration_sum_ = acceleration_z_g;
  signal_min_ = signal_millivolts;
  signal_max_ = signal_millivolts;
  acceleration_min_ = acceleration_z_g;
  acceleration_max_ = acceleration_z_g;
  tare_ = TarePoint{};
}

StartupTareOutcome StartupTareEstimator::add_sample(double signal_millivolts,
                                                    double acceleration_z_g,
                                                    bool explicitly_unloaded,
                                                    bool motion_stable) noexcept {
  if (tare_.valid) {
    return StartupTareOutcome::ready;
  }
  if (!finite_sample(signal_millivolts, acceleration_z_g)) {
    reset();
    return StartupTareOutcome::rejected_invalid_sample;
  }
  if (!explicitly_unloaded) {
    reset();
    return StartupTareOutcome::rejected_not_explicitly_unloaded;
  }
  if (!motion_stable) {
    reset();
    return StartupTareOutcome::rejected_motion;
  }

  if (count_ == 0U) {
    begin_window(signal_millivolts, acceleration_z_g);
  } else {
    const double prospective_signal_min =
        signal_millivolts < signal_min_ ? signal_millivolts : signal_min_;
    const double prospective_signal_max =
        signal_millivolts > signal_max_ ? signal_millivolts : signal_max_;
    const double prospective_acceleration_min =
        acceleration_z_g < acceleration_min_ ? acceleration_z_g : acceleration_min_;
    const double prospective_acceleration_max =
        acceleration_z_g > acceleration_max_ ? acceleration_z_g : acceleration_max_;

    if ((prospective_signal_max - prospective_signal_min) >
            config_.maximum_signal_span_millivolts ||
        (prospective_acceleration_max - prospective_acceleration_min) >
            config_.maximum_acceleration_span_g) {
      begin_window(signal_millivolts, acceleration_z_g);
      return StartupTareOutcome::restarted_unstable_window;
    }

    signal_min_ = prospective_signal_min;
    signal_max_ = prospective_signal_max;
    acceleration_min_ = prospective_acceleration_min;
    acceleration_max_ = prospective_acceleration_max;
    signal_sum_ += signal_millivolts;
    acceleration_sum_ += acceleration_z_g;
    ++count_;
  }

  if (count_ >= config_.required_samples) {
    tare_.signal_millivolts = signal_sum_ / static_cast<double>(count_);
    tare_.acceleration_z_g = acceleration_sum_ / static_cast<double>(count_);
    tare_.valid = true;
    return StartupTareOutcome::ready;
  }
  return StartupTareOutcome::collecting;
}

AutoZeroTracker::AutoZeroTracker(const TarePoint& initial_tare, const AutoZeroConfig& config,
                                 double orientation_slope_millivolts_per_g) noexcept
    : config_(config),
      initial_tare_(initial_tare),
      tare_(initial_tare),
      orientation_slope_(orientation_slope_millivolts_per_g) {
  if (config_.required_consecutive_samples == 0U) {
    config_.required_consecutive_samples = 1U;
  }
  if (!std::isfinite(config_.unloaded_window_millivolts) ||
      config_.unloaded_window_millivolts < 0.0) {
    config_.unloaded_window_millivolts = 0.0;
  }
  config_.correction_gain = clamp_value(config_.correction_gain, 0.0, 1.0);
  if (!std::isfinite(config_.maximum_step_millivolts) || config_.maximum_step_millivolts < 0.0) {
    config_.maximum_step_millivolts = 0.0;
  }
  if (!std::isfinite(config_.maximum_total_adjustment_millivolts) ||
      config_.maximum_total_adjustment_millivolts < 0.0) {
    config_.maximum_total_adjustment_millivolts = 0.0;
  }
  if (!std::isfinite(orientation_slope_)) {
    orientation_slope_ = kOrientationSlopeMillivoltsPerG;
  }
}

void AutoZeroTracker::reset(const TarePoint& initial_tare) noexcept {
  initial_tare_ = initial_tare;
  tare_ = initial_tare;
  clear_candidate();
}

void AutoZeroTracker::clear_candidate() noexcept {
  candidate_count_ = 0U;
  residual_sum_ = 0.0;
}

AutoZeroUpdate AutoZeroTracker::update(double signal_millivolts, double acceleration_z_g,
                                       bool explicitly_unloaded, bool motion_stable) noexcept {
  AutoZeroUpdate result{};
  if (!tare_.valid) {
    clear_candidate();
    result.outcome = AutoZeroOutcome::unavailable_without_tare;
    return result;
  }
  if (!finite_sample(signal_millivolts, acceleration_z_g)) {
    clear_candidate();
    result.outcome = AutoZeroOutcome::rejected_invalid_sample;
    return result;
  }
  if (!explicitly_unloaded) {
    clear_candidate();
    result.outcome = AutoZeroOutcome::rejected_not_explicitly_unloaded;
    return result;
  }
  if (!motion_stable) {
    clear_candidate();
    result.outcome = AutoZeroOutcome::rejected_motion;
    return result;
  }

  const double residual = orientation_corrected_delta_millivolts(
      signal_millivolts, acceleration_z_g, tare_, orientation_slope_);
  if (!std::isfinite(residual) || std::fabs(residual) > config_.unloaded_window_millivolts) {
    clear_candidate();
    result.outcome = AutoZeroOutcome::rejected_outside_window;
    return result;
  }

  residual_sum_ += residual;
  ++candidate_count_;
  result.eligible_sample_count = candidate_count_;
  if (candidate_count_ < config_.required_consecutive_samples) {
    result.outcome = AutoZeroOutcome::collecting;
    return result;
  }

  const double average_residual = residual_sum_ / static_cast<double>(candidate_count_);
  const double requested_step =
      clamp_value(average_residual * config_.correction_gain, -config_.maximum_step_millivolts,
                  config_.maximum_step_millivolts);
  const double current_total = total_adjustment_millivolts();
  const double bounded_total =
      clamp_value(current_total + requested_step, -config_.maximum_total_adjustment_millivolts,
                  config_.maximum_total_adjustment_millivolts);
  const double applied_step = bounded_total - current_total;
  tare_.signal_millivolts += applied_step;

  clear_candidate();
  result.outcome = AutoZeroOutcome::adjustment_applied;
  result.applied_adjustment_millivolts = applied_step;
  result.eligible_sample_count = 0U;
  return result;
}

}  // namespace smart_shovel
