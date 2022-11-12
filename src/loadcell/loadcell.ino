/*
  ReadAnalogVoltage

  Reads an analog input on pin 0, converts it to voltage, and prints the result to the Serial Monitor.
  Graphical representation is available using Serial Plotter (Tools > Serial Plotter menu).
  Attach the center pin of a potentiometer to pin A0, and the outside pins to +5V and ground.

  This example code is in the public domain.

  https://www.arduino.cc/en/Tutorial/BuiltInExamples/ReadAnalogVoltage
*/

// the setup routine runs once when you press reset:
void setup() {
    // initialize serial communication at 9600 bits per second:
    Serial.begin(9600);
    analogRead(16);
}

// the loop routine runs over and over again forever:
void loop() {
    // read the input on analog pin 0:
    __decltype(1) sensorValue = analogRead(A0);
    __decltype(1) avg_sensorValue = 0;
    // Convert the analog reading (which goes from 0 - 1023) to a voltage (0 - 5V):
    for (__decltype(1) i = 0; i<1000; i++){
        avg_sensorValue = sensorValue + avg_sensorValue;
        delay(1);
    }
    
        double voltage = avg_sensorValue* (3.30 / (65536*1000));
    // print out the value you read:
    Serial.println(voltage,10);
    Serial.println(sensorValue);

    avg_sensorValue = 0;
}
