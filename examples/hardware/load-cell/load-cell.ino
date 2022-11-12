// Hardware-only diagnostic for the Nano RP2040 Connect load-cell amplifier.
// Keep the A0 input between GND and 3.3 V. This sketch reports nominal voltage;
// it does not apply tare, orientation compensation, or grams calibration.

#include <Arduino.h>

constexpr uint8_t kLoadCellPin = A0;
constexpr uint8_t kAdcBits = 16;
constexpr uint16_t kSamples = 64;
constexpr double kReferenceVolts = 3.3;
constexpr double kAdcScale = 65536.0;

void setup() {
  Serial.begin(115200);
  analogReadResolution(kAdcBits);
}

void loop() {
  uint64_t total = 0;
  for (uint16_t sample = 0; sample < kSamples; ++sample) {
    total += static_cast<uint32_t>(analogRead(kLoadCellPin));
  }

  const double averageCounts = static_cast<double>(total) / kSamples;
  const double nominalVolts = averageCounts * kReferenceVolts / kAdcScale;
  Serial.print("adc_mean=");
  Serial.print(averageCounts, 3);
  Serial.print(",nominal_voltage_v=");
  Serial.println(nominalVolts, 6);
  delay(100);
}
