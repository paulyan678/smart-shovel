#include "gps.hpp"
#include <limits>

void gps::init()
{
    Serial1.begin(GPS_BAUD);
}

gpsinfo gps::read(unsigned long ms)
{
    unsigned long start = millis();
    while (millis() - start < ms)
        while (Serial1.available())
            _gps.encode(Serial1.read());
    // if (millis() > 5000 && _gps.charsProcessed() < 10)
    //     Serial.println(F("No GPS data received: check wiring"));
    
    if (_gps.location.isValid())
    {
        cur_info.lat = _gps.location.lat();
        cur_info.lon = _gps.location.lng();
        cur_info.age = _gps.location.age();
    }
    else
    {
        cur_info.lat = std::numeric_limits<double>::quiet_NaN();
        cur_info.lon = std::numeric_limits<double>::quiet_NaN();
        cur_info.age = std::numeric_limits<uint32_t>::max();
    }

    if (_gps.altitude.isValid())
        cur_info.alt = _gps.altitude.meters();
    else 
        cur_info.alt = std::numeric_limits<double>::quiet_NaN();
    
    if (_gps.speed.isValid())
        cur_info.speed = _gps.speed.knots();
    else
        cur_info.speed = std::numeric_limits<double>::quiet_NaN();
    

    if (_gps.date.isValid())
    {
        cur_info.yr = _gps.date.year();
        cur_info.mo = _gps.date.month();
        cur_info.day = _gps.date.day();
    }
    else
    {
        cur_info.yr = std::numeric_limits<uint16_t>::max();
        cur_info.mo = std::numeric_limits<uint8_t>::max();
        cur_info.day = std::numeric_limits<uint8_t>::max();
    }

    if (_gps.time.isValid())
    {
        cur_info.hr = _gps.time.hour();
        cur_info.min = _gps.time.minute();
        cur_info.sec = _gps.time.second();
    }
    else
    {
        cur_info.hr = std::numeric_limits<uint8_t>::max();
        cur_info.min = std::numeric_limits<uint8_t>::max();
        cur_info.sec = std::numeric_limits<uint8_t>::max();
    }

    return cur_info;
}

double gps::distance_to(double lat, double lon)
{
    return TinyGPSPlus::distanceBetween(
        cur_info.lat, cur_info.lon, lat, lon);
}

TinyGPSPlus gps::_gps;
gpsinfo gps::cur_info;
