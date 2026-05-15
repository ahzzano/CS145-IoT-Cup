#include <Servo.h>

int pos = 0;
int tact_input = 2; //This is the pin where we will get input from the tact switch
int callback   = LOW;

Servo servo;

void setup()
{
  servo.attach(9, 500, 2500);
  pinMode(tact_input, INPUT);

  servo.write(pos);
  delay(500);
}

void loop()
{
  bool pressed = digitalRead(tact_input);
  if (pressed){
    Serial.print("pressed");
    servo.write(90);
    delay(2000);
    servo.write(0);
    digitalWrite(callback, HIGH);
      delay(2000);
  }

  digitalWrite(callback, LOW);
}

