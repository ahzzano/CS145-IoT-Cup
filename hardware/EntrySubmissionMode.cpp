#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <SoftwareSerial.h>

#define SCANNER_RX      D5
#define SCANNER_TX      D6
#define MODE_ENTRY      D2

const char* ssid      = "s3wifi";
const char* password  = "Com9L3x!";

const char* esp32IP   = "192.168.60.236";
const int   esp32Port = 8080;

const char* serverIP  = "192.168.60.63";
const int   serverPort = 8000;

const byte  TRIGGER_CMD[] = {0x7E, 0x00, 0x08, 0x01, 0x00, 0x02, 0x01, 0xAB, 0xCD};

WiFiClient         wifiClient;
ESP8266WebServer   server(80);
SoftwareSerial     scanner(SCANNER_RX, SCANNER_TX);

uint8_t* imageBuffer  = nullptr;
size_t   imageSize    = 0;
String   NationalID   = "";
int      mode         = 0;

enum EntryState {
    IDLE,
    SCANNING_ID,
    CAPTURING_IMAGE,
    SENDING_QR,       // ← new step
    SENDING_IMAGE,    // ← new step
    DONE
};

EntryState entryState = IDLE;

// ── Send QR / National ID ─────────────────────────────
bool sendQRData() {
    HTTPClient http;
    String url = "http://" + String(serverIP) + ":" + String(serverPort) + "/mosip/auth/enrolled";

    Serial.println("[POST] Sending QR data to " + url);
    http.begin(wifiClient, url);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String payload = "qr_data=" + NationalID;
    int httpCode = http.POST(payload);

    Serial.printf("[POST QR] Response code: %d\n", httpCode);
    if (httpCode > 0) Serial.println("[POST QR] Response: " + http.getString());

    http.end();
    return (httpCode == HTTP_CODE_OK || httpCode == 201);
}

// ── Send Image ────────────────────────────────────────
bool sendImageData() {
    if (imageBuffer == nullptr || imageSize == 0) {
        Serial.println("[POST Image] No image in buffer.");
        return false;
    }

    HTTPClient http;
    String url = "http://" + String(serverIP) + ":" + String(serverPort) + "/examinee/timein";

    Serial.println("[POST] Sending image to " + url);
    http.begin(wifiClient, url);
    http.addHeader("Content-Type", "image/jpeg");
    // http.addHeader("X-National-ID", NationalID);   // attach ID as a header for reference

    int httpCode = http.POST(imageBuffer, imageSize);

    Serial.printf("[POST Image] Response code: %d\n", httpCode);
    if (httpCode > 0) Serial.println("[POST Image] Response: " + http.getString());

    http.end();
    return (httpCode == HTTP_CODE_OK || httpCode == 201);
}

// ── Scan National ID ──────────────────────────────────
String ScanNationalID() {
    static unsigned long lastTrigger = 0;
    static unsigned long triggerSent = 0;
    static bool          waiting     = false;

    if (!waiting && millis() - lastTrigger > 500) {
        while (scanner.available()) scanner.read();
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

    waiting = false;
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

        if (imageBuffer != nullptr) { free(imageBuffer); imageBuffer = nullptr; }

        imageBuffer = (uint8_t*)malloc(contentLength);
        if (!imageBuffer) {
            Serial.println("Memory allocation failed!");
            http.end();
            return false;
        }

        WiFiClient* stream = http.getStreamPtr();
        imageSize = stream->readBytes(imageBuffer, contentLength);

        http.end();
        return true;
    }

    Serial.printf("Fetch failed, code: %d\n", httpCode);
    http.end();
    return false;
}

void printImageToSerial() {
    if (imageBuffer == nullptr || imageSize == 0) { Serial.println("No image in buffer."); return; }
    Serial.println("===== IMAGE DATA =====");
    Serial.printf("Size:        %d bytes\n", imageSize);
    Serial.printf("JPEG Header: %02X %02X (should be FF D8)\n", imageBuffer[0], imageBuffer[1]);
    Serial.printf("JPEG Footer: %02X %02X (should be FF D9)\n", imageBuffer[imageSize-2], imageBuffer[imageSize-1]);
    Serial.printf("JPEG Valid:  %s\n", (imageBuffer[0] == 0xFF && imageBuffer[1] == 0xD8) ? "YES" : "NO");
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
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
    Serial.println("\nWiFi connected! IP: " + WiFi.localIP().toString());

    server.begin();
    Serial.println("Server started.");
    pinMode(MODE_ENTRY, INPUT);
}

String secondScanID = "";
bool   scannerActive = false;

String ScanTestKit() {
    static unsigned long lastTrigger = 0;
    static unsigned long triggerSent = 0;
    static bool          waiting     = false;

    if (!scannerActive) {
        Serial.println("[Second Scanner ON]");
        while (scanner.available()) scanner.read();
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
                Serial.println("[Second Scanner OFF]");
                scannerActive = false;
                Serial.println("[Second Scan]: " + data);
                return data;
            }
        }
        return "";
    }

    if (waiting && millis() - triggerSent >= 300) {
        waiting = false;
        while (scanner.available()) scanner.read();
        scanner.write(TRIGGER_CMD, sizeof(TRIGGER_CMD));
        triggerSent = millis();
        waiting     = true;
    }

    return "";
}

void loop() {
    server.handleClient();

    mode = digitalRead(MODE_ENTRY);

    if (mode == LOW) {
        switch (entryState) {

            case IDLE:
                delay(500);
                Serial.println("[Entry] Waiting for ID scan...");
                entryState = SCANNING_ID;
                break;

            case SCANNING_ID: {
                String id = ScanNationalID();
                if (id.length() > 7) {
                    NationalID = id;
                    Serial.println("[Entry] ID captured: " + NationalID);
                    delay(1000);
                    entryState = SENDING_QR;
                }
                break;
            }

            case CAPTURING_IMAGE: {
                Serial.println("[Entry] Capturing image...");
                delay(5000);
                bool success = fetchImage();
                if (success) {
                    printImageToSerial();
                    entryState = SENDING_IMAGE;        // ← proceed to send
                } else {
                    Serial.println("[Entry] Image capture failed. Retrying...");
                    // stays in CAPTURING_IMAGE to retry next loop
                }
                break;
            }

            case SENDING_QR: {
                Serial.println("[Entry] Sending QR data...");
                bool ok = sendQRData();
                Serial.println(ok ? "[Entry] QR sent OK." : "[Entry] QR send FAILED.");
                entryState = CAPTURING_IMAGE;         // ← proceed regardless, or gate on ok
                break;
            }

            case SENDING_IMAGE: {
                Serial.println("[Entry] Sending image...");
                bool ok = sendImageData();
                Serial.println(ok ? "[Entry] Image sent OK." : "[Entry] Image send FAILED.");
                entryState = DONE;
                break;
            }

            case DONE:
                // Locked — flip mode pin or power-cycle to reset
                break;
        }

    } else {
        if (entryState != IDLE) {
            Serial.println("[Reset] Exit mode — resetting entry state.");
            entryState = IDLE;
            NationalID  = "";
        }

        Serial.println("Exit mode: scanning test kit...");
        delay(5000);
    }
}