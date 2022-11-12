#ifndef WIFI_SERVER_HPP
#define WIFI_SERVER_HPP
#include <WiFiNINA.h>

class server
{
    static bool initialized;
    static WiFiServer _server;
    static __decltype(1) status;
public:
    static void init();


};

#endif
