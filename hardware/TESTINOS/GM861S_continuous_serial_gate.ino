#include <SoftwareSerial.h>

// GM861S Pin 5 (TXD) -> D5, GM861S Pin 4 (RXD) -> D6
#define SCANNER_RX D5
#define SCANNER_TX D6

SoftwareSerial scanner(SCANNER_RX, SCANNER_TX);

bool reading = false;

void setup() {
  Serial.begin(115200);
  scanner.begin(9600);
  delay(1000);
  Serial.println("\n--- GM861S QR Scanner Ready (Serial Button Trigger) ---");
  Serial.println("Send '1' to start reading, '0' to stop.");
}

void loop() {
  if (Serial.available()) {
    char cmd = Serial.read();
    if (cmd == '1') {
      if (!reading) {
        reading = true;
        // Flush any stale data in the buffer
        while (scanner.available()) scanner.read();
        Serial.println("[Reading ON]");
      }
    } else if (cmd == '0') {
      if (reading) {
        reading = false;
        // Discard any leftover data
        while (scanner.available()) scanner.read();
        Serial.println("[Reading OFF]");
      }
    }
  }

  if (reading) {
    if (scanner.available()) {
      String data = "";
      unsigned long lastByte = millis();
      while (millis() - lastByte < 150) {
        if (scanner.available()) {
          char c = scanner.read();
          if (c >= 0x20 && c <= 0x7E) {
            data += c;
          }
          lastByte = millis();
        }
      }
      if (data.length() > 0) {
        Serial.println(data);
      }
    }
  }
}
