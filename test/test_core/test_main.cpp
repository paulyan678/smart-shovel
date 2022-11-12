#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <utility>

#include <unity.h>

#include "smart_shovel/core.hpp"

// PlatformIO's native Unity package excludes its optional double helpers by
// default. Keep the production math in double precision and express tolerance
// checks through Unity's always-available boolean assertion instead.
#undef TEST_ASSERT_DOUBLE_WITHIN
#define TEST_ASSERT_DOUBLE_WITHIN(delta, expected, actual) \
  TEST_ASSERT_TRUE(std::fabs((expected) - (actual)) <= (delta))

using namespace smart_shovel;

void setUp() {}
void tearDown() {}

namespace {

constexpr const char kBootSessionA[] = "0123456789abcdef";
constexpr const char kBootSessionB[] = "fedcba9876543210";

template <typename Enum>
int enum_value(Enum value) {
  return static_cast<int>(value);
}

void test_ring_buffer_empty_full_wrap_and_statistics() {
  FixedRingBuffer<int, 3U> values;
  int output = 0;
  double average = 0.0;

  TEST_ASSERT_TRUE(values.empty());
  TEST_ASSERT_FALSE(values.full());
  TEST_ASSERT_EQUAL_UINT(0U, values.count());
  TEST_ASSERT_FALSE(values.get(0U, output));
  TEST_ASSERT_FALSE(values.average(average));
  TEST_ASSERT_FALSE(values.min(output));
  TEST_ASSERT_FALSE(values.max(output));
  TEST_ASSERT_NULL(values.oldest());
  TEST_ASSERT_NULL(values.newest());

  TEST_ASSERT_EQUAL_INT(enum_value(RingPushResult::inserted), enum_value(values.push_back(1)));
  (void)values.push_back(2);
  (void)values.push_back(3);
  TEST_ASSERT_TRUE(values.full());
  TEST_ASSERT_EQUAL_UINT(3U, values.count());
  TEST_ASSERT_EQUAL_INT(1, values[0U]);
  TEST_ASSERT_EQUAL_INT(3, values[2U]);
  TEST_ASSERT_EQUAL_INT(1, *values.oldest());
  TEST_ASSERT_EQUAL_INT(3, *values.newest());
  TEST_ASSERT_TRUE(values.average(average));
  TEST_ASSERT_DOUBLE_WITHIN(1.0e-12, 2.0, average);
  TEST_ASSERT_TRUE(values.min(output));
  TEST_ASSERT_EQUAL_INT(1, output);
  TEST_ASSERT_TRUE(values.max(output));
  TEST_ASSERT_EQUAL_INT(3, output);

  TEST_ASSERT_EQUAL_INT(enum_value(RingPushResult::overwritten_oldest),
                        enum_value(values.push_back(4)));
  TEST_ASSERT_EQUAL_INT(2, values[0U]);
  TEST_ASSERT_EQUAL_INT(3, values[1U]);
  TEST_ASSERT_EQUAL_INT(4, values[2U]);
  TEST_ASSERT_FALSE(values.get(3U, output));
}

void test_ring_buffer_copy_and_move_preserve_value_semantics() {
  FixedRingBuffer<int, 3U> original;
  (void)original.push_back(1);
  (void)original.push_back(2);
  (void)original.push_back(3);
  (void)original.push_back(4);

  FixedRingBuffer<int, 3U> copied(original);
  (void)original.push_back(5);
  TEST_ASSERT_EQUAL_INT(2, copied[0U]);
  TEST_ASSERT_EQUAL_INT(4, copied[2U]);
  TEST_ASSERT_EQUAL_INT(3, original[0U]);

  FixedRingBuffer<int, 3U> copy_assigned;
  copy_assigned = copied;
  TEST_ASSERT_EQUAL_INT(2, copy_assigned[0U]);
  TEST_ASSERT_EQUAL_INT(4, copy_assigned[2U]);

  FixedRingBuffer<int, 3U> moved(std::move(copied));
  TEST_ASSERT_TRUE(copied.empty());
  TEST_ASSERT_EQUAL_UINT(3U, moved.count());
  TEST_ASSERT_EQUAL_INT(2, moved[0U]);
  TEST_ASSERT_EQUAL_INT(4, moved[2U]);

  FixedRingBuffer<int, 3U> move_assigned;
  move_assigned = std::move(moved);
  TEST_ASSERT_TRUE(moved.empty());
  TEST_ASSERT_EQUAL_INT(2, move_assigned[0U]);
  TEST_ASSERT_EQUAL_INT(4, move_assigned[2U]);
}

void test_adc_orientation_and_injectable_mass_calibration() {
  TEST_ASSERT_DOUBLE_WITHIN(1.0e-12, 0.0, adc_counts_to_millivolts(0U));
  TEST_ASSERT_DOUBLE_WITHIN(1.0e-12, 1650.0, adc_counts_to_millivolts(32768U));
  TEST_ASSERT_DOUBLE_WITHIN(1.0e-9, 3299.94964599609375, adc_counts_to_millivolts(65535U));
  TEST_ASSERT_TRUE(std::isnan(adc_counts_to_millivolts(10U, AdcCalibration{3300.0, 0U})));

  const TarePoint tare{100.0, 1.0, true};
  const double changed_orientation_signal = 100.0 + kOrientationSlopeMillivoltsPerG * (0.5 - 1.0);
  TEST_ASSERT_DOUBLE_WITHIN(
      1.0e-9, 0.0, orientation_corrected_delta_millivolts(changed_orientation_signal, 0.5, tare));

  const WeightResult provisional = calculate_weight(changed_orientation_signal - 10.0, 0.5, tare,
                                                    kLegacyProvisionalMassCalibration);
  TEST_ASSERT_TRUE(provisional.valid);
  TEST_ASSERT_DOUBLE_WITHIN(1.0e-9, -10.0, provisional.corrected_millivolts);
  TEST_ASSERT_DOUBLE_WITHIN(1.0e-9, 150.0, provisional.mass_grams);
  TEST_ASSERT_EQUAL_INT(enum_value(MassCalibrationStatus::provisional),
                        enum_value(provisional.calibration_status));
  TEST_ASSERT_EQUAL_STRING("provisional",
                           mass_calibration_status_text(kLegacyProvisionalMassCalibration.status));

  const WeightResult injected =
      calculate_weight(changed_orientation_signal - 10.0, 0.5, tare,
                       MassCalibration{2.5, MassCalibrationStatus::verified});
  TEST_ASSERT_TRUE(injected.valid);
  TEST_ASSERT_DOUBLE_WITHIN(1.0e-9, -25.0, injected.mass_grams);

  const WeightResult unavailable =
      calculate_weight(100.0, 1.0, tare, MassCalibration{1.0, MassCalibrationStatus::unavailable});
  TEST_ASSERT_FALSE(unavailable.valid);

  const AdcCalibration adc{3300.0, 65536U};
  TEST_ASSERT_FALSE(adc_counts_plausible(0U, adc, 64U));
  TEST_ASSERT_FALSE(adc_counts_plausible(63U, adc, 64U));
  TEST_ASSERT_TRUE(adc_counts_plausible(64U, adc, 64U));
  TEST_ASSERT_TRUE(adc_counts_plausible(65471U, adc, 64U));
  TEST_ASSERT_FALSE(adc_counts_plausible(65472U, adc, 64U));
  TEST_ASSERT_FALSE(adc_counts_plausible(65536U, adc, 64U));
  TEST_ASSERT_FALSE(adc_counts_plausible(1U, AdcCalibration{3300.0, 0U}, 0U));
}

void test_startup_tare_requires_explicit_stable_unloaded_window() {
  StartupTareEstimator tare(StartupTareConfig{3U, 1.0, 0.03});

  TEST_ASSERT_EQUAL_INT(enum_value(StartupTareOutcome::rejected_not_explicitly_unloaded),
                        enum_value(tare.add_sample(100.0, 1.0, false, true)));
  TEST_ASSERT_EQUAL_UINT(0U, tare.sample_count());
  TEST_ASSERT_EQUAL_INT(enum_value(StartupTareOutcome::rejected_motion),
                        enum_value(tare.add_sample(100.0, 1.0, true, false)));
  TEST_ASSERT_EQUAL_UINT(0U, tare.sample_count());

  TEST_ASSERT_EQUAL_INT(enum_value(StartupTareOutcome::collecting),
                        enum_value(tare.add_sample(100.0, 1.000, true, true)));
  TEST_ASSERT_EQUAL_INT(enum_value(StartupTareOutcome::collecting),
                        enum_value(tare.add_sample(100.4, 1.010, true, true)));
  TEST_ASSERT_EQUAL_INT(enum_value(StartupTareOutcome::ready),
                        enum_value(tare.add_sample(99.8, 0.995, true, true)));
  TEST_ASSERT_TRUE(tare.ready());
  TEST_ASSERT_DOUBLE_WITHIN(1.0e-9, 100.0666666667, tare.tare().signal_millivolts);
  TEST_ASSERT_DOUBLE_WITHIN(1.0e-9, 1.0016666667, tare.tare().acceleration_z_g);
}

void test_startup_tare_restarts_unstable_window_and_rejects_nan() {
  StartupTareEstimator tare(StartupTareConfig{3U, 0.5, 0.02});
  (void)tare.add_sample(100.0, 1.0, true, true);
  TEST_ASSERT_EQUAL_INT(enum_value(StartupTareOutcome::restarted_unstable_window),
                        enum_value(tare.add_sample(103.0, 1.0, true, true)));
  TEST_ASSERT_EQUAL_UINT(1U, tare.sample_count());
  (void)tare.add_sample(103.1, 1.005, true, true);
  TEST_ASSERT_EQUAL_INT(enum_value(StartupTareOutcome::ready),
                        enum_value(tare.add_sample(102.9, 0.995, true, true)));
  TEST_ASSERT_TRUE(tare.ready());

  tare.reset();
  TEST_ASSERT_EQUAL_INT(
      enum_value(StartupTareOutcome::rejected_invalid_sample),
      enum_value(tare.add_sample(std::numeric_limits<double>::quiet_NaN(), 1.0, true, true)));
  TEST_ASSERT_FALSE(tare.ready());
  TEST_ASSERT_EQUAL_UINT(0U, tare.sample_count());
}

void test_auto_zero_is_gated_and_bounded() {
  const TarePoint initial{100.0, 1.0, true};
  AutoZeroTracker tracker(initial, AutoZeroConfig{2U, 2.0, 1.0, 0.5, 1.0});

  TEST_ASSERT_EQUAL_INT(enum_value(AutoZeroOutcome::rejected_not_explicitly_unloaded),
                        enum_value(tracker.update(101.0, 1.0, false, true).outcome));
  TEST_ASSERT_EQUAL_INT(enum_value(AutoZeroOutcome::rejected_motion),
                        enum_value(tracker.update(101.0, 1.0, true, false).outcome));
  TEST_ASSERT_EQUAL_INT(enum_value(AutoZeroOutcome::rejected_outside_window),
                        enum_value(tracker.update(103.0, 1.0, true, true).outcome));
  TEST_ASSERT_DOUBLE_WITHIN(1.0e-12, 100.0, tracker.tare().signal_millivolts);

  TEST_ASSERT_EQUAL_INT(enum_value(AutoZeroOutcome::collecting),
                        enum_value(tracker.update(101.0, 1.0, true, true).outcome));
  const AutoZeroUpdate first_adjustment = tracker.update(101.0, 1.0, true, true);
  TEST_ASSERT_EQUAL_INT(enum_value(AutoZeroOutcome::adjustment_applied),
                        enum_value(first_adjustment.outcome));
  TEST_ASSERT_DOUBLE_WITHIN(1.0e-12, 0.5, first_adjustment.applied_adjustment_millivolts);
  TEST_ASSERT_DOUBLE_WITHIN(1.0e-12, 100.5, tracker.tare().signal_millivolts);

  (void)tracker.update(101.0, 1.0, true, true);
  (void)tracker.update(101.0, 1.0, true, true);
  TEST_ASSERT_DOUBLE_WITHIN(1.0e-12, 1.0, tracker.total_adjustment_millivolts());

  (void)tracker.update(102.0, 1.0, true, true);
  const AutoZeroUpdate capped = tracker.update(102.0, 1.0, true, true);
  TEST_ASSERT_DOUBLE_WITHIN(1.0e-12, 0.0, capped.applied_adjustment_millivolts);
  TEST_ASSERT_DOUBLE_WITHIN(1.0e-12, 1.0, tracker.total_adjustment_millivolts());
}

void test_auto_zero_orientation_change_cancels_at_tare() {
  const TarePoint initial{100.0, 1.0, true};
  AutoZeroTracker tracker(initial, AutoZeroConfig{1U, 1.0, 1.0, 0.5, 2.0});
  const double signal = 100.0 + kOrientationSlopeMillivoltsPerG * (0.8 - 1.0);
  const AutoZeroUpdate update = tracker.update(signal, 0.8, true, true);
  TEST_ASSERT_EQUAL_INT(enum_value(AutoZeroOutcome::adjustment_applied),
                        enum_value(update.outcome));
  TEST_ASSERT_DOUBLE_WITHIN(1.0e-9, 0.0, update.applied_adjustment_millivolts);
}

void test_moving_average_warms_up_and_wraps() {
  MovingAverageFilter<3U> filter;
  double value = 0.0;
  TEST_ASSERT_FALSE(filter.value(value));
  TEST_ASSERT_DOUBLE_WITHIN(1.0e-12, 1.0, filter.add(1.0));
  TEST_ASSERT_FALSE(filter.ready());
  TEST_ASSERT_DOUBLE_WITHIN(1.0e-12, 2.0, filter.add(3.0));
  TEST_ASSERT_DOUBLE_WITHIN(1.0e-12, 3.0, filter.add(5.0));
  TEST_ASSERT_TRUE(filter.ready());
  TEST_ASSERT_DOUBLE_WITHIN(1.0e-12, 5.0, filter.add(7.0));
  filter.reset();
  TEST_ASSERT_EQUAL_UINT(0U, filter.sample_count());
}

void test_event_detector_requires_stability_hysteresis_release_and_cooldown() {
  EventDetector detector(EventDetectorConfig{100.0, 50.0, 5.0, 3U, 2U, 100U});

  (void)detector.update(0.0, true, 0U);
  TEST_ASSERT_FALSE(detector.update(101.0, true, 1U).sample_was_stable);
  TEST_ASSERT_FALSE(detector.update(102.0, false, 2U).event_emitted);
  TEST_ASSERT_FALSE(detector.update(102.0, true, 3U).event_emitted);
  TEST_ASSERT_FALSE(detector.update(103.0, true, 4U).event_emitted);
  const EventDetectorUpdate emitted = detector.update(102.0, true, 5U);
  TEST_ASSERT_TRUE(emitted.event_emitted);
  TEST_ASSERT_EQUAL_INT(enum_value(EventDetectorState::latched), enum_value(emitted.state));

  TEST_ASSERT_FALSE(detector.update(103.0, true, 6U).event_emitted);
  TEST_ASSERT_FALSE(detector.update(49.0, true, 7U).sample_was_stable);
  TEST_ASSERT_EQUAL_INT(enum_value(EventDetectorState::latched), enum_value(detector.state()));
  (void)detector.update(48.0, true, 8U);
  (void)detector.update(49.0, true, 9U);
  TEST_ASSERT_EQUAL_INT(enum_value(EventDetectorState::cooldown), enum_value(detector.state()));
  (void)detector.update(49.0, true, 108U);
  TEST_ASSERT_EQUAL_INT(enum_value(EventDetectorState::cooldown), enum_value(detector.state()));
  (void)detector.update(49.0, true, 109U);
  TEST_ASSERT_EQUAL_INT(enum_value(EventDetectorState::idle), enum_value(detector.state()));
}

void test_event_detector_cooldown_is_millis_wrap_safe() {
  EventDetector detector(EventDetectorConfig{100.0, 50.0, 1000.0, 1U, 1U, 32U});
  const std::uint32_t event_time = 0xfffffff0U;
  TEST_ASSERT_TRUE(detector.update(120.0, true, event_time).event_emitted);
  (void)detector.update(0.0, true, event_time + 1U);
  TEST_ASSERT_EQUAL_INT(enum_value(EventDetectorState::cooldown), enum_value(detector.state()));
  (void)detector.update(0.0, true, 0x00000005U);
  TEST_ASSERT_EQUAL_INT(enum_value(EventDetectorState::cooldown), enum_value(detector.state()));
  (void)detector.update(0.0, true, 0x00000010U);
  TEST_ASSERT_EQUAL_INT(enum_value(EventDetectorState::cooldown), enum_value(detector.state()));
  (void)detector.update(0.0, true, 0x00000011U);
  TEST_ASSERT_EQUAL_INT(enum_value(EventDetectorState::idle), enum_value(detector.state()));
}

void test_event_detector_rearms_if_new_load_arrives_during_cooldown() {
  EventDetector detector(EventDetectorConfig{100.0, 50.0, 1000.0, 2U, 2U, 100U});
  TEST_ASSERT_FALSE(detector.update(120.0, true, 1U).event_emitted);
  TEST_ASSERT_TRUE(detector.update(120.0, true, 2U).event_emitted);
  (void)detector.update(0.0, true, 3U);
  (void)detector.update(0.0, true, 4U);
  TEST_ASSERT_EQUAL_INT(enum_value(EventDetectorState::cooldown), enum_value(detector.state()));

  (void)detector.update(120.0, true, 50U);
  (void)detector.update(120.0, true, 103U);
  TEST_ASSERT_EQUAL_INT(enum_value(EventDetectorState::cooldown), enum_value(detector.state()));
  (void)detector.update(120.0, true, 104U);
  TEST_ASSERT_EQUAL_INT(enum_value(EventDetectorState::idle), enum_value(detector.state()));
  TEST_ASSERT_FALSE(detector.update(120.0, true, 105U).event_emitted);
  TEST_ASSERT_TRUE(detector.update(120.0, true, 106U).event_emitted);
}

void test_event_detector_invalid_sample_breaks_candidate_continuity() {
  EventDetector detector(EventDetectorConfig{100.0, 50.0, 10.0, 3U, 2U, 100U});
  TEST_ASSERT_FALSE(detector.update(120.0, true, 1U).event_emitted);
  TEST_ASSERT_FALSE(detector.update(120.0, true, 2U).event_emitted);
  TEST_ASSERT_FALSE(
      detector.update(std::numeric_limits<double>::quiet_NaN(), false, 3U).event_emitted);
  TEST_ASSERT_EQUAL_INT(enum_value(EventDetectorState::idle), enum_value(detector.state()));
  TEST_ASSERT_FALSE(detector.update(120.0, true, 4U).event_emitted);
  TEST_ASSERT_FALSE(detector.update(120.0, true, 5U).event_emitted);
  TEST_ASSERT_TRUE(detector.update(120.0, true, 6U).event_emitted);
}

void test_datetime_calendar_validation() {
  TEST_ASSERT_TRUE(is_leap_year(2024U));
  TEST_ASSERT_FALSE(is_leap_year(2100U));
  TEST_ASSERT_TRUE(is_leap_year(2000U));
  TEST_ASSERT_TRUE(valid_utc_datetime(UtcDateTime{2024U, 2U, 29U, 23U, 59U, 59U}));
  TEST_ASSERT_FALSE(valid_utc_datetime(UtcDateTime{2023U, 2U, 29U, 0U, 0U, 0U}));
  TEST_ASSERT_FALSE(valid_utc_datetime(UtcDateTime{2024U, 1U, 1U, 24U, 0U, 0U}));
  TEST_ASSERT_FALSE(valid_utc_datetime(UtcDateTime{0U, 1U, 1U, 0U, 0U, 0U}));
}

GpsFix valid_fix(std::uint32_t updated_ms) {
  GpsFix fix{};
  fix.has_location = true;
  fix.latitude = 5.6037;
  fix.longitude = -0.1870;
  fix.location_updated_ms = updated_ms;
  fix.has_altitude = true;
  fix.altitude_meters = 42.5;
  fix.altitude_updated_ms = updated_ms;
  fix.has_satellites = true;
  fix.satellites = 9U;
  fix.satellites_updated_ms = updated_ms;
  fix.has_date = true;
  fix.has_time = true;
  fix.utc = UtcDateTime{2026U, 7U, 16U, 12U, 34U, 56U};
  fix.datetime_updated_ms = updated_ms;
  return fix;
}

void test_gps_assessment_valid_invalid_stale_and_optional_time() {
  const GpsFreshnessPolicy policy{100U, 100U};
  GpsFix fix = valid_fix(1000U);
  GpsAssessment assessment = assess_gps(fix, 1050U, policy);
  TEST_ASSERT_EQUAL_INT(enum_value(GpsLocationStatus::valid), enum_value(assessment.location));
  TEST_ASSERT_EQUAL_INT(enum_value(GpsDateTimeStatus::valid), enum_value(assessment.datetime));
  TEST_ASSERT_EQUAL_INT(enum_value(GpsStatus::valid), enum_value(assessment.overall));
  TEST_ASSERT_TRUE(assessment.altitude_valid);
  TEST_ASSERT_TRUE(assessment.satellites_valid);
  TEST_ASSERT_EQUAL_UINT32(50U, assessment.location_age_ms);

  fix.altitude_updated_ms = 900U;
  fix.satellites_updated_ms = 900U;
  assessment = assess_gps(fix, 1050U, policy);
  TEST_ASSERT_EQUAL_INT(enum_value(GpsStatus::valid), enum_value(assessment.overall));
  TEST_ASSERT_FALSE(assessment.altitude_valid);
  TEST_ASSERT_FALSE(assessment.satellites_valid);

  fix.latitude = 91.0;
  assessment = assess_gps(fix, 1050U, policy);
  TEST_ASSERT_EQUAL_INT(enum_value(GpsStatus::invalid), enum_value(assessment.overall));

  fix = valid_fix(1000U);
  assessment = assess_gps(fix, 1101U, policy);
  TEST_ASSERT_EQUAL_INT(enum_value(GpsStatus::stale), enum_value(assessment.overall));

  fix = valid_fix(1000U);
  fix.has_time = false;
  assessment = assess_gps(fix, 1050U, policy);
  TEST_ASSERT_EQUAL_INT(enum_value(GpsStatus::location_only), enum_value(assessment.overall));

  fix = valid_fix(1000U);
  fix.utc.month = 13U;
  assessment = assess_gps(fix, 1050U, policy);
  TEST_ASSERT_EQUAL_INT(enum_value(GpsStatus::invalid), enum_value(assessment.overall));

  fix = GpsFix{};
  assessment = assess_gps(fix, 0U, policy);
  TEST_ASSERT_EQUAL_INT(enum_value(GpsStatus::no_fix), enum_value(assessment.overall));
}

void test_gps_freshness_uses_reported_age_and_millis_wrap() {
  const std::uint32_t updated = 0xfffffff5U;
  GpsFix fix = valid_fix(updated);
  GpsAssessment assessment = assess_gps(fix, 0x00000005U, GpsFreshnessPolicy{20U, 20U});
  TEST_ASSERT_EQUAL_INT(enum_value(GpsStatus::valid), enum_value(assessment.overall));

  fix.reported_location_age_ms = 21U;
  assessment = assess_gps(fix, 0x00000005U, GpsFreshnessPolicy{20U, 20U});
  TEST_ASSERT_EQUAL_INT(enum_value(GpsStatus::stale), enum_value(assessment.overall));
  TEST_ASSERT_EQUAL_UINT32(16U, elapsed_ms(0x00000005U, updated));
}

void test_gps_merge_retains_independent_location_and_utc_components() {
  const GpsFreshnessPolicy policy{100U, 100U};
  GpsFix retained{};
  retained.has_location = true;
  retained.latitude = 5.6037;
  retained.longitude = -0.1870;
  retained.location_updated_ms = 1000U;

  GpsFix candidate{};
  candidate.has_date = true;
  candidate.has_time = true;
  candidate.utc = UtcDateTime{2026U, 7U, 16U, 12U, 34U, 56U};
  candidate.datetime_updated_ms = 1020U;
  candidate.has_altitude = true;
  candidate.altitude_meters = 42.5;
  candidate.altitude_updated_ms = 1020U;
  candidate.has_satellites = true;
  candidate.satellites = 9U;
  candidate.satellites_updated_ms = 1020U;

  GpsAssessment merged = assess_gps(retained, 1030U, policy);
  merged = merge_best_gps_fix(retained, merged, candidate, 1030U, policy);
  TEST_ASSERT_EQUAL_INT(enum_value(GpsStatus::valid), enum_value(merged.overall));
  TEST_ASSERT_DOUBLE_WITHIN(1.0e-9, 5.6037, retained.latitude);
  TEST_ASSERT_EQUAL_UINT16(2026U, retained.utc.year);
  TEST_ASSERT_TRUE(merged.altitude_valid);
  TEST_ASSERT_TRUE(merged.satellites_valid);

  GpsFix worse = valid_fix(800U);
  worse.latitude = 91.0;
  merged = merge_best_gps_fix(retained, merged, worse, 1030U, policy);
  TEST_ASSERT_EQUAL_INT(enum_value(GpsStatus::valid), enum_value(merged.overall));
  TEST_ASSERT_DOUBLE_WITHIN(1.0e-9, 5.6037, retained.latitude);
  TEST_ASSERT_EQUAL_UINT16(2026U, retained.utc.year);
}

void test_gps_merge_freezes_event_valid_evidence_during_bounded_wait() {
  const GpsFreshnessPolicy policy{5000U, 5000U};
  GpsFix retained{};
  retained.has_location = true;
  retained.latitude = 5.6037;
  retained.longitude = -0.1870;
  retained.location_updated_ms = 4000U;
  GpsAssessment merged = assess_gps(retained, 5000U, policy);
  TEST_ASSERT_EQUAL_INT(enum_value(GpsStatus::location_only), enum_value(merged.overall));
  TEST_ASSERT_EQUAL_UINT32(1000U, merged.location_age_ms);

  GpsFix candidate{};
  candidate.has_date = true;
  candidate.has_time = true;
  candidate.utc = UtcDateTime{2026U, 7U, 16U, 12U, 34U, 56U};
  candidate.datetime_updated_ms = 9500U;
  merged = merge_best_gps_fix(retained, merged, candidate, 9500U, policy);

  TEST_ASSERT_EQUAL_INT(enum_value(GpsStatus::valid), enum_value(merged.overall));
  TEST_ASSERT_EQUAL_INT(enum_value(GpsLocationStatus::valid), enum_value(merged.location));
  TEST_ASSERT_EQUAL_UINT32(1000U, merged.location_age_ms);
  TEST_ASSERT_DOUBLE_WITHIN(1.0e-9, 5.6037, retained.latitude);
}

void test_csv_header_and_complete_record_are_deterministic() {
  char header[256]{};
  const CsvFormatResult header_result = format_csv_header(header, sizeof(header));
  TEST_ASSERT_TRUE(header_result.success);
  TEST_ASSERT_EQUAL_STRING(
      "schema_version,device_id,boot_session_id,event_sequence,event_uptime_ms,timestamp_utc,"
      "mass_g,mass_calibration_status,corrected_signal_mv,raw_adc,accel_z_g,"
      "latitude,longitude,altitude_m,gps_age_ms,satellites,gps_status,"
      "gps_wait_timed_out,system_health\n",
      header);
  TEST_ASSERT_EQUAL_STRING(csv_header(), header);
  TEST_ASSERT_EQUAL_UINT(std::strlen(header), header_result.length);

  CollectionRecord record{};
  record.identity = RecordIdentity{"shovel-01", kBootSessionA, 42U};
  record.event_uptime_ms = 123456U;
  record.timestamp_valid = true;
  record.timestamp_utc = UtcDateTime{2026U, 7U, 16U, 12U, 34U, 56U};
  record.mass_valid = true;
  record.mass_grams = 123.4567;
  record.mass_calibration_status = MassCalibrationStatus::provisional;
  record.corrected_signal_valid = true;
  record.corrected_signal_millivolts = -8.5;
  record.raw_adc_valid = true;
  record.raw_adc = 54321U;
  record.acceleration_z_valid = true;
  record.acceleration_z_g = 0.998;
  record.location_valid = true;
  record.latitude = 5.6037;
  record.longitude = -0.187;
  record.altitude_valid = true;
  record.altitude_meters = 42.5;
  record.gps_age_valid = true;
  record.gps_age_ms = 250U;
  record.satellites_valid = true;
  record.satellites = 9U;
  record.gps_status = GpsStatus::valid;
  record.gps_wait_timed_out = false;
  record.system_health = SystemHealth::healthy;

  char output[256]{};
  const CsvFormatResult result = format_csv_record(record, output, sizeof(output));
  TEST_ASSERT_TRUE(result.success);
  TEST_ASSERT_EQUAL_STRING(
      "2,shovel-01,0123456789abcdef,42,123456,2026-07-16T12:34:56Z,123.457,provisional,"
      "-8.500,54321,0.998000,5.6037000,-0.1870000,42.50,250,9,valid,"
      "false,healthy\n",
      output);
  TEST_ASSERT_EQUAL_UINT(std::strlen(output), result.length);
}

void test_csv_uses_empty_optional_fields_and_rejects_invalid_output() {
  CollectionRecord record{};
  record.identity = RecordIdentity{"dev", kBootSessionA, 7U};
  record.event_uptime_ms = 123U;
  record.mass_grams = std::numeric_limits<double>::quiet_NaN();
  record.corrected_signal_millivolts = std::numeric_limits<double>::quiet_NaN();
  record.acceleration_z_g = std::numeric_limits<double>::quiet_NaN();
  record.latitude = std::numeric_limits<double>::quiet_NaN();
  record.longitude = std::numeric_limits<double>::quiet_NaN();
  record.altitude_meters = std::numeric_limits<double>::quiet_NaN();
  record.gps_status = GpsStatus::timeout;
  record.gps_wait_timed_out = true;
  record.system_health = SystemHealth::gps_degraded;
  char output[128]{};
  CsvFormatResult result = format_csv_record(record, output, sizeof(output));
  TEST_ASSERT_TRUE(result.success);
  TEST_ASSERT_EQUAL_STRING(
      "2,dev,0123456789abcdef,7,123,,,unavailable,,,,,,,,,timeout,true,gps_degraded\n", output);

  char too_small[8] = "dirty";
  result = format_csv_record(record, too_small, sizeof(too_small));
  TEST_ASSERT_FALSE(result.success);
  TEST_ASSERT_EQUAL_CHAR('\0', too_small[0]);

  record.identity.device_id = "bad,id";
  std::strcpy(output, "dirty");
  result = format_csv_record(record, output, sizeof(output));
  TEST_ASSERT_FALSE(result.success);
  TEST_ASSERT_EQUAL_CHAR('\0', output[0]);

  record.identity.device_id = "dev";
  record.identity.boot_session_id = "invalid-session!";
  std::strcpy(output, "dirty");
  result = format_csv_record(record, output, sizeof(output));
  TEST_ASSERT_FALSE(result.success);
  TEST_ASSERT_EQUAL_CHAR('\0', output[0]);

  record.identity.boot_session_id = kBootSessionA;
  record.timestamp_valid = true;
  record.timestamp_utc = UtcDateTime{2026U, 2U, 30U, 0U, 0U, 0U};
  result = format_csv_record(record, output, sizeof(output));
  TEST_ASSERT_FALSE(result.success);

  record.timestamp_valid = false;
  record.schema_version = 0U;
  result = format_csv_record(record, output, sizeof(output));
  TEST_ASSERT_FALSE(result.success);
}

void test_record_identity_uses_device_boot_session_and_sequence() {
  TEST_ASSERT_TRUE(valid_device_id("123456789012345678901234"));
  TEST_ASSERT_FALSE(valid_device_id("1234567890123456789012345"));
  TEST_ASSERT_TRUE(same_identity(RecordIdentity{"dev-a", kBootSessionA, 12U},
                                 RecordIdentity{"dev-a", kBootSessionA, 12U}));
  TEST_ASSERT_FALSE(same_identity(RecordIdentity{"dev-a", kBootSessionA, 12U},
                                  RecordIdentity{"dev-a", kBootSessionA, 13U}));
  TEST_ASSERT_FALSE(same_identity(RecordIdentity{"dev-a", kBootSessionA, 12U},
                                  RecordIdentity{"dev-b", kBootSessionA, 12U}));
  TEST_ASSERT_FALSE(same_identity(RecordIdentity{"dev-a", kBootSessionA, 12U},
                                  RecordIdentity{"dev-a", kBootSessionB, 12U}));
  TEST_ASSERT_FALSE(same_identity(RecordIdentity{"dev-a", "not-a-session-id", 12U},
                                  RecordIdentity{"dev-a", "not-a-session-id", 12U}));
  TEST_ASSERT_FALSE(valid_device_id("bad,id"));
  TEST_ASSERT_TRUE(valid_device_id("accra_shovel-01.v2"));
  TEST_ASSERT_TRUE(valid_boot_session_id(kBootSessionA));
  TEST_ASSERT_TRUE(valid_boot_session_id("ABCDEF0123456789"));
  TEST_ASSERT_FALSE(valid_boot_session_id(nullptr));
  TEST_ASSERT_FALSE(valid_boot_session_id("0123456789abcde"));
  TEST_ASSERT_FALSE(valid_boot_session_id("0123456789abcdef0"));
  TEST_ASSERT_FALSE(valid_boot_session_id("0123456789abcdeg"));
}

void test_sequence_scanner_handles_chunks_malformed_and_partial_tail() {
  SequenceScanner scanner("shovel-a", kBootSessionA);
  const char* first_chunk =
      "schema_version,device_id,boot_session_id,event_sequence,rest\r\n"
      "2,shovel-a,0123456789abcdef,1,x\n"
      "2,other,0123456789abcdef,99,x\n"
      "2,shovel-a,fedcba9876543210,77,x\n"
      "2,shovel-a,0123456789abcdef,nope,x\n"
      "1,shovel-a,0123456789abcdef,5,x\n"
      "2,shovel-a,01234567";
  const char* second_chunk = "89abcdef,7,x\n2,shovel-a,0123456789abcdef,8,partial";
  scanner.feed(first_chunk, std::strlen(first_chunk));
  scanner.feed(second_chunk, std::strlen(second_chunk));
  scanner.finish();

  std::uint32_t next = 0U;
  TEST_ASSERT_TRUE(scanner.next_sequence(next));
  TEST_ASSERT_EQUAL_UINT32(9U, next);
  TEST_ASSERT_EQUAL_UINT(4U, scanner.valid_record_count());
  TEST_ASSERT_EQUAL_UINT(2U, scanner.malformed_record_count());
  TEST_ASSERT_TRUE(scanner.trailing_partial_record());
}

void test_sequence_scanner_initial_sequence_invalid_target_and_overflow() {
  SequenceScanner empty("device-1", kBootSessionA, 10U);
  std::uint32_t next = 0U;
  TEST_ASSERT_TRUE(empty.next_sequence(next));
  TEST_ASSERT_EQUAL_UINT32(10U, next);

  SequenceScanner invalid_device("bad,id", kBootSessionA);
  TEST_ASSERT_FALSE(invalid_device.next_sequence(next));

  SequenceScanner invalid_session("device-1", "invalid-session!");
  TEST_ASSERT_FALSE(invalid_session.next_sequence(next));

  SequenceScanner overflow("device-1", kBootSessionA);
  const char* line = "2,device-1,0123456789abcdef,4294967295,x\n";
  overflow.feed(line, std::strlen(line));
  TEST_ASSERT_FALSE(overflow.next_sequence(next));

  SequenceScanner malformed("device-1", kBootSessionA);
  const char* bad = "2,device-1,0123456789abcdef,4294967296,x\n";
  malformed.feed(bad, std::strlen(bad));
  TEST_ASSERT_EQUAL_UINT(1U, malformed.malformed_record_count());
}

void test_sequence_scanner_partial_identity_reservation_rules() {
  std::uint32_t next = 0U;

  SequenceScanner partial_header("device-1", kBootSessionA);
  const char* header = "schema_version,device_";
  partial_header.feed(header, std::strlen(header));
  partial_header.finish();
  TEST_ASSERT_TRUE(partial_header.trailing_partial_record());
  TEST_ASSERT_TRUE(partial_header.next_sequence(next));
  TEST_ASSERT_EQUAL_UINT32(1U, next);

  SequenceScanner partial_id("device-1", kBootSessionA);
  const char* identity_prefix = "2,device-1";
  partial_id.feed(identity_prefix, std::strlen(identity_prefix));
  partial_id.finish();
  TEST_ASSERT_TRUE(partial_id.next_sequence(next));
  TEST_ASSERT_EQUAL_UINT32(1U, next);

  SequenceScanner partial_session("device-1", kBootSessionA);
  const char* session_prefix = "2,device-1,01234567";
  partial_session.feed(session_prefix, std::strlen(session_prefix));
  partial_session.finish();
  TEST_ASSERT_TRUE(partial_session.next_sequence(next));
  TEST_ASSERT_EQUAL_UINT32(1U, next);

  SequenceScanner complete_identity("device-1", kBootSessionA);
  const char* torn_record = "2,device-1,0123456789abcdef,12,partial";
  complete_identity.feed(torn_record, std::strlen(torn_record));
  complete_identity.finish();
  TEST_ASSERT_TRUE(complete_identity.trailing_partial_record());
  TEST_ASSERT_EQUAL_UINT(0U, complete_identity.valid_record_count());
  TEST_ASSERT_TRUE(complete_identity.next_sequence(next));
  TEST_ASSERT_EQUAL_UINT32(13U, next);

  SequenceScanner different_session("device-1", kBootSessionA);
  const char* other_session = "2,device-1,fedcba9876543210,99,partial";
  different_session.feed(other_session, std::strlen(other_session));
  different_session.finish();
  TEST_ASSERT_TRUE(different_session.next_sequence(next));
  TEST_ASSERT_EQUAL_UINT32(1U, next);

  SequenceScanner maximum_identity("device-1", kBootSessionA);
  const char* maximum = "2,device-1,0123456789abcdef,4294967295";
  maximum_identity.feed(maximum, std::strlen(maximum));
  maximum_identity.finish();
  TEST_ASSERT_FALSE(maximum_identity.next_sequence(next));
}

void test_sequence_scanner_reserves_sequence_cut_exactly_after_digits() {
  std::uint32_t next = 0U;

  SequenceScanner partial("device-1", kBootSessionA);
  const char* cut_after_digits = "2,device-1,0123456789abcdef,12";
  partial.feed(cut_after_digits, std::strlen(cut_after_digits));
  partial.finish();
  TEST_ASSERT_TRUE(partial.trailing_partial_record());
  TEST_ASSERT_EQUAL_UINT(0U, partial.malformed_record_count());
  TEST_ASSERT_TRUE(partial.next_sequence(next));
  TEST_ASSERT_EQUAL_UINT32(13U, next);

  SequenceScanner recovered_on_next_boot("device-1", kBootSessionA);
  const char* terminated_identity = "2,device-1,0123456789abcdef,12\n";
  recovered_on_next_boot.feed(terminated_identity, std::strlen(terminated_identity));
  recovered_on_next_boot.finish();
  TEST_ASSERT_FALSE(recovered_on_next_boot.trailing_partial_record());
  TEST_ASSERT_EQUAL_UINT(1U, recovered_on_next_boot.malformed_record_count());
  TEST_ASSERT_TRUE(recovered_on_next_boot.next_sequence(next));
  TEST_ASSERT_EQUAL_UINT32(13U, next);
}

void test_pipeline_completes_event_with_fresh_gps_and_storage() {
  CollectionPipeline pipeline(CollectionPipelineConfig{100U, 50U, 2U});
  CollectionPipelineUpdate update = pipeline.begin_event(10U);
  TEST_ASSERT_EQUAL_INT(enum_value(CollectionPipelineState::pending_gps), enum_value(update.state));
  TEST_ASSERT_EQUAL_INT(enum_value(CollectionPipelineAction::request_gps),
                        enum_value(update.action));
  TEST_ASSERT_TRUE(pipeline.has_pending_event());

  update = pipeline.provide_gps_result(false, 15U);
  TEST_ASSERT_EQUAL_INT(enum_value(CollectionPipelineAction::none), enum_value(update.action));
  update = pipeline.provide_gps_result(true, 20U);
  TEST_ASSERT_EQUAL_INT(enum_value(CollectionPipelineAction::write_record),
                        enum_value(update.action));
  TEST_ASSERT_TRUE(pipeline.pending_record_has_gps());

  update = pipeline.provide_storage_result(true, 21U);
  TEST_ASSERT_EQUAL_INT(enum_value(CollectionPipelineAction::event_completed),
                        enum_value(update.action));
  TEST_ASSERT_EQUAL_INT(enum_value(CollectionPipelineState::ready), enum_value(pipeline.state()));
  TEST_ASSERT_EQUAL_INT(enum_value(SystemHealth::healthy), enum_value(pipeline.health()));
  TEST_ASSERT_FALSE(pipeline.has_pending_event());
}

void test_pipeline_timeout_retry_and_degraded_recovery_are_bounded() {
  CollectionPipeline pipeline(CollectionPipelineConfig{100U, 50U, 2U});
  const std::uint32_t start = 0xffffffceU;
  (void)pipeline.begin_event(start);
  CollectionPipelineUpdate update = pipeline.tick(20U);
  TEST_ASSERT_EQUAL_INT(enum_value(CollectionPipelineAction::none), enum_value(update.action));
  update = pipeline.tick(50U);
  TEST_ASSERT_EQUAL_INT(enum_value(CollectionPipelineAction::write_record),
                        enum_value(update.action));
  TEST_ASSERT_TRUE(pipeline.gps_timed_out());
  TEST_ASSERT_FALSE(pipeline.pending_record_has_gps());
  TEST_ASSERT_EQUAL_INT(enum_value(SystemHealth::gps_degraded), enum_value(pipeline.health()));

  update = pipeline.provide_storage_result(false, 50U);
  TEST_ASSERT_EQUAL_INT(enum_value(CollectionPipelineState::waiting_storage_retry),
                        enum_value(update.state));
  TEST_ASSERT_EQUAL_UINT(1U, pipeline.storage_failure_count());
  TEST_ASSERT_EQUAL_INT(enum_value(CollectionPipelineAction::none),
                        enum_value(pipeline.tick(99U).action));
  TEST_ASSERT_EQUAL_INT(enum_value(CollectionPipelineAction::write_record),
                        enum_value(pipeline.tick(100U).action));

  update = pipeline.provide_storage_result(false, 100U);
  TEST_ASSERT_EQUAL_INT(enum_value(CollectionPipelineAction::entered_storage_degraded),
                        enum_value(update.action));
  TEST_ASSERT_EQUAL_INT(enum_value(CollectionPipelineState::degraded_storage),
                        enum_value(pipeline.state()));
  TEST_ASSERT_EQUAL_INT(enum_value(SystemHealth::gps_and_storage_degraded),
                        enum_value(pipeline.health()));
  TEST_ASSERT_TRUE(pipeline.has_pending_event());

  update = pipeline.retry_degraded_storage();
  TEST_ASSERT_EQUAL_INT(enum_value(CollectionPipelineAction::write_record),
                        enum_value(update.action));
  update = pipeline.provide_storage_result(true, 101U);
  TEST_ASSERT_EQUAL_INT(enum_value(CollectionPipelineAction::event_completed),
                        enum_value(update.action));
  TEST_ASSERT_EQUAL_INT(enum_value(SystemHealth::gps_degraded), enum_value(pipeline.health()));
  pipeline.note_gps_recovered();
  TEST_ASSERT_EQUAL_INT(enum_value(SystemHealth::healthy), enum_value(pipeline.health()));
}

void run_all_tests() {
  RUN_TEST(test_ring_buffer_empty_full_wrap_and_statistics);
  RUN_TEST(test_ring_buffer_copy_and_move_preserve_value_semantics);
  RUN_TEST(test_adc_orientation_and_injectable_mass_calibration);
  RUN_TEST(test_startup_tare_requires_explicit_stable_unloaded_window);
  RUN_TEST(test_startup_tare_restarts_unstable_window_and_rejects_nan);
  RUN_TEST(test_auto_zero_is_gated_and_bounded);
  RUN_TEST(test_auto_zero_orientation_change_cancels_at_tare);
  RUN_TEST(test_moving_average_warms_up_and_wraps);
  RUN_TEST(test_event_detector_requires_stability_hysteresis_release_and_cooldown);
  RUN_TEST(test_event_detector_cooldown_is_millis_wrap_safe);
  RUN_TEST(test_event_detector_rearms_if_new_load_arrives_during_cooldown);
  RUN_TEST(test_event_detector_invalid_sample_breaks_candidate_continuity);
  RUN_TEST(test_datetime_calendar_validation);
  RUN_TEST(test_gps_assessment_valid_invalid_stale_and_optional_time);
  RUN_TEST(test_gps_freshness_uses_reported_age_and_millis_wrap);
  RUN_TEST(test_gps_merge_retains_independent_location_and_utc_components);
  RUN_TEST(test_gps_merge_freezes_event_valid_evidence_during_bounded_wait);
  RUN_TEST(test_csv_header_and_complete_record_are_deterministic);
  RUN_TEST(test_csv_uses_empty_optional_fields_and_rejects_invalid_output);
  RUN_TEST(test_record_identity_uses_device_boot_session_and_sequence);
  RUN_TEST(test_sequence_scanner_handles_chunks_malformed_and_partial_tail);
  RUN_TEST(test_sequence_scanner_initial_sequence_invalid_target_and_overflow);
  RUN_TEST(test_sequence_scanner_partial_identity_reservation_rules);
  RUN_TEST(test_sequence_scanner_reserves_sequence_cut_exactly_after_digits);
  RUN_TEST(test_pipeline_completes_event_with_fresh_gps_and_storage);
  RUN_TEST(test_pipeline_timeout_retry_and_degraded_recovery_are_bounded);
}

}  // namespace

#ifdef ARDUINO
void setup() {
  UNITY_BEGIN();
  run_all_tests();
  UNITY_END();
}

void loop() {}
#else
int main() {
  UNITY_BEGIN();
  run_all_tests();
  return UNITY_END();
}
#endif
