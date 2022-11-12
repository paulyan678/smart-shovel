#pragma once

// Copy to include/smart_shovel/device_config.hpp for a deployed shovel. Device
// IDs must be stable, unique across the fleet, and contain no commas/newlines.
#define SMART_SHOVEL_DEVICE_ID "SS-001"
#define SMART_SHOVEL_DEVICE_ID_CONFIGURED 1

// Replace this legacy provisional value using the documented known-mass
// calibration procedure, then set the verification flag to 1.
#define SMART_SHOVEL_GRAMS_PER_MILLIVOLT -15.0
#define SMART_SHOVEL_MASS_CALIBRATION_VERIFIED 0

// The fail-safe production default is a single external status LED on D2. If
// the assembled hardware uses another non-conflicting pin or active-low driver,
// record that wiring revision and override both values here.
#define SMART_SHOVEL_STATUS_LED_PIN 2
#define SMART_SHOVEL_STATUS_LED_ACTIVE_HIGH 1
