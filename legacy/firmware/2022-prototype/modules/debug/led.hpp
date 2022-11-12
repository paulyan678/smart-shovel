#ifndef DEBUG_LED_HPP
#define DEBUG_LED_HPP
#include <cstdint>

namespace errorcode
{
    constexpr long GENERAL = 2500000L;
    constexpr long IMU = 500L;
    constexpr long GPS = 1000L;
    constexpr long SD_INIT = 1500L;
    constexpr long SD_OPEN = 250L; 
    constexpr long SD_FAIL = 3000L;
}

class rgb_led
{
    static bool initialized;

public:
    static constexpr uint8_t MAX_DUTY_CYCLE = 255;
    /**
     * Initialize the RGB LEDs
     */
    static void init();

    /**
     * Turn the LED off. 
     */
    static void off();

    /**
     * Set the LED to yellow to communicate a warning. 
     */
    static void warn(unsigned long ms = 0);

    /**
     * Set the LED to red to communicate an error. 
     */
    static void error(unsigned long ms = 0);

    /**
     * Set the LED to green to communicate an OK. 
     */
    static void ok(unsigned long ms = 0);

    /**
     * Set the LED to the given hex color code. 
     * @param r The red value.
     * @param g The green value.
     * @param b The blue value. 
     * @param ms The delay in milliseconds. 
     */
    static void set(uint8_t r, uint8_t g, uint8_t b, unsigned long ms = 0);
    
    /**
     * Set the LED to the given hex color. 
     * @param hex_code The hex color code. 
     * @param ms The delay in milliseconds. 
     */
    static void set(uint32_t hex_code, unsigned long ms = 0);

    rgb_led() = delete;
    rgb_led(const rgb_led&) = delete;
};

#endif