#include "led.hpp"
#include <WiFiNINA.h>
#include <PDM.h>

bool rgb_led::initialized = false;

void rgb_led::init()
{
    if (initialized)
        return;
    pinMode(LEDR, OUTPUT);
    pinMode(LEDG, OUTPUT);
    pinMode(LEDB, OUTPUT);
    initialized = true;
    off();
}

void rgb_led::off()
{
    if (!initialized)
        return;
    set(0,0,0,0);
}

void rgb_led::set(uint8_t r, uint8_t g, uint8_t b, unsigned long ms)
{
    if (!initialized)
        return;

    analogWrite(LEDR, MAX_DUTY_CYCLE - r);
    analogWrite(LEDG, MAX_DUTY_CYCLE - g);
    analogWrite(LEDB, MAX_DUTY_CYCLE - b);
    if (ms > 0)
    {
        delay(ms);
    }
}

void rgb_led::set(uint32_t hex_code, unsigned long ms)
{
    uint8_t r = (hex_code >> 16) & 0xff;
    uint8_t g = (hex_code >> 8) & 0xff;
    uint8_t b = hex_code & 0xff;
    rgb_led::set(r, g, b, ms);
}

void rgb_led::warn(unsigned long ms)
{
    rgb_led::set(0x80, 0x40, 0x00, ms);
}

void rgb_led::error(unsigned long ms)
{
    while (true)
    {
        rgb_led::set(0xff, 0x00, 0x00, ms);
        off();
        delay(ms);
    }
}

void rgb_led::ok(unsigned long ms)
{
    rgb_led::set(0x00, 0xff, 0x00, ms);
}
