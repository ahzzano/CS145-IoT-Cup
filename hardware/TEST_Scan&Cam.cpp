#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <SoftwareSerial.h>

#define SCANNER_RX      D5
#define SCANNER_TX      D6

const char* ssid      = "s3wifi";
const char* password  = "Com9L3x!";

const char* esp32IP   = "192.168.60.236";
const int   esp32Port = 8080;
const byte  TRIGGER_CMD[] = {0x7E, 0x00, 0x08, 0x01, 0x00, 0x02, 0x01, 0xAB, 0xCD};

WiFiClient         wifiClient;
ESP8266WebServer   server(80);
SoftwareSerial     scanner(SCANNER_RX, SCANNER_TX);

uint8_t* imageBuffer  = nullptr;
size_t   imageSize    = 0;
bool     triggered    = false;
bool     jobDone      = false;
String   NationalID   = "";

// ── Scan National ID ─────────────────────────────────
// Non-blocking: called repeatedly from loop()
// Returns scanned ID string, or "" if nothing yet
String ScanNationalID() {
    static unsigned long lastTrigger = 0;
    static unsigned long triggerSent = 0;
    static bool          waiting     = false;

    if (!waiting && millis() - lastTrigger > 500) {
        while (scanner.available()) scanner.read(); // flush
        scanner.write(TRIGGER_CMD, sizeof(TRIGGER_CMD));
        triggerSent = millis();
        lastTrigger = millis();
        waiting     = true;
        return "";
    }

    if (waiting && millis() - triggerSent < 300) {
        if (scanner.available()) {
            String data = "";
            unsigned long lastByte = millis();
            while (millis() - lastByte < 150) {
                if (scanner.available()) {
                    char c = scanner.read();
                    if (c >= 0x20 && c <= 0x7E) data += c;
                    lastByte = millis();
                }
            }
            waiting = false;
            if (data.length() > 7) {
                Serial.println("[ID Scanned]: " + data);
                return data;
            }
        }
        return "";
    }

    waiting = false; // timeout, try again next cycle
    return "";
}

bool fetchImage() {
    HTTPClient http;
    String url = "http://" + String(esp32IP) + ":" + String(esp32Port) + "/capture";

    Serial.println("Fetching image from ESP32-CAM...");
    http.begin(wifiClient, url);
    http.setTimeout(10000);

    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        int contentLength = http.getSize();
        Serial.printf("Image size: %d bytes\n", contentLength);

        if (imageBuffer != nullptr) {
            free(imageBuffer);
            imageBuffer = nullptr;
        }

        imageBuffer = (uint8_t*)malloc(contentLength);
        if (!imageBuffer) {
            Serial.println("Memory allocation failed!");
            http.end();
            return false;
        }

        WiFiClient* stream = http.getStreamPtr();
        size_t bytesRead = stream->readBytes(imageBuffer, contentLength);
        imageSize = bytesRead;

        http.end();
        return true;

    } else {
        Serial.printf("Fetch failed, code: %d\n", httpCode);
        Serial.printf("Error: %s\n", http.errorToString(httpCode).c_str());
        http.end();
        return false;
    }
}

void printImageToSerial() {
    if (imageBuffer == nullptr || imageSize == 0) {
        Serial.println("No image in buffer.");
        return;
    }

    Serial.println("===== IMAGE DATA =====");
    Serial.printf("Size:        %d bytes\n", imageSize);
    Serial.printf("JPEG Header: %02X %02X (should be FF D8)\n", imageBuffer[0], imageBuffer[1]);
    Serial.printf("JPEG Footer: %02X %02X (should be FF D9)\n", imageBuffer[imageSize-2], imageBuffer[imageSize-1]);
    Serial.printf("JPEG Valid:  %s\n", (imageBuffer[0] == 0xFF && imageBuffer[1] == 0xD8) ? "YES" : "NO");

    Serial.println("First 64 bytes (hex):");
    for (int i = 0; i < 64 && i < imageSize; i++) {
        Serial.printf("%02X ", imageBuffer[i]);
        if ((i + 1) % 16 == 0) Serial.println();
    }
    Serial.println();
    Serial.println("======================");
}

void setup() {
    Serial.begin(115200);
    scanner.begin(9600);

    IPAddress local_IP(192, 168, 60, 131);
    IPAddress gateway(192, 168, 60, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.config(local_IP, gateway, subnet);

    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected!");
    Serial.print("ESP8266 IP: ");
    Serial.println(WiFi.localIP());

    server.begin();
    Serial.println("Server started.");
    // Serial.println("Send '1' to scan, '0' to stop, 'r' to reset.");
}

String scanKit() {
    // Simulate scanning a test kit (replace with actual scanner code)
    delay(1000);
    String testKitID = "TESTKIT12345";
    Serial.println("Test kit scanned: " + testKitID);
    return testKitID;
}

String secondScanID = "";
bool   scannerActive = false;

String ScanTestKit() {
    static unsigned long lastTrigger = 0;
    static unsigned long triggerSent = 0;
    static bool          waiting     = false;

    // First call — turn scanner ON
    if (!scannerActive) {
        Serial.println("[Second Scanner ON]");
        while (scanner.available()) scanner.read(); // flush
        scanner.write(TRIGGER_CMD, sizeof(TRIGGER_CMD));
        scannerActive = true;
        triggerSent   = millis();
        lastTrigger   = millis();
        waiting       = true;
        return "";
    }

    if (waiting && millis() - triggerSent < 300) {
        if (scanner.available()) {
            String data = "";
            unsigned long lastByte = millis();
            while (millis() - lastByte < 150) {
                if (scanner.available()) {
                    char c = scanner.read();
                    if (c >= 0x20 && c <= 0x7E) data += c;
                    lastByte = millis();
                }
            }
            waiting = false;
            if (data.length() > 7) {
                // Turn scanner OFF
                Serial.println("[Second Scanner OFF]");
                scannerActive = false;
                Serial.println("[Second Scan]: " + data);
                return data;
            }
        }
        return "";
    }

    // Timeout — retrigger
    if (waiting && millis() - triggerSent >= 300) {
        waiting     = false;
        lastTrigger = millis();
        while (scanner.available()) scanner.read();
        scanner.write(TRIGGER_CMD, sizeof(TRIGGER_CMD));
        triggerSent = millis();
        waiting     = true;
    }

    return "";
}

void loop() {
    server.handleClient();  // ← keep WiFi alive

    if (jobDone) return;    // ← locked until reset

    // Step 1 — Listen for scan
    NationalID = ScanNationalID();

    // Step 2 — Only proceed if ID was scanned
    if (NationalID.length() > 7) {
        delay(1000);
        Serial.println("ID found, capturing image...");

        // Step 3 — Fetch image
        bool success = fetchImage();

        // Step 4 — Print to Serial Monitor
        if (success) {
            printImageToSerial();
        } else {
            Serial.println("Image capture failed.");

        }

        // Step 5 — Lock until reset
        jobDone   = true;
        triggered = false;
        Serial.println("[Job done — send 'r' to reset]");
    }

    // Now, we will scan the test_kit
}