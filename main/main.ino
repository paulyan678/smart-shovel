#include "src/sd/file.hpp"
#include "src/imu/input.hpp"
#include "src/gps/gps.hpp"
#include "src/debug/led.hpp"
#include <Arduino_LSM6DSOX.h>
#include <iostream>
#include <sstream>

constexpr const char *REC = "rec.csv";
constexpr const char *LOG = "log.csv";

void setup()
{
    rgb_led::init();
    sd::init();
    imu::init();
    analogReadResolution(16);
    gps::init();
    {
        sd_file log_file(LOG, O_WRITE | O_CREAT);
        log_file << "voltage,ax,ay,az,gx,gy,gz\n";
    }
    {
        sd_file rec_file(REC, O_WRITE | O_CREAT);
        rec_file << "time,weight,latitude,longitude,alt\n";
    }
}

// void loop()
// {
//     sd_file log_file(LOG);
//     rgb_led::ok(10L);

//     float ax, ay, az, gx, gy, gz;
//     double voltage = analogRead(A0);
//     IMU.readAcceleration(ax, ay, az);
//     IMU.readGyroscope(gx, gy, gz);
//     log_file << voltage << "," << ax << "," << ay << ","
//         << az << "," << gx << "," << gy << "," << gz << "\n";
    
    
// }

void loop()
{

// {
//     gpsinfo data = gps::read(2000UL);
//     if (data.yr == 2080)
//     {
//         rgb_led::set(0xece259, 200L);
//         rgb_led::warn(100L);
//     }
//     if (isnan(data.lat) || isnan(data.lon))
//         rgb_led::warn(10L);
//     else
//         rgb_led::ok(10L);

//     sd_file rec_file(REC);
//     rec_file << data.yr << "-" << data.mo << "-" << data.day << " "
//         << data.hr << ":" << data.min << ":" << data.sec << ",0,"
//         << data.lat << "," << data.lon << "\n";
// }
{
    double val = 0.;
    for (int i = 0; i < 1000; i++)
        val += analogRead(A0);
    val *= 3300. / (1000*(1L << 16));
    float ax, ay, az, gx, gy, gz;
    IMU.readAcceleration(ax, ay, az);
    IMU.readGyroscope(gx, gy, gz);    

    sd_file log_file(LOG);
    log_file << std::string("\n") << val << "," <<
        ax << "," << ay << "," << az << "," <<
        gx << "," << gy << "," << gz;

    rgb_led::ok(100L);
}

}

// #include <SPI.h>
// #include <SD.h>

// #include <WiFiNINA.h>
// #include "src/debug/led.hpp"
// #include "src/weight/weight.hpp"
// #include "src/gps/gps.hpp"
// #include "src/std/frame.hpp"
// #include <cmath>
// #include <algorithm>

// const __decltype(1) chipSelect = 10;

// struct
// {
//     __decltype(1) attempts = 0;
//     __decltype(1) wifi = WL_IDLE_STATUS;
//     bool sd_only; // ftrue for no wifi, else false
// } status;

// struct
// {
//     bool writeSD;
//     bool empty;
//     bool header;
//     double weight;
// } data;


// //#include "arduino_secrets.h" 
// ///////please enter your sensitive data in the Secret tab/arduino_secrets.h
// char ssid[] = "hina";           // your network SSID (name)
// char pass[] = "asdflkjasdflkj"; // your network password (use for WPA, or use as key for WEP)
// __decltype(1) keyIndex = 0;               // your network key index number (needed only for WEP)

// __decltype(1) _x = 2;
// WiFiServer server(80);

// // write new static void wifi_start() function here later
// 	// needs a condition where if it fails to connect, then make sd true after 5s or so.

// static void wifi_start()
// {
//     // check for the WiFi module:
//     if (WiFi.status() == WL_NO_MODULE)
//     {
//         Serial.println("Communication with WiFi module failed!");
//         // don't continue
//         while (true)
//         {}
//     }
//     // attempt to connect to WiFi network:
//     while (status.wifi != WL_CONNECTED)
//     {
//         Serial.print("Attempting to connect to Network named: ");
//         Serial.println(ssid);                   // print the network name (SSID);

//         // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
//         status.wifi = WiFi.begin(ssid, pass);
//         // wait 10 seconds for connection:
//         delay(10000);
//     }
//     server.begin();                           // start the web server on port 80
//     printWifiStatus();                        // you're connected now, so print out the status
// }

// void printWifiStatus() 
// {
//     // print the SSID of the network you're attached to:
//     Serial.print("SSID: ");
//     Serial.println(WiFi.SSID());

//     // print your board's IP address:
//     IPAddress ip = WiFi.localIP();
//     Serial.print("IP Address: ");
//     Serial.println(ip);

//     // print the received signal strength:
//     long rssi = WiFi.RSSI();
//     Serial.print("signal strength (RSSI):");
//     Serial.print(rssi);
//     Serial.println(" dBm");
//     // print where to go in a browser:
//     Serial.print("To see this page in action, open a browser to http://");
//     Serial.println(ip);
// }

// bool datalogEmpty(){
//     // see if the datalog file on the SD card is empty or not.
//     // open the file for reading:
//     File = SD.open("datalog.txt");
//     if (File) {
//         Serial.println("datalog.txt:");

//         // read from the file until there's nothing else in it:
//         if (File.read() == -1){
//             data.empty == true;
//         }else{
//             data.empty == false;
//         }
//         // WRITE TEST FOR PROPER HEADERLINE FORMAT

//         // close the file:
//         File.close();
//     } else {
//         // if the file didn't open, print an error:
//         Serial.println("error opening datalog.txt");
//     }
// }


// void setup() {
//     // Open serial communications and wait for port to open:
//     Serial.begin(9600);
//     while (!Serial) {
//         ; // wait for serial port to connect. Needed for native USB port only
//     }
// 	// setup wifi
// 	bool wifi = wifi_start();
// 	if (wifi == true){
// 		Serial.println("The chosen data storage method is: WIFI");
// 	}else{
// 		Serial.println("The chosen data storage method is: SD");
// 		// see if the card is present and can be initialized:
//     	if (!SD.begin(chipSelect)) {
//         	Serial.println("Card failed, or not present");
// 			// do nothing more
//         	while (1);
//     	}
//     	Serial.println("card initialized.");
// 	}
// 	// initialize gps and other classes
// 	gps::init();
// 	rgb_led::init();
//     imu::init();
// }

// void loop() {
//     // RUN IF THERE IS NO INTERNET CONNECTION
//     if (status.sd_only == true){
//         while (data.writeSD == true){
//             auto gps_cur = gps::get_cur_info();
//             // open the file. note that only one file can be open at a time,
//             // so you have to close this one before opening another.
//             if (data.empty == true){
//                 data.header = false;
//                 data.empty = false;
//             }

//             File dataFile = SD.open("datalog.txt", FILE_WRITE);
            
//             // if the file is available, write to it:
//             if (dataFile) {
//                 if (data.header == false){
//                     String header = "Date, Time, Latitude, Longitude, Weight\n";
//                     dataFile.println(header);
//                 }
//                 // Write date and time data to dataFile
//                 dataFile.print(gps_cur.yr); dataFile.print("/"); dataFile.print(gps_cur.mo); dataFile.print("/"); dataFile.print(gps_cur.day); dataFile.print(", "); // date
//                 dataFile.print(gps_cur.hr); dataFile.print(":"); dataFile.print(gps_cur.min); dataFile.print("."); dataFile.print(gps_cur.sec); dataFile.print(", "); // time
//                 // write GPS data to dataFile
//                 dataFile.print(gps_cur.lat, 7); dataFile.print(", ") ; // latitude
//                 Serial.print("Latitude: "); Serial.print(gps_cur.lat); Serial.print(", ");
//                 dataFile.print(gps_cur.lon, 7); dataFile.print(", ") ; // longitude
//                 Serial.print("Longitude: "); Serial.print(gps_cur.lon); Serial.print(", ");
//                 // write weight data to dataFile
//                 dataFile.print("WEIGHT"); dataFile.print(", ") ; // weight
//                 Serial.print("Weight: "); Serial.print("WEIGHT"); Serial.print("\n");
//                 // newline and close dataFile
//                 dataFile.print("\n");
//                 dataFile.close();
//             }
//             // if the file isn't open, pop up an error:
//             else {
//                 Serial.println("error opening datalog.txt");
//             }
//         }
//     }
//     // RUN IF THERE IS INTERNET CONNECTION
//     else if(status.sd_only == false){
//         WiFiClient client = server.available();   // listen for incoming clients
//         if (client) { // if you get a client,
//             Serial.println("new client"); // print a message out the serial port
//             String currentLine = ""; // make a String to hold incoming data from the client

//             while (client.connected()) { // loop while the client's connected
//                 // write data backup to sd card
//                 if (data.writeSD == true){
//                     auto gps_cur = gps::get_cur_info();
//                     // open the file. note that only one file can be open at a time,
//                     // so you have to close this one before opening another.
//                     if (data.empty == true){
//                         data.header = false;
//                         data.empty = false;
//                     }

//                     File dataFile = SD.open("datalog.txt", FILE_WRITE);
                    
//                     // if the file is available, write to it:
//                     if (dataFile) {
//                         if (data.header == false){
//                             String header = "Date, Time, Latitude, Longitude, Weight\n";
//                             dataFile.println(header);
//                             data.header = true;
//                         }
//                         // Write date and time data to dataFile
//                         dataFile.print(gps_cur.yr); dataFile.print("/"); dataFile.print(gps_cur.mo); dataFile.print("/"); dataFile.print(gps_cur.day); dataFile.print(", "); // date
//                         dataFile.print(gps_cur.hr); dataFile.print(":"); dataFile.print(gps_cur.min); dataFile.print("."); dataFile.print(gps_cur.sec); dataFile.print(", "); // time
//                         // write GPS data to dataFile
//                         dataFile.print(gps_cur.lat, 7); dataFile.print(", ") ; // latitude
//                         Serial.print("Latitude: "); Serial.print(gps_cur.lat); Serial.print(", ");
//                         dataFile.print(gps_cur.lon, 7); dataFile.print(", ") ; // longitude
//                         Serial.print("Longitude: "); Serial.print(gps_cur.lon); Serial.print(", ");
//                         // write weight data to dataFile
//                         dataFile.print("WEIGHT"); dataFile.print(", ") ; // weight
//                         Serial.print("Weight: "); Serial.print("WEIGHT"); Serial.print("\n");
//                         // newline and close dataFile
//                         dataFile.print("\n");
//                         dataFile.close();
//                     }
//                     // if the file isn't open, pop up an error:
//                     else {
//                         Serial.println("error opening datalog.txt");
//                     }
//                 }
                
//                 if (client.available()) { // if there's bytes to read from the client,
//                     char c = client.read();             // read a byte, then
//                     Serial.write(c);                    // print it out the serial monitor
//                     if (c == '\n') {
//                         // if the byte is a newline character
//                         // if the current line is blank, you got two newline characters in a row.
//                         // that's the end of the client HTTP request, so send a response:

//                         if (currentLine.length() == 0) 
//                         {
//                         // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
//                         // and a content-type so the client knows what's coming, then a blank line:
//                             client.println("HTTP/1.1 200 OK");
//                             client.println("Content-type:text/html");
//                             client.println();

//                             // the content of the HTTP response follows the header:
//                             client.print("Click <a href=\"/L\">here</a> to <b>disable data collection</b> if you don't want data to be stored to the sd card anymore.<br>");
                            
//                             client.print("<br>");
//                             // print lat and lon

//                             // Write date and time data to dataFile
//                             client.print(gps_cur.yr); client.print("/"); client.print(gps_cur.mo); client.print("/"); client.print(gps_cur.day); client.print(", "); // date
//                             client.print(gps_cur.hr); client.print(":"); client.print(gps_cur.min); client.print("."); client.print(gps_cur.sec); client.print(".\n"); // time
//                             // send gps data to client
//                             client.print("Latitude: "); client.print(gps_cur.lat); client.print(", "); // latitude
//                             client.print("Longitude: "); client.print(gps_cur.lon); client.print(", "); // longitude
//                             // write weight data to dataFile
//                             client.print("Weight: "); client.print("WEIGHT"); client.print("<br>");

//                             // The HTTP response ends with another blank line:
//                             client.println();
//                             // break out of the while loop:
//                             break;
//                         } 
//                         else 
//                         {    // if you got a newline, then clear currentLine:
//                             currentLine = "";
//                         }
//                     } 
//                     else if (c != '\r') 
//                     {  // if you got anything else but a carriage return character,
//                         currentLine += c;      // add it to the end of the currentLine
//                     }
//                 }
//                 else
//                 {
//                     update_env();
//                 }
//             }
//             // close the connection:
//             client.stop();
//             Serial.println("client disconnected");
//         }

//     }

// }

