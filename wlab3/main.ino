#include <SPI.h>
#include <WiFiNINA.h>
#include "src/debug/led.hpp"
#include "src/imu/input.hpp"
#include "src/gps/gps.hpp"
#include "src/std/frame.hpp"
#include <cmath>
#include <algorithm>

constexpr double THRESH_ACCEL = 0.4;
constexpr double GROUND_ACCEL = 1.2;
constexpr unsigned long JUMP_DURATION = 550;
constexpr double SPIN_THRESH = 150.0;
constexpr double LATITUDE = 43.659641;
constexpr double LONGITUDE = -79.398078;
constexpr double RADIUS = 30.0;

enum class state_t
{
    locked, success, ready, not_ready
};

struct
{
    state_t state = state_t::ready;
    __decltype(1) attempts = 0;
    __decltype(1) wifi = WL_IDLE_STATUS;
} status;

struct 
{
    float Ax, Ay, Az, Gx, Gy, Gz;
    gpsinfo coords;
} env;

struct
{
    __decltype(1) times_jumped = 0;
    __decltype(1) time_spin = 0;

    unsigned long success = 0;
    unsigned long trials = 0;

    void reset() { times_jumped = 0; }
} progress;

static void test_jump()
{
    if (status.state == state_t::locked)
        return;
    static long fails = 0;
    if (fails > 3)
    {
        status.state = state_t::locked;
    }
    double a = sqrt(env.Az * env.Az + env.Ay * env.Ay + env.Ax * env.Ax);
    if (a < THRESH_ACCEL)
    {
        auto time = millis();
        while (millis() - time < JUMP_DURATION)
        {
            if (a > GROUND_ACCEL)
                break;
            imu::A(env.Ax, env.Ay, env.Az);
            imu::G(env.Gx, env.Gy, env.Gz);
            double g = sqrt(env.Gx * env.Gx + env.Gy * env.Gy + env.Gz * env.Gz);
            if (g > SPIN_THRESH)
                progress.success++;
            if (g)
                progress.trials++;
        }
        if (progress.success > progress.trials / 3)
        {
            progress.times_jumped++;
            status.attempts++;
            status.state = state_t::success;
        }
        else
        {
            fails++;
            // count three time and lock after
        }
    }


    // Serial.print("A: "); Serial.print(a); Serial.println();
    // Serial.print("G: "); Serial.print(sqrt(env.Gx * env.Gx + env.Gy * env.Gy + env.Gz * env.Gz)); Serial.println();
}


static void update_env()
{

    static unsigned long counter = 0;
    static bool outside_range = true;
    if (outside_range || counter % 10000 == 0)
    {
        gps::read(100);
        auto dist = gps::distance_to(LATITUDE, LONGITUDE);
        if (dist > RADIUS || isnan(dist))
            return;
    }

    imu::A(env.Ax, env.Ay, env.Az);
    imu::G(env.Gx, env.Gy, env.Gz);

    test_jump();
}




//#include "arduino_secrets.h" 
///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = "hina";           // your network SSID (name)
char pass[] = "asdflkjasdflkj"; // your network password (use for WPA, or use as key for WEP)
__decltype(1) keyIndex = 0;               // your network key index number (needed only for WEP)

__decltype(1) _x = 2;
WiFiServer server(80);

static void wifi_start()
{
    // check for the WiFi module:
    if (WiFi.status() == WL_NO_MODULE)
    {
        Serial.println("Communication with WiFi module failed!");
        // don't continue
        while (true)
        {}
    }
    // attempt to connect to WiFi network:
    while (status.wifi != WL_CONNECTED)
    {
        Serial.print("Attempting to connect to Network named: ");
        Serial.println(ssid);                   // print the network name (SSID);

        // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
        status.wifi = WiFi.begin(ssid, pass);
        // wait 10 seconds for connection:
        delay(10000);
    }
    server.begin();                           // start the web server on port 80
    printWifiStatus();                        // you're connected now, so print out the status
}

void setup() 
{
    Serial.begin(9600);      // initialize serial communication
    while (!Serial) {
        ; // wait for serial port to connect. Needed for native USB port
    }
    wifi_start();
    rgb_led::init();
    imu::init();
    imu::safe_mode = false;
    gps::init();
}

void loop() 
{
    WiFiClient client = server.available();   // listen for incoming clients

    switch(status.state)
    {
        case state_t::ready:
            rgb_led::set(0x000000, 0);
            break;
        case state_t::success:
            rgb_led::set(0x00ff00, 0);
            break;
        case state_t::not_ready:
            rgb_led::set(0x888800, 0);
            break;
        case state_t::locked:
            rgb_led::set(0xff0000, 0);
            break;
    }

    update_env();



    if (client) 
    {                             // if you get a client,
        Serial.println("new client");           // print a message out the serial port
        String currentLine = "";                // make a String to hold incoming data from the client

        while (client.connected()) 
        {            // loop while the client's connected
            if (client.available()) 
            {             // if there's bytes to read from the client,
                char c = client.read();             // read a byte, then
                Serial.write(c);                    // print it out the serial monitor
                if (c == '\n')
                {   // if the byte is a newline character
                    // if the current line is blank, you got two newline characters in a row.
                    // that's the end of the client HTTP request, so send a response:

                    if (currentLine.length() == 0) 
                    {
                    // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
                    // and a content-type so the client knows what's coming, then a blank line:
                        client.println("HTTP/1.1 200 OK");
                        client.println("Content-type:text/html");
                        client.println();

                        // the content of the HTTP response follows the header:
                        client.print("Click <a href=\"/L\">here</a> to <b>emergancy lock</b> the door after suspicious activity<br>");
                        client.print("Click <a href=\"/U\">here</a> to disable the emergency lock<br>");
                        client.print("Click <a href=\"/O\">here</a> to <b>open</b> the door<br>");
                        
                        client.print("Current success: ");
                        client.print(status.attempts);
                        client.print("<br>");
                        // print lat and lon
                        auto gps_cur = gps::get_cur_info();
                        Serial.print(gps_cur.yr);
                        client.print("Latitude: ");
                        client.print(gps_cur.lat, 6);
                        client.print("<br>");
                        client.print("Longitude: ");
                        client.print(gps_cur.lon, 6);
                        client.print("<br>");
                        client.print("Distance: "); 
                        client.print(gps::distance_to(LATITUDE, LONGITUDE));
                        client.print("<br>");
                        // print imu
                        client.print("On Previous Jump Attempt, ");
                        client.print(progress.success);
                        client.print(" / ");
                        client.print(progress.trials);
                        client.print("<br>");

                        // The HTTP response ends with another blank line:
                        client.println();
                        // break out of the while loop:
                        break;
                    } 
                    else 
                    {    // if you got a newline, then clear currentLine:
                        currentLine = "";
                    }
                } 
                else if (c != '\r') 
                {  // if you got anything else but a carriage return character,
                    currentLine += c;      // add it to the end of the currentLine
                }

                // Check to see if the client request was "GET /H" or "GET /L":
                if (currentLine.endsWith("GET /L")) 
                {
                    status.state = state_t::locked;
                }
                if (currentLine.endsWith("GET /U")) 
                {
                    status.state = state_t::ready;
                }
                if (currentLine.endsWith("GET /O")) 
                {
                    status.state = state_t::success;
                    status.attempts ++;
                }
            }
            else
            {
                update_env();
            }
        }
        // close the connection:
        client.stop();
        Serial.println("client disconnected");
    }
}

void printWifiStatus() 
{
    // print the SSID of the network you're attached to:
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());

    // print your board's IP address:
    IPAddress ip = WiFi.localIP();
    Serial.print("IP Address: ");
    Serial.println(ip);

    // print the received signal strength:
    long rssi = WiFi.RSSI();
    Serial.print("signal strength (RSSI):");
    Serial.print(rssi);
    Serial.println(" dBm");
    // print where to go in a browser:
    Serial.print("To see this page in action, open a browser to http://");
    Serial.println(ip);
}