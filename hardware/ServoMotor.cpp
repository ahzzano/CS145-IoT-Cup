#include <Servo.h>

int pos        = 0;
int signal     = 2; // CONNCT TO D0. This is input from the 8266 to tell us when to open the door.
int callback   = 3; // This pin will tell the 8266 when the door is closed.
int mode       = 4; // 0 -> dispenser, 1 -> submission mode

Servo dispenserServo;
Servo submissionServo;

void setup()
{
  dispenserServo.attach(9,  500, 2500);
  submissionServo.attach(11, 500, 2500);
  pinMode(signal,   INPUT);
  pinMode(mode,     INPUT);
  pinMode(callback, OUTPUT);

  dispenserServo.write(pos);
  submissionServo.write(pos);
  delay(500);
}

void loop()
{
  bool open    = digitalRead(signal);
  bool mode    = digitalRead(mode);
  if (open) {
    if (mode == 0) {
      Serial.print("Dispensing Exam...");
      dispenserServo.write(90);
      delay(2000);
      dispenserServo.write(0);
    } else {
      Serial.print("Accepting Submission...");
      submissionServo.write(90);
      delay(2000);
      submissionServo.write(0);
    }
    digitalWrite(callback, HIGH);
    delay(2000);
    digitalWrite(callback, LOW);
  }
  delay(500);
}

