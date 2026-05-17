#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <SoftwareSerial.h>

#define SCANNER_RX      D5
#define SCANNER_TX      D6
#define MODE_ENTRY      D2
#define ARDUINO_SIGNAL  D0
#define FROM_ARDUINO    D1
#define REDLED          D7
#define BLUELED         D8

// Wifi Credentials
// const char* ssid      = "s3wifi";
// const char* password  = "Com9L3x!";
const char* ssid = "HG8145V5_F59B8";
const char* password = "4UGxc887";

// const char* esp32IP   = "192.168.60.236"; //This is the IP address when connected to S3 WiFi
const char* esp32IP = "192.168.254.164";
const int   esp32Port = 8080;

// Server Endpoint/Communciation Channel with Backend
const char* serverIP  = "13.214.144.32";
const int   serverPort = 8000;

// Command trigger for GM861S
const byte  TRIGGER_CMD[] = {0x7E, 0x00, 0x08, 0x01, 0x00, 0x02, 0x01, 0xAB, 0xCD};

WiFiClient         wifiClient;
ESP8266WebServer   server(80);
SoftwareSerial     scanner(SCANNER_RX, SCANNER_TX);

// This is to store the captured image from ESP32-CAM
uint8_t* imageBuffer  = nullptr;
size_t   imageSize    = 0;
String   NationalID   = "";

// Determines whether entry or submission mode. 
// We start in entry mode by default (placeholder).
// Can be overridden by reading the MODE_ENTRY pin on startup.
int      mode         = 0;

// Exam Kit Numbers
// Hardcoded exam kits, do not put in documentation
const char* examKitNumbers[] = {"KIT001", "KIT002", "KIT003"};
int        currentKitIndex   = -1;

// Conditional flag to track if we're in submission mode.
bool submissionMode = false;

// State machine for entry/submission process
enum EntryState {
    IDLE,
    SCANNING_ID,
    CAPTURING_IMAGE,
    SENDING_QR,       
    SENDING_IMAGE,    
    SCANNING_KIT,
    SENDING_KIT,     
    WAITING_ARD,     // For submission mode, we wait for Arduino confirmation after sending data
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

// ── Scan Exam Kit ──────────────────────────────────
// Do not include in documentation.
bool scanExamKit() {
    // For demo purposes, we'll just simulate a successful scan after a delay
    delay(3000);
    Serial.println("[Kit Scanned]: SUCCESS");
    currentKitIndex++;
    return true;
}

// This is the real scan exam kit function.
// Exactly the same as ScanNationID
String ScanExamKit() {
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

// ── Send Kit Data ──────────────────────────────────
bool sendKitData() {
    HTTPClient http;
    String url = "http://" + String(serverIP) + ":" + String(serverPort) + "/examinee/kit"; // CHANGE THIS LATER!

    Serial.println("[POST] Sending kit data to " + url);
    http.begin(wifiClient, url);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String payload = "kit_number=" + String(examKitNumbers[currentKitIndex]);
    int httpCode = http.POST(payload);
    http.end();
    return (httpCode == HTTP_CODE_OK || httpCode == 201);
}

// ── Fetch Image from ESP32-CAM ───────────────────────
// This function sends an HTTP GET request to the ESP32-CAM to capture an image.
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

// ── Debug Function to Print Image Data ───────────────────────
// This was used to check what kind of data we were handling.
// Very useful for the backend team to understand what we're sending, and also for us to debug the image capture process.
void printImageToSerial() {
    if (imageBuffer == nullptr || imageSize == 0) { Serial.println("No image in buffer."); return; }
    Serial.println("===== IMAGE DATA =====");
    Serial.printf("Size:        %d bytes\n", imageSize);
    Serial.printf("JPEG Header: %02X %02X (should be FF D8)\n", imageBuffer[0], imageBuffer[1]);
    Serial.printf("JPEG Footer: %02X %02X (should be FF D9)\n", imageBuffer[imageSize-2], imageBuffer[imageSize-1]);
    Serial.printf("JPEG Valid:  %s\n", (imageBuffer[0] == 0xFF && imageBuffer[1] == 0xD8) ? "YES" : "NO");
    Serial.println("======================");
}

// This is the ready state where the LED is ready to scan the ID or QR code.
void ReadytoScanLED() {
    digitalWrite(REDLED,   HIGH);
}

// Function to indicate facial recognition in progress by lighting up the blue LED.
void FacialRecognitionLED() {
    digitalWrite(REDLED,   LOW);
    digitalWrite(BLUELED,  HIGH);
}

// Function to indicate dispensing action by turning off both LEDs.
// Something to do with voltage allocation
void dispensingLED() {
    digitalWrite(REDLED,  LOW);
    digitalWrite(BLUELED, LOW);
}


void setup() {
    Serial.begin(115200);
    scanner.begin(9600);

    // Static IP configuration (optional, can be removed if using DHCP)
    IPAddress local_IP(192, 168, 60, 131);
    IPAddress gateway(192, 168, 60, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.config(local_IP, gateway, subnet);

    // Connect to WiFi
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
    Serial.println("\nWiFi connected! IP: " + WiFi.localIP().toString());

    server.begin();
    Serial.println("Server started.");
    pinMode(MODE_ENTRY,     INPUT);
    pinMode(REDLED,         OUTPUT);
    pinMode(BLUELED,        OUTPUT);
    pinMode(ARDUINO_SIGNAL, OUTPUT);
    pinMode(FROM_ARDUINO,   INPUT);
}

// The main loop handles the state machine for both entry and submission modes, 
// as well as processing incoming HTTP requests to trigger actions like scanning IDs, 
// capturing images, and communicating with the Arduino.
void loop() {
    server.handleClient();

    mode = digitalRead(MODE_ENTRY);     // Read the mode pin to determine if we're in entry or submission mode

    // Check for mode switch and reset state if necessary
    if (submissionMode) {
        if (mode == LOW) {
            Serial.println("[Mode Switch] Detected mode switch back to Entry. Resetting state.");
            submissionMode = false;
            entryState = IDLE;
            NationalID  = "";
        }
    }

    // Dispensing mode (Entry)
    if (mode == LOW) {
        switch (entryState) {

            case IDLE:
                delay(500);
                ReadytoScanLED();
                Serial.println("[Entry] Waiting for ID scan...");
                entryState = SCANNING_ID;
                break;

            case SCANNING_ID: {
                digitalWrite(ARDUINO_SIGNAL, LOW);
                String id = ScanNationalID();
                if (id.length() > 7) {
                    NationalID = id;
                    Serial.println("[Entry] ID captured: " + NationalID);
                    delay(1000);
                    entryState = SENDING_QR;  // ← proceed to send QR data after successful ID scan
                }
                break;
            }

            case SENDING_QR: {
                Serial.println("[Entry] Sending QR data...");
                bool ok = sendQRData();
                Serial.println(ok ? "[Entry] QR sent OK." : "[Entry] QR send FAILED.");
                if (ok) {
                    entryState = CAPTURING_IMAGE;  // ← proceed to capture image only if QR send was successful
                } else {
                    Serial.println("[Entry] Failed to send QR data.");
                    entryState = SCANNING_ID; // ← go back to scanning ID if QR send failed, or you could choose to retry sending QR data instead
                }
                break;
            }

            case CAPTURING_IMAGE: {
                Serial.println("[Entry] Capturing image...");
                delay(5000);
                FacialRecognitionLED();
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

            case SENDING_IMAGE: {
                Serial.println("[Entry] Sending image...");
                bool ok = sendImageData();
                Serial.println(ok ? "[Entry] Image sent OK." : "[Entry] Image send FAILED.");
                if (!ok) {
                    Serial.println("[Entry] Failed to send image data.");
                    entryState = SCANNING_ID; // ← go back to scanning ID if image send failed, or you could choose to retry sending image data instead
                } else {
                    entryState = SCANNING_KIT;  // ← proceed to scan kit only if image send was successful,
                    break;
                }
            }

            case SCANNING_KIT: {
                Serial.println("[Entry] Scanning test kit...");
                bool ok = scanExamKit();
                Serial.println(ok ? "[Entry] Kit scan OK." : "[Entry] Kit scan FAILED.");
                entryState = SENDING_KIT;
                break;
            }

            case SENDING_KIT: {
                Serial.println("[Entry] Sending kit data...");
                bool ok = sendKitData();
                Serial.println(ok ? "[Entry] Kit data sent OK." : "[Entry] Kit data send FAILED.");
                if (ok) {
                    Serial.println("[Entry] Entry process complete. Waiting for Arduino confirmation...");
                    entryState = WAITING_ARD;  // For entry, we wait for Arduino confirmation before resetting, so we go to WAITING_ARD
                } else {
                    Serial.println("[Entry] Failed to send kit data.");
                    entryState = SCANNING_ID; // ← go back to scanning ID if kit data send failed, or you could choose to retry sending kit data instead
                }
                entryState = WAITING_ARD;  // For entry, we wait for Arduino confirmation before resetting, so we go to WAITING_ARD
                break;
            }

            case WAITING_ARD: {
                Serial.println("[Entry] Sending Signal to Arduino");
                dispensingLED();
                digitalWrite(ARDUINO_SIGNAL, HIGH);
                entryState = DONE;
                break;
            }
            
            case DONE: {
                bool submit = digitalRead(FROM_ARDUINO);
                if (submit) {
                    Serial.println("[Entry] Submission confirmed by Arduino.");
                    digitalWrite(ARDUINO_SIGNAL, LOW);
                    entryState = IDLE;
                    NationalID  = "";
                }
                break;
            }
        }

    } else {        // Submission mode

        // If we detect a mode switch while we're in the middle of entry mode, we reset everything and switch to submission mode.
        if (entryState != IDLE && !submissionMode) {
            Serial.println("[Mode Switch] Detected mode switch. Resetting state.");
            entryState = IDLE;
            NationalID  = "";
            submissionMode = true;
        }


        switch (entryState) {
            case IDLE:
                delay(500);
                ReadytoScanLED();
                Serial.println("[Submission] Waiting for ID scan...");
                entryState = SCANNING_ID;
                break;

            case SCANNING_ID: {
                String id = ScanNationalID();
                if (id.length() > 7) {
                    NationalID = id;
                    Serial.println("[Submission] ID captured: " + NationalID);
                    delay(1000);
                    entryState = SENDING_QR;  // For submission, we can go straight to sending QR after scanning ID
                }
                break;
            }

            case SENDING_QR: {
                Serial.println("[Submission] Sending QR data...");
                bool ok = sendQRData();
                Serial.println(ok ? "[Submission] QR sent OK." : "[Submission] QR send FAILED.");
                if (ok) {
                    entryState = CAPTURING_IMAGE;
                } else {
                    Serial.println("[Submission] Failed to send QR data.");
                    entryState = SCANNING_ID; // ← go back to scanning ID if QR send failed, or you could choose to retry sending QR data instead
                }
                break;
            }

            case CAPTURING_IMAGE: {
                Serial.println("[Submission] Capturing image...");
                delay(5000);
                FacialRecognitionLED();
                bool success = fetchImage();
                if (success) {
                    printImageToSerial();
                    entryState = SENDING_IMAGE;        // ← proceed to send
                } else {
                    Serial.println("[Submission] Image capture failed. Retrying...");
                    // stays in CAPTURING_IMAGE to retry next loop
                }
                break;
            }

            case SENDING_IMAGE: {
                Serial.println("[Submission] Sending image...");
                bool ok = sendImageData();
                Serial.println(ok ? "[Submission] Image sent OK." : "[Submission] Image send FAILED.");
                if (ok) {
                    entryState = WAITING_ARD;  // For submission, we just go back to idle after sending image
                } else {
                    Serial.println("[Submission] Failed to send image data.");
                    entryState = SCANNING_ID; // ← go back to scanning ID if image send failed, or you could choose to retry sending image data instead
                }
                break;
            }

            case WAITING_ARD: {
                Serial.println("[Submission] Process complete. Resetting state.");
                dispensingLED();
                digitalWrite(ARDUINO_SIGNAL, HIGH);
                entryState = DONE;
                break;
            }
            
            case DONE: {
                bool submit = digitalRead(FROM_ARDUINO);
                if (submit) {
                    Serial.println("[Submission] Submission confirmed by Arduino.");
                    digitalWrite(ARDUINO_SIGNAL, LOW);
                    entryState = IDLE;
                    NationalID  = "";
                }
                break;
            }

        delay(5000);
        }
    }
}