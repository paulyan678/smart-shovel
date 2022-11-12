#include "input.hpp"
#include <PDM.h>
#include <Arduino_LSM6DSOX.h>
#include "../debug/led.hpp"
#include <thread>
#include <limits>

void imu::init()
{
    if (initialized)
        return;
    if (!IMU.begin())
    {
        rgb_led::error(errorcode::IMU);
        return;
    }
    A_sample_rate = IMU.accelerationSampleRate();
    G_sample_rate = IMU.gyroscopeSampleRate();
    initialized = true;  
}


void imu::A(float &ax, float &ay, float &az)
{
    if (!initialized)
        return;
    if (IMU.accelerationAvailable())
    {
        IMU.readAcceleration(ax, ay, az);
    }
    else if (safe_mode)
    {
        rgb_led::warn(5000ul);
    }
}

void imu::G(float &gx, float &gy, float &gz)
{
    if (!initialized)
        return;
    if (IMU.gyroscopeAvailable())
    {
        IMU.readGyroscope(gx, gy, gz);
    }
    else if (safe_mode)
    {
        rgb_led::warn(5000ul);
    }
}

void imu::enable()
{
    if (!initialized)
        return;
    if (enabled)
        return;
    enabled = true;
}

void imu::disable()
{
    enabled = false;
}

void imu::worker()
{
    while (enabled)
    {
        if (IMU.accelerationAvailable())
        {
            IMU.readAcceleration(_Ax, _Ay, _Az);
        }
        if (IMU.gyroscopeAvailable())
        {
            IMU.readGyroscope(_Gx, _Gy, _Gz);
        }
    }
}

float imu::_Ax = std::numeric_limits<float>::quiet_NaN();
float imu::_Ay = std::numeric_limits<float>::quiet_NaN();
float imu::_Az = std::numeric_limits<float>::quiet_NaN();
float imu::_Gx = std::numeric_limits<float>::quiet_NaN();
float imu::_Gy = std::numeric_limits<float>::quiet_NaN();
float imu::_Gz = std::numeric_limits<float>::quiet_NaN();
float imu::A_sample_rate = std::numeric_limits<float>::quiet_NaN();
float imu::G_sample_rate = std::numeric_limits<float>::quiet_NaN();
bool imu::enabled = false;
bool imu::initialized = false;
bool imu::safe_mode = true;
