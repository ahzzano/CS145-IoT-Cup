#include <Servo.h>

int pos = 0;
int signal = 2; //This is the pin where we will get input from ESP32 for granting access

Servo servo;

void setup()
{
  servo.attach(9, 500, 2500);
  pinMode(signal, INPUT);

  servo.write(pos);
  delay(500);
}

void loop()
{
  bool granted = digitalRead(signal);
  if (granted){
    Serial.print("granted");
    servo.write(90);
    delay(5000);
    servo.write(0);
  }
}

// We will isolate the servo motor control to an Arduino.
// The ESP32 will send a signal to the Arduino when access is granted, and the Arduino will move the servo motor to open the door.

