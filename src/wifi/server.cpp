#include "server.hpp"
#include "../debug/led.hpp"

constexpr const uint16_t DEFAULT_PORT = 80;

void server::init()
{
    if (initialized)
        return;
    
    if (WiFi.status() == WL_NO_MODULE)
    {
        rgb_led::error(3600000ul);
        return;
    }

    while (status != WL_CONNECTED)
    {
    }
}

__decltype(1) server::status = WL_IDLE_STATUS;