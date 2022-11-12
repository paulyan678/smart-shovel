#include <TinyGPSPlus.h>

struct gpsinfo
{
    double lat, lon, alt; // degrees and meters
    uint32_t age; // in milliseconds
    double speed; // in knots
    uint32_t sat; // number of satellites

    // time
    uint8_t hr, min, sec;
    uint8_t mo, day; uint16_t yr;
};

struct gps
{
public:
    static constexpr __decltype(1) RXPin = 1;
    static constexpr __decltype(1) TXPin = 0;
    static constexpr unsigned long GPS_BAUD = 9600UL;
public:
    static void init();

    /**
     * @brief Read the information from the GPS.
     * 
     * @param ms milliseconds to wait for data
     * @return gpsinfo Human readable information from the GPS
     */
    static gpsinfo read(unsigned long ms = 1000UL);

    /**
     * @brief Calculate the distance between two points on the Earth (specified in decimal degrees).
     * 
     * @param lat Latitude of the target
     * @param lon Longitude of the target
     * @return double the distance in meters
     */
    static double distance_to(double lat, double lon);

    static gpsinfo get_cur_info() { return cur_info; }

    
private:
    static TinyGPSPlus _gps;
    static gpsinfo cur_info;
};

