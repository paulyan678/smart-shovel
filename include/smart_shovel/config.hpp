#pragma once

#include <Arduino.h>
#include <cstddef>
#include <cstdint>

#if __has_include("smart_shovel/device_config.hpp")
#include "smart_shovel/device_config.hpp"
#endif

#ifndef SMART_SHOVEL_DEVICE_ID
#define SMART_SHOVEL_DEVICE_ID "UNCONFIGURED"
#define SMART_SHOVEL_DEVICE_ID_CONFIGURED 0
#endif

#ifndef SMART_SHOVEL_GRAMS_PER_MILLIVOLT
// Historical value from the 2022 sketch. No tracked known-mass/raw-voltage data
// supports it, so records must identify it as provisional until recalibrated.
#define SMART_SHOVEL_GRAMS_PER_MILLIVOLT -15.0
#endif

#ifndef SMART_SHOVEL_MASS_CALIBRATION_VERIFIED
#define SMART_SHOVEL_MASS_CALIBRATION_VERIFIED 0
#endif

#ifndef SMART_SHOVEL_STATUS_LED_PIN
// D13/LED_BUILTIN is also SPI SCK on this board, while the onboard RGB LED is
// controlled through a coprocessor API with unbounded waits. D2 is the
// reversible fail-safe default for an external LED plus current-limit resistor.
#define SMART_SHOVEL_STATUS_LED_PIN 2
#endif

#ifndef SMART_SHOVEL_STATUS_LED_ACTIVE_HIGH
#define SMART_SHOVEL_STATUS_LED_ACTIVE_HIGH 1
#endif

namespace smart_shovel {
namespace firmware_config {

constexpr char kDeviceId[] = SMART_SHOVEL_DEVICE_ID;
constexpr bool kDeviceIdConfigured = SMART_SHOVEL_DEVICE_ID_CONFIGURED != 0;
static_assert(sizeof(kDeviceId) <= 25, "device id must be at most 24 characters");

constexpr uint8_t kLoadCellPin = A0;
constexpr uint8_t kSdChipSelectPin = 10;
constexpr uint8_t kStatusLedPin = SMART_SHOVEL_STATUS_LED_PIN;
constexpr bool kStatusLedActiveHigh = SMART_SHOVEL_STATUS_LED_ACTIVE_HIGH != 0;
constexpr uint32_t kSerialBaud = 115200;
constexpr uint32_t kGpsBaud = 9600;

constexpr uint8_t kAdcBits = 16;
constexpr double kAdcReferenceMillivolts = 3300.0;
constexpr uint32_t kAdcSaturationMarginCounts = 64U;
// Reproduced from calibration/data/raw/calib1.csv. Units are provisional
// because the 2022 data producer is not tracked.
constexpr double kOrientationSlopeMillivoltsPerG = -74.7089168184;
constexpr double kGramsPerMillivolt = SMART_SHOVEL_GRAMS_PER_MILLIVOLT;
constexpr bool kMassCalibrationVerified = SMART_SHOVEL_MASS_CALIBRATION_VERIFIED != 0;

constexpr uint32_t kSampleIntervalMs = 50;
constexpr uint16_t kAdcSamplesPerReading = 8;
constexpr std::size_t kWeightFilterSamples = 8;
constexpr std::size_t kStartupTareSamples = 32;
constexpr double kStartupTareMaxSpanMillivolts = 12.0;
constexpr double kStartupTareMaxAccelerationSpanG = 0.05;

constexpr double kAccelerationMagnitudeToleranceG = 0.15;
constexpr double kGyroscopeStableLimitDps = 15.0;
constexpr double kEventThresholdGrams = 150.0;
constexpr double kReleaseThresholdGrams = 50.0;
constexpr double kStableWeightSpanGrams = 75.0;
constexpr uint8_t kEventStableSamples = 5;
constexpr uint8_t kReleaseStableSamples = 5;
constexpr uint32_t kEventCooldownMs = 1000;

constexpr double kAutoZeroWindowGrams = 50.0;
constexpr uint16_t kAutoZeroStableSamples = 200;
constexpr double kAutoZeroMaxStepMillivolts = 0.05;
constexpr double kAutoZeroMaximumTotalAdjustmentMillivolts = 25.0;
constexpr double kAutoZeroCorrectionGain = 0.25;

constexpr uint32_t kGpsFreshnessMs = 5000;
constexpr uint32_t kGpsEventWaitMs = 5000;
constexpr uint32_t kGpsStreamWarningMs = 10000;
constexpr uint32_t kImuRetryMs = 5000;
constexpr uint32_t kStorageRetryInitialMs = 30000;
constexpr uint32_t kStorageRetryMaximumMs = 300000;
constexpr std::size_t kStorageWriteFailureLimit = 3U;
constexpr uint32_t kDiagnosticIntervalMs = 5000;
constexpr uint16_t kSensorFailureLimit = 20;
// RP2040 hardware maximum is about 8.3 s. This bounds otherwise non-timeout
// Arduino USB/I2C/SPI dependency calls; a reset can lose RAM-queued events.
constexpr uint32_t kWatchdogTimeoutMs = 8000;

constexpr char kCsvFilename[] = "events.csv";
constexpr std::size_t kCsvBufferSize = 512;
constexpr std::size_t kPendingEventCapacity = 4;

}  // namespace firmware_config
}  // namespace smart_shovel
