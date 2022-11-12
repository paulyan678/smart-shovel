#include<Wire.h>

// __decltype(1) bytesGPS = 

void setup() {
  // start the I2C bus as master
  Wire.begin();
  //set up recieve event for GPS data
  Wire.onReceive(gpsData);
}

void loop() {
  // transmit to device 1 (Nano #1, GPS)
  Wire.beginTransmission(1);
  Wire.write(x);
  Wire.endTransmission(1);
  // set delay to 500ms
  delay(500);
  if (x == 0){
    x = 1;
  }else{
    x = 0;
  }
}

void gpsData(__decltype(1) bytesGPS){
  // If internet is unavailable, write to ssd
  printf("test");
  // If internet is available, write to the online database
}
