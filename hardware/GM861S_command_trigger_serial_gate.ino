#include <SoftwareSerial.h>

// GM861S Pin 5 (TXD) -> D5, GM861S Pin 4 (RXD) -> D6
#define SCANNER_RX D5
#define SCANNER_TX D6

SoftwareSerial scanner(SCANNER_RX, SCANNER_TX);

// GM861S Command Triggered Mode scan command
const byte TRIGGER_CMD[] = {0x7E, 0x00, 0x08, 0x01, 0x00, 0x02, 0x01, 0xAB, 0xCD};

bool triggered = false;

void setup() {
  Serial.begin(115200);
  scanner.begin(9600);
  delay(1000);
  Serial.println("\n--- GM861S QR Scanner Ready (Serial Command Trigger) ---");
  Serial.println("Send '1' to trigger scan, '0' to stop.");
}

void loop() {
  if (Serial.available()) {
    char cmd = Serial.read();
    if (cmd == '1') {
      if (!triggered) {
        triggered = true;
        scanner.write(TRIGGER_CMD, sizeof(TRIGGER_CMD));
        Serial.println("[Scan triggered]");
      }
    } else if (cmd == '0') {
      if (triggered) {
        triggered = false;
        // Discard any leftover data
        while (scanner.available()) scanner.read();
        Serial.println("[Scan stopped]");
      }
    }
  }

  if (triggered) {
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
