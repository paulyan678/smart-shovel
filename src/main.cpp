#include <Arduino.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

#include <hardware/watchdog.h>

#include "boot_session.hpp"
#include "gps_receiver.hpp"
#include "nonblocking_diagnostics.hpp"
#include "sd_logger.hpp"
#include "sensor_reader.hpp"
#include "smart_shovel/config.hpp"
#include "smart_shovel/core.hpp"
#include "status_led.hpp"

namespace smart_shovel {
namespace firmware {
namespace {

namespace config = firmware_config;

struct PendingEvent {
  uint32_t event_uptime_ms{0U};
  uint32_t event_sequence{0U};
  double mass_grams{0.0};
  double corrected_signal_millivolts{0.0};
  uint32_t raw_adc{0U};
  double acceleration_z_g{0.0};
  bool storage_available_at_event{false};
  bool sequence_assigned{false};
  CollectionPipeline lifecycle{};
  GpsSnapshot gps{};
  GpsAssessment gps_assessment{};
  GpsStatus gps_status{GpsStatus::no_fix};
};

template <typename T, std::size_t Capacity>
class FixedQueue {
 public:
  bool push(const T& value) noexcept {
    if (full()) {
      return false;
    }
    entries_[(head_ + count_) % Capacity] = value;
    ++count_;
    return true;
  }

  void pop() noexcept {
    if (empty()) {
      return;
    }
    head_ = (head_ + 1U) % Capacity;
    --count_;
  }

  [[nodiscard]] T* front() noexcept {
    return empty() ? nullptr : &entries_[head_];
  }

  [[nodiscard]] T& at(std::size_t index) noexcept {
    return entries_[(head_ + index) % Capacity];
  }

  [[nodiscard]] bool empty() const noexcept {
    return count_ == 0U;
  }
  [[nodiscard]] bool full() const noexcept {
    return count_ == Capacity;
  }
  [[nodiscard]] std::size_t count() const noexcept {
    return count_;
  }

 private:
  std::array<T, Capacity> entries_{};
  std::size_t head_{0U};
  std::size_t count_{0U};
};

StartupTareConfig startup_tare_config() noexcept {
  return StartupTareConfig{config::kStartupTareSamples, config::kStartupTareMaxSpanMillivolts,
                           config::kStartupTareMaxAccelerationSpanG};
}

AutoZeroConfig auto_zero_config() noexcept {
  const double magnitude = std::fabs(config::kGramsPerMillivolt);
  const double window_millivolts = magnitude > 0.0 ? config::kAutoZeroWindowGrams / magnitude : 0.0;
  return AutoZeroConfig{
      config::kAutoZeroStableSamples,
      window_millivolts,
      config::kAutoZeroCorrectionGain,
      config::kAutoZeroMaxStepMillivolts,
      config::kAutoZeroMaximumTotalAdjustmentMillivolts,
  };
}

EventDetectorConfig event_detector_config() noexcept {
  return EventDetectorConfig{
      config::kEventThresholdGrams, config::kReleaseThresholdGrams, config::kStableWeightSpanGrams,
      config::kEventStableSamples,  config::kReleaseStableSamples,  config::kEventCooldownMs,
  };
}

MassCalibration mass_calibration() noexcept {
  if (!std::isfinite(config::kGramsPerMillivolt) || config::kGramsPerMillivolt == 0.0) {
    return MassCalibration{};
  }
  return MassCalibration{
      config::kGramsPerMillivolt,
      config::kMassCalibrationVerified ? MassCalibrationStatus::verified
                                       : MassCalibrationStatus::provisional,
  };
}

SystemHealth system_health(bool gps_healthy, bool storage_healthy) noexcept {
  if (gps_healthy && storage_healthy) {
    return SystemHealth::healthy;
  }
  if (!gps_healthy && !storage_healthy) {
    return SystemHealth::gps_and_storage_degraded;
  }
  return gps_healthy ? SystemHealth::storage_degraded : SystemHealth::gps_degraded;
}

bool gps_stream_active(const GpsSnapshot& snapshot, uint32_t now_ms) noexcept {
  return snapshot.stream_seen &&
         !interval_elapsed(now_ms, snapshot.last_byte_ms, config::kGpsStreamWarningMs);
}

StatusLed status_led;
NonBlockingDiagnostics diagnostics;
SensorReader sensors;
GpsReceiver gps;
SdLogger storage;
StartupTareEstimator startup_tare{startup_tare_config()};
AutoZeroTracker auto_zero{TarePoint{}, auto_zero_config(), config::kOrientationSlopeMillivoltsPerG};
MovingAverageFilter<config::kWeightFilterSamples> weight_filter;
MovingAverageFilter<config::kWeightFilterSamples> corrected_signal_filter;
MovingAverageFilter<config::kWeightFilterSamples> raw_adc_filter;
MovingAverageFilter<config::kWeightFilterSamples> acceleration_z_filter;
EventDetector event_detector{event_detector_config()};
FixedQueue<PendingEvent, config::kPendingEventCapacity> pending_events;

const AdcCalibration adc_calibration{config::kAdcReferenceMillivolts, 1UL << config::kAdcBits};
const MassCalibration configured_mass_calibration = mass_calibration();
const GpsFreshnessPolicy gps_policy{config::kGpsFreshnessMs, config::kGpsFreshnessMs};

bool tare_ready = false;
bool queue_overflow_latched = false;
uint16_t consecutive_sensor_failures = 0U;
uint16_t consecutive_adc_failures = 0U;
uint32_t dropped_event_count = 0U;
uint32_t last_sample_ms = 0U;
uint32_t last_imu_attempt_ms = 0U;
uint32_t last_storage_attempt_ms = 0U;
uint32_t last_diagnostic_ms = 0U;
uint32_t storage_retry_interval_ms = config::kStorageRetryInitialMs;
char boot_session_id[kBootSessionIdLength + 1U]{};

void print_storage_status(bool success) {
  diagnostics.print(F("storage="));
  diagnostics.print(success ? F("ready") : F("degraded"));
  if (!success) {
    diagnostics.print(F(",reason="));
    diagnostics.print(storage.last_error_text());
  }
  diagnostics.println();
}

void reset_measurement_filters() noexcept {
  weight_filter.reset();
  corrected_signal_filter.reset();
  raw_adc_filter.reset();
  acceleration_z_filter.reset();
}

void invalidate_measurement_continuity(uint32_t now_ms) noexcept {
  if (!tare_ready) {
    startup_tare.reset();
  } else {
    reset_measurement_filters();
  }
  (void)event_detector.update(std::numeric_limits<double>::quiet_NaN(), false, now_ms);
}

void enqueue_event(double filtered_mass_grams, double filtered_corrected_signal_millivolts,
                   uint32_t filtered_raw_adc, double filtered_acceleration_z_g,
                   uint32_t now_ms) noexcept {
  if (!config::kDeviceIdConfigured) {
    ++dropped_event_count;
    diagnostics.println(F("event=dropped,reason=device_id_not_configured"));
    return;
  }
  PendingEvent event{};
  event.lifecycle = CollectionPipeline(CollectionPipelineConfig{
      config::kGpsEventWaitMs, config::kStorageRetryInitialMs, config::kStorageWriteFailureLimit});
  (void)event.lifecycle.begin_event(now_ms);
  event.event_uptime_ms = now_ms;
  event.mass_grams = filtered_mass_grams;
  event.corrected_signal_millivolts = filtered_corrected_signal_millivolts;
  event.raw_adc = filtered_raw_adc;
  event.acceleration_z_g = filtered_acceleration_z_g;
  event.storage_available_at_event = storage.available();

  event.gps = gps.snapshot(now_ms);
  event.gps_assessment = assess_gps(event.gps.fix, now_ms, gps_policy);
  event.gps_status = event.gps_assessment.overall;
  if (event.gps_assessment.overall == GpsStatus::valid) {
    (void)event.lifecycle.provide_gps_result(true, now_ms);
    event.gps_status = GpsStatus::valid;
  }

  if (!pending_events.push(event)) {
    queue_overflow_latched = true;
    ++dropped_event_count;
    diagnostics.println(F("event=dropped,reason=pending_queue_full"));
    return;
  }
  diagnostics.print(F("event=detected,mass_g="));
  diagnostics.print(filtered_mass_grams, 3);
  diagnostics.print(F(",pending="));
  diagnostics.println(static_cast<unsigned long>(pending_events.count()));
}

void process_sensor_sample(uint32_t now_ms) noexcept {
  const SensorSample sample =
      sensors.read(config::kLoadCellPin, config::kAdcSamplesPerReading,
                   config::kAccelerationMagnitudeToleranceG, config::kGyroscopeStableLimitDps);

  if (!sample.acceleration_valid || !sample.gyroscope_valid) {
    if (consecutive_sensor_failures < UINT16_MAX) {
      ++consecutive_sensor_failures;
    }
    if (consecutive_sensor_failures == config::kSensorFailureLimit) {
      diagnostics.println(F("imu=degraded,reason=repeated_read_failure"));
      sensors.mark_imu_unavailable();
      last_imu_attempt_ms = now_ms;
    }
  } else {
    consecutive_sensor_failures = 0U;
  }

  if (!adc_counts_plausible(sample.raw_adc, adc_calibration, config::kAdcSaturationMarginCounts)) {
    if (consecutive_adc_failures < UINT16_MAX) {
      ++consecutive_adc_failures;
    }
    if (consecutive_adc_failures == config::kSensorFailureLimit) {
      diagnostics.println(F("adc=degraded,reason=rail_or_out_of_range"));
    }
    invalidate_measurement_continuity(now_ms);
    return;
  }
  consecutive_adc_failures = 0U;
  if (!sample.acceleration_valid) {
    invalidate_measurement_continuity(now_ms);
    return;
  }

  const double signal_millivolts = adc_counts_to_millivolts(sample.raw_adc, adc_calibration);
  if (!tare_ready) {
    // With no independent unloaded sensor, startup is the one explicit tare
    // protocol: the documented operator precondition is an empty, stable head.
    const StartupTareOutcome outcome = startup_tare.add_sample(
        signal_millivolts, sample.acceleration_z_g, true, sample.motion_stable);
    if (outcome == StartupTareOutcome::ready) {
      tare_ready = true;
      auto_zero.reset(startup_tare.tare());
      reset_measurement_filters();
      event_detector.reset();
      diagnostics.print(F("tare=ready,signal_mv="));
      diagnostics.print(startup_tare.tare().signal_millivolts, 6);
      diagnostics.print(F(",az_g="));
      diagnostics.println(startup_tare.tare().acceleration_z_g, 6);
    }
    return;
  }

  const WeightResult weight =
      calculate_weight(signal_millivolts, sample.acceleration_z_g, auto_zero.tare(),
                       configured_mass_calibration, config::kOrientationSlopeMillivoltsPerG);
  if (!weight.valid) {
    invalidate_measurement_continuity(now_ms);
    return;
  }
  if (!sample.motion_stable) {
    // Do not let vibration-contaminated samples remain in a window later
    // described as stable. Rewarm every aligned evidence filter from stable
    // samples after motion stops.
    invalidate_measurement_continuity(now_ms);
    return;
  }

  const double filtered_mass_grams = weight_filter.add(weight.mass_grams);
  const double filtered_corrected_signal_millivolts =
      corrected_signal_filter.add(weight.corrected_millivolts);
  const double filtered_raw_adc = raw_adc_filter.add(static_cast<double>(sample.raw_adc));
  const double filtered_acceleration_z_g = acceleration_z_filter.add(sample.acceleration_z_g);
  if (!weight_filter.ready()) {
    return;
  }

  const EventDetectorUpdate event =
      event_detector.update(filtered_mass_grams, sample.motion_stable, now_ms);
  if (event.event_emitted) {
    enqueue_event(filtered_mass_grams, filtered_corrected_signal_millivolts,
                  static_cast<uint32_t>(std::lround(filtered_raw_adc)), filtered_acceleration_z_g,
                  now_ms);
  }

  // The weight channel cannot independently prove that a stable small reading
  // is an empty shovel. Keep runtime maintenance fail-closed until an explicit
  // unloaded input or workflow is added; the host-tested tracker rejects every
  // production sample through this gate and cannot absorb a real load.
  (void)auto_zero.update(signal_millivolts, sample.acceleration_z_g, false, sample.motion_stable);
}

void service_pending_gps(uint32_t now_ms) noexcept {
  if (pending_events.empty()) {
    return;
  }
  const GpsSnapshot latest = gps.snapshot(now_ms);

  for (std::size_t index = 0U; index < pending_events.count(); ++index) {
    PendingEvent& event = pending_events.at(index);
    if (event.lifecycle.state() != CollectionPipelineState::pending_gps) {
      continue;
    }
    event.gps.stream_seen = event.gps.stream_seen || latest.stream_seen;
    if (latest.stream_seen) {
      event.gps.last_byte_ms = latest.last_byte_ms;
    }
    event.gps_assessment =
        merge_best_gps_fix(event.gps.fix, event.gps_assessment, latest.fix, now_ms, gps_policy);
    event.gps.satellites_valid = event.gps_assessment.satellites_valid;
    if (event.gps.satellites_valid) {
      event.gps.satellites = event.gps.fix.satellites;
    }
    event.gps_status = event.gps_assessment.overall;
    if (event.gps_assessment.overall == GpsStatus::valid) {
      event.gps_status = GpsStatus::valid;
      (void)event.lifecycle.provide_gps_result(true, now_ms);
    } else {
      const CollectionPipelineUpdate update = event.lifecycle.tick(now_ms);
      if (update.action != CollectionPipelineAction::write_record) {
        continue;
      }
      event.gps_status = event.gps_assessment.overall;
    }
  }
}

CollectionRecord make_record(const PendingEvent& event, uint32_t sequence) noexcept {
  CollectionRecord record{};
  record.identity = RecordIdentity{config::kDeviceId, boot_session_id, sequence};
  record.event_uptime_ms = event.event_uptime_ms;
  record.mass_valid = true;
  record.mass_grams = event.mass_grams;
  record.mass_calibration_status = configured_mass_calibration.status;
  record.corrected_signal_valid = true;
  record.corrected_signal_millivolts = event.corrected_signal_millivolts;
  record.raw_adc_valid = true;
  record.raw_adc = event.raw_adc;
  record.acceleration_z_valid = true;
  record.acceleration_z_g = event.acceleration_z_g;

  const GpsAssessment& assessment = event.gps_assessment;
  record.timestamp_valid = assessment.datetime == GpsDateTimeStatus::valid;
  if (record.timestamp_valid) {
    record.timestamp_utc = event.gps.fix.utc;
  }
  record.location_valid = assessment.location == GpsLocationStatus::valid;
  if (record.location_valid) {
    record.latitude = event.gps.fix.latitude;
    record.longitude = event.gps.fix.longitude;
  }
  if (event.gps.fix.has_location) {
    record.gps_age_valid = true;
    record.gps_age_ms = assessment.location_age_ms;
  }
  record.altitude_valid = record.location_valid && assessment.altitude_valid;
  if (record.altitude_valid) {
    record.altitude_meters = event.gps.fix.altitude_meters;
  }
  record.satellites_valid = assessment.satellites_valid;
  if (record.satellites_valid) {
    record.satellites = event.gps.fix.satellites;
  }
  record.gps_status = event.gps_status;
  record.gps_wait_timed_out = event.lifecycle.gps_timed_out();
  const SystemHealth lifecycle_health = event.lifecycle.health();
  const bool storage_degraded = lifecycle_health == SystemHealth::storage_degraded ||
                                lifecycle_health == SystemHealth::gps_and_storage_degraded;
  record.system_health = system_health(event.gps_status == GpsStatus::valid,
                                       event.storage_available_at_event && !storage_degraded);
  return record;
}

void service_storage(uint32_t now_ms) noexcept {
  if (!config::kDeviceIdConfigured) {
    return;
  }
  if (!storage.available()) {
    if (interval_elapsed(now_ms, last_storage_attempt_ms, storage_retry_interval_ms)) {
      last_storage_attempt_ms = now_ms;
      const bool recovered = storage.begin(config::kSdChipSelectPin, config::kCsvFilename,
                                           config::kDeviceId, boot_session_id);
      if (recovered) {
        storage_retry_interval_ms = config::kStorageRetryInitialMs;
      } else if (storage_retry_interval_ms < config::kStorageRetryMaximumMs) {
        const uint32_t doubled = storage_retry_interval_ms * 2U;
        storage_retry_interval_ms =
            doubled < storage_retry_interval_ms || doubled > config::kStorageRetryMaximumMs
                ? config::kStorageRetryMaximumMs
                : doubled;
      }
      print_storage_status(recovered);
    }
    return;
  }

  PendingEvent* event = pending_events.front();
  if (event == nullptr || event->lifecycle.state() == CollectionPipelineState::pending_gps) {
    return;
  }
  if (event->lifecycle.state() == CollectionPipelineState::waiting_storage_retry) {
    const CollectionPipelineUpdate update = event->lifecycle.tick(now_ms);
    if (update.action != CollectionPipelineAction::write_record) {
      return;
    }
  } else if (event->lifecycle.state() == CollectionPipelineState::degraded_storage) {
    const CollectionPipelineUpdate update = event->lifecycle.retry_degraded_storage();
    if (update.action != CollectionPipelineAction::write_record) {
      return;
    }
  } else if (event->lifecycle.state() != CollectionPipelineState::ready_to_store) {
    return;
  }

  if (!event->sequence_assigned) {
    if (!storage.reserve_sequence(event->event_sequence)) {
      (void)event->lifecycle.provide_storage_result(false, now_ms);
      diagnostics.print(F("storage=degraded,reason="));
      diagnostics.println(storage.last_error_text());
      return;
    }
    event->sequence_assigned = true;
  }

  char buffer[config::kCsvBufferSize]{};
  const CollectionRecord record = make_record(*event, event->event_sequence);
  if (!storage.append(record, buffer, sizeof(buffer))) {
    (void)event->lifecycle.provide_storage_result(false, now_ms);
    diagnostics.print(F("storage=write_failed,event_sequence="));
    diagnostics.print(event->event_sequence);
    diagnostics.print(F(",reason="));
    diagnostics.println(storage.last_error_text());
    return;
  }
  diagnostics.print(F("storage=recorded,event_sequence="));
  diagnostics.println(event->event_sequence);
  (void)event->lifecycle.provide_storage_result(true, now_ms);
  pending_events.pop();
}

LedMode select_led_mode(uint32_t now_ms) noexcept {
  const bool sensor_fault = !sensors.imu_ready() ||
                            consecutive_sensor_failures >= config::kSensorFailureLimit ||
                            consecutive_adc_failures >= config::kSensorFailureLimit;
  if (sensor_fault) {
    return LedMode::sensor_fault;
  }
  if (!tare_ready) {
    return LedMode::calibrating;
  }
  if (!storage.available() || queue_overflow_latched) {
    return LedMode::storage_degraded;
  }
  for (std::size_t index = 0U; index < pending_events.count(); ++index) {
    if (pending_events.at(index).lifecycle.state() == CollectionPipelineState::pending_gps) {
      return LedMode::waiting_for_gps;
    }
  }

  const GpsSnapshot current = gps.snapshot(now_ms);
  const GpsAssessment assessment = assess_gps(current.fix, now_ms, gps_policy);
  if (!gps_stream_active(current, now_ms) || assessment.overall != GpsStatus::valid) {
    return LedMode::gps_degraded;
  }
  return LedMode::ready;
}

void print_diagnostics(uint32_t now_ms) {
  const GpsSnapshot current = gps.snapshot(now_ms);
  const GpsAssessment assessment = assess_gps(current.fix, now_ms, gps_policy);
  diagnostics.print(F("status,tare="));
  diagnostics.print(tare_ready ? F("ready") : F("collecting"));
  diagnostics.print(F(",imu="));
  if (!sensors.imu_ready()) {
    diagnostics.print(F("retrying"));
  } else if (consecutive_sensor_failures > 0U) {
    diagnostics.print(F("read_fault"));
  } else {
    diagnostics.print(F("ready"));
  }
  diagnostics.print(F(",imu_read_failures="));
  diagnostics.print(consecutive_sensor_failures);
  diagnostics.print(F(",adc="));
  diagnostics.print(consecutive_adc_failures >= config::kSensorFailureLimit
                        ? F("rail_or_out_of_range")
                        : F("ready"));
  diagnostics.print(F(",adc_failures="));
  diagnostics.print(consecutive_adc_failures);
  diagnostics.print(F(",gps="));
  diagnostics.print(gps_status_text(assessment.overall));
  diagnostics.print(F(",gps_uart="));
  diagnostics.print(gps_stream_active(current, now_ms) ? F("active") : F("silent"));
  diagnostics.print(F(",sd="));
  if (storage.available()) {
    diagnostics.print(F("ready"));
  } else {
    diagnostics.print(storage.last_error_text());
  }
  diagnostics.print(F(",pending="));
  diagnostics.print(static_cast<unsigned long>(pending_events.count()));
  diagnostics.print(F(",dropped="));
  diagnostics.print(dropped_event_count);
  diagnostics.print(F(",diagnostic_bytes_dropped="));
  diagnostics.println(diagnostics.dropped_byte_count());
}

}  // namespace
}  // namespace firmware
}  // namespace smart_shovel

void setup() {
  using namespace smart_shovel::firmware;
  namespace config = smart_shovel::firmware_config;

  // Enable before the first peripheral or diagnostic call. Pinned Arduino Mbed
  // I2C and USB CDC paths contain waits without application-visible timeouts;
  // the hardware watchdog is the final bounded-recovery backstop.
  const bool recovering_from_watchdog = watchdog_enable_caused_reboot();
  watchdog_enable(config::kWatchdogTimeoutMs, true);
  watchdog_update();

  diagnostics.begin(config::kSerialBaud);
  status_led.begin();
  status_led.set_mode(LedMode::booting);
  status_led.update(millis());

  diagnostics.println(F("smart-shovel firmware starting"));
  if (recovering_from_watchdog) {
    diagnostics.println(F("reset_reason=watchdog_timeout"));
  }
  diagnostics.print(F("device_id="));
  diagnostics.println(config::kDeviceId);
  const bool session_ready = create_boot_session_id(boot_session_id, sizeof(boot_session_id));
  diagnostics.print(F("boot_session_id="));
  diagnostics.println(session_ready ? boot_session_id : "unavailable");
  if (!config::kDeviceIdConfigured) {
    diagnostics.println(F("storage=disabled,reason=device_id_not_configured"));
  }
  diagnostics.print(F("mass_calibration="));
  diagnostics.println(
      smart_shovel::mass_calibration_status_text(configured_mass_calibration.status));
  if (!config::kMassCalibrationVerified) {
    diagnostics.println(F("warning=legacy_grams_factor_requires_hardware_calibration"));
  }
  diagnostics.println(F("autozero=disabled_no_independent_unloaded_signal"));
  diagnostics.poll();

  sensors.begin(config::kAdcBits);
  const bool imu_ready = sensors.begin_imu();
  diagnostics.print(F("adc=ready,imu="));
  diagnostics.println(imu_ready ? F("ready") : F("degraded"));

  gps.begin(config::kGpsBaud);
  diagnostics.println(F("gps_uart=ready"));

  const uint32_t now_ms = millis();
  last_imu_attempt_ms = now_ms;
  last_storage_attempt_ms = now_ms;
  if (config::kDeviceIdConfigured && session_ready) {
    print_storage_status(storage.begin(config::kSdChipSelectPin, config::kCsvFilename,
                                       config::kDeviceId, boot_session_id));
  } else if (config::kDeviceIdConfigured) {
    diagnostics.println(F("storage=disabled,reason=boot_session_unavailable"));
  }
  diagnostics.println(F("tare=waiting_for_empty_stable_shovel"));
  diagnostics.poll();
}

void loop() {
  using namespace smart_shovel;
  using namespace smart_shovel::firmware;
  namespace config = smart_shovel::firmware_config;

  watchdog_update();
  diagnostics.poll();
  const uint32_t now_ms = millis();
  gps.poll(now_ms);

  if (!sensors.imu_ready() && interval_elapsed(now_ms, last_imu_attempt_ms, config::kImuRetryMs)) {
    last_imu_attempt_ms = now_ms;
    diagnostics.print(F("imu_retry="));
    const bool recovered = sensors.begin_imu();
    if (recovered) {
      consecutive_sensor_failures = 0U;
    }
    diagnostics.println(recovered ? F("ready") : F("failed"));
  }

  if (interval_elapsed(now_ms, last_sample_ms, config::kSampleIntervalMs)) {
    last_sample_ms = now_ms;
    process_sensor_sample(now_ms);
  }

  service_pending_gps(now_ms);
  service_storage(now_ms);

  status_led.set_mode(select_led_mode(now_ms));
  status_led.update(now_ms);

  if (interval_elapsed(now_ms, last_diagnostic_ms, config::kDiagnosticIntervalMs)) {
    last_diagnostic_ms = now_ms;
    print_diagnostics(now_ms);
  }
  diagnostics.poll();
}
