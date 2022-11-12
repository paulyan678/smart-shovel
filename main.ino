#include "src/sd/file.hpp"
#include "src/imu/input.hpp"
#include "src/gps/gps.hpp"
#include "src/debug/led.hpp"
#include "src/std/frame.hpp"
#include <Arduino_LSM6DSOX.h>
#include <iostream>
#include <sstream>

constexpr const char *REC = "rec.csv";
constexpr const char *LOG = "log.csv";

constexpr int CALIBRATION_TRIES = 5000;
constexpr int READING_TRIES = 1000;
constexpr int RESOLUTION = 16;
constexpr int SUCCESS_REQUIRED = 64;
constexpr double READ_TO_MV = 3300. / (1L << RESOLUTION);
constexpr bool GPS_INIT_REQUIRED = true;
constexpr double AZ_ADJUSTMENT = 73.;
constexpr double MV_TO_GRAM = -15.;
constexpr unsigned long GPS_WAIT_TIME = 500UL;

/**
 * Global state of the program
 */
struct 
{
    double zero_voltage = 0;
    int success = 0;
    gpsinfo loc;
} state;

/**
 * Read the voltage from the weight sensor.
 * 
 * @param tries number of tries to read and average over
 * @return the voltage in mV
 */
static double read_weight(int tries)
{
    double val = 0.;
    for (int i = 0; i < tries; ++i)
    {
        float ax, ay, az;
        IMU.readAcceleration(ax, ay, az);
        val += analogRead(A0) + az * AZ_ADJUSTMENT;
    }
    return val * READ_TO_MV / tries;
}

void setup()
{
    rgb_led::init();
    sd::init();
    imu::init();
    analogReadResolution(RESOLUTION);
    gps::init();
    {
        sd_file log_file(LOG, O_WRITE | O_CREAT);
        log_file << "gram,ax,ay,az,gx,gy,gz\n";
    }
    {
        sd_file rec_file(REC, O_WRITE | O_CREAT);
        rec_file << "time,weight,latitude,longitude,alt\n";
    }
    delay(3000UL);

    // perform calibration
    state.zero_voltage = read_weight(CALIBRATION_TRIES);

    delay(2000UL);

    if (GPS_INIT_REQUIRED)
    {
        while (true)
        {
            state.loc = gps::read(2000UL);
            gpsinfo &data = state.loc;
            if (data.yr == 2080)
            {
                rgb_led::set(0xece259, 200L);
                rgb_led::warn(100L);
            }
            if (isnan(data.lat) || isnan(data.lon))
                rgb_led::warn(10L);
            else break;
        }
    }
}

void loop()
{
    rgb_led::ok(10UL);

    gpsinfo &data = state.loc;

    if (true)
    {
        double val = read_weight(READING_TRIES) - state.zero_voltage;
        val *= MV_TO_GRAM;

        float ax, ay, az, gx, gy, gz;
        IMU.readAcceleration(ax, ay, az);
        IMU.readGyroscope(gx, gy, gz);

        sd_file log_file(LOG);
        log_file << "\n" << val << "," <<
            ax << "," << ay << "," << az << "," <<
            gx << "," << gy << "," << gz;
        log_file.close();

        sd_file rec_file(REC);
        rec_file << "\n" << data.yr << "-" << data.mo << "-" << data.day << " " <<
            data.hr << ":" << data.min << ":" << data.sec << "," <<
            val << "," << data.lat << "," << data.lon << "," << data.alt;
        state.success ++;
    }

    if (state.success > SUCCESS_REQUIRED)
    {
        state.success = 0;

        state.loc = gps::read(GPS_WAIT_TIME);

        if (data.yr == 2080)
        {
            rgb_led::set(0xece259, 200L);
            rgb_led::warn(100L);
        }
        if (isnan(data.lat) || isnan(data.lon))
            rgb_led::warn(10L);
        else
            rgb_led::ok(10L);
    }
}
