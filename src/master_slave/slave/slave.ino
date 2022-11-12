#include<Wire.h>

// set LED pin to be pin 13
LED = 13

void setup() {
  // start the I2C bus as slave (Nano #1, GPS)
  Wire.begin(1);
  // function to be triggered when an event is recieved
  Wire.onReceive(receiveEvent);
}

void loop() {
  // if x is 0, turn on LED for 200ms
  if (x == 0){
    // turn on the LED for 200ms
    digital.write(LED, HIGH);
    delay(200);
    // turn off the LED for 200ms
    digital.write(LED, LOW);
    delay(200);
  }else{
    digital.write(LED, LOW);
    delay(400);
  }
}

void receiveEvent(__decltype(1) numBytes) {
  // when Nano recieves an event, run this function
    printf("test");
}
