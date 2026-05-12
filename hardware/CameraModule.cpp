#include <Arduino.h>
#include <WiFi.h>         // For connecting to WiFi
#include <HTTPClient.h>   // For sending HTTP POST requests to the backend
#include "esp_camera.h"   // ESP32-CAM camera driver
#include <HardwareSerial.h>

// --- WiFi Credentials ---
// Replace these with your actual network name and password
const char* ssid     = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

// --- Backend URL ---
// Replace YOUR_BACKEND_IP with the local IP of the machine running your Flask server
// Make sure ESP32-CAM and the server are on the same WiFi network
const char* serverUrl = "http://YOUR_BACKEND_IP:5000/recognize";

// --- ESP32-CAM (AI-Thinker) GPIO Pin Definitions ---
// These are fixed hardware pins for the AI-Thinker ESP32-CAM module
// Do not change these unless you are using a different camera board variant
#define PWDN_GPIO_NUM     32  // Power down pin — used to power cycle the camera
#define RESET_GPIO_NUM    -1  // Reset pin — not connected on AI-Thinker (-1 = unused)
#define XCLK_GPIO_NUM      0  // External clock output to camera sensor
#define SIOD_GPIO_NUM     26  // SCCB data line (like I2C SDA) for camera config
#define SIOC_GPIO_NUM     27  // SCCB clock line (like I2C SCL) for camera config
#define Y9_GPIO_NUM       35  // Pixel data bit 9 (MSB) — part of 8-bit parallel data bus
#define Y8_GPIO_NUM       34  // Pixel data bit 8
#define Y7_GPIO_NUM       39  // Pixel data bit 7
#define Y6_GPIO_NUM       36  // Pixel data bit 6
#define Y5_GPIO_NUM       21  // Pixel data bit 5
#define Y4_GPIO_NUM       19  // Pixel data bit 4
#define Y3_GPIO_NUM       18  // Pixel data bit 3
#define Y2_GPIO_NUM        5  // Pixel data bit 2 (LSB)
#define VSYNC_GPIO_NUM    25  // Vertical sync — signals start of a new frame
#define HREF_GPIO_NUM     23  // Horizontal reference — signals start of a new line
#define PCLK_GPIO_NUM     22  // Pixel clock — clocks each pixel on the data bus

// --- Camera Initialization ---
// Configures and starts the camera sensor with the pin map and image settings above
void initCamera() {
  camera_config_t config;

  // Assign the LEDC (PWM) channel and timer used to generate the camera's clock signal
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;

  // Map each data and control pin to the hardware GPIO numbers defined above
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;

  // Set the clock frequency for the camera sensor (20 MHz is standard)
  config.xclk_freq_hz = 20000000;

  // Output format: JPEG — compressed image, much smaller than raw, ideal for HTTP upload
  config.pixel_format = PIXFORMAT_JPEG;

  // Frame size: QVGA = 320x240 pixels
  // Good balance between image detail and upload speed over WiFi
  // Use FRAMESIZE_VGA (640x480) for better accuracy if your network allows
  config.frame_size   = FRAMESIZE_QVGA;

  // JPEG compression quality: lower number = higher quality, larger file
  // 10–12 is a good range; avoid going below 10 (too large) or above 20 (too blurry)
  config.jpeg_quality = 12;

  // Number of frame buffers to allocate in memory
  // 1 is sufficient for single-shot capture; use 2+ for streaming
  config.fb_count     = 1;

  // Initialize the camera with the configuration above
  // Returns ESP_OK on success
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    // Print the error code in hex to help with debugging
    Serial.printf("Camera init failed: 0x%x\n", err);
  }
}

// --- Send Captured Image to Backend ---
// Takes a camera frame buffer, sends it as a raw JPEG via HTTP POST,
// reads the response ("MATCH" or "NO_MATCH"), and relays it back to the ESP32 via Serial
void sendImageToBackend(camera_fb_t* fb) {
  HTTPClient http;

  // Open a connection to the backend server URL
  http.begin(serverUrl);

  // Tell the server we're sending a raw JPEG image (not form data or JSON)
  http.addHeader("Content-Type", "image/jpeg");

  // Send the HTTP POST request with the JPEG bytes from the camera frame buffer
  // fb->buf = pointer to the image data, fb->len = size in bytes
  int httpCode = http.POST(fb->buf, fb->len);

  if (httpCode == 200) {
    // Get the plain-text response body from the backend ("MATCH" or "NO_MATCH")
    String response = http.getString();
    response.trim(); // Strip any extra whitespace

    Serial.println("Backend response: " + response);

    // Relay the result back to the ESP32 over UART (Serial)
    // The ESP32 is listening for exactly "MATCH" or "NO_MATCH"
    Serial.println(response);
  } else {
    // Something went wrong with the HTTP request — print the error code
    Serial.println("HTTP error: " + String(httpCode));

    // Default to NO_MATCH so the ESP32 doesn't hang waiting for a response
    Serial.println("NO_MATCH");
  }

  // Always close the HTTP connection to free up memory
  http.end();
}

void setup() {
  // Start UART0 (Serial) at 115200 baud
  // On ESP32-CAM, UART0 is also used to communicate with the ESP32 via TX/RX pins
  // Note: disconnect the CAM-MB programmer board before using UART0 at runtime
  Serial.begin(115200);

  // Begin WiFi connection using the credentials defined above
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");
  // Block here until WiFi is connected, printing a dot each half-second
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Print the assigned local IP — useful for verifying network connectivity
  Serial.println("\nWiFi connected: " + WiFi.localIP().toString());

  // Initialize the camera hardware
  initCamera();
}

void loop() {
  // Check if any data has been received over Serial (from the ESP32 trigger)
  if (Serial.available()) {
    // Read until a newline character — this is the command sent by the ESP32
    String cmd = Serial.readStringUntil('\n');
    cmd.trim(); // Remove whitespace/newline for clean comparison

    if (cmd == "CAPTURE") {
      Serial.println("Capture command received — taking photo...");

      // Grab a single frame from the camera into a frame buffer in memory
      camera_fb_t* fb = esp_camera_fb_get();

      if (!fb) {
        // Frame buffer is null — camera failed to capture
        Serial.println("Camera capture failed");
        // Send NO_MATCH back to ESP32 so it doesn't wait indefinitely
        Serial.println("NO_MATCH");
        return; // Exit loop() early and try again on next iteration
      }

      // Send the captured image to the backend for recognition
      sendImageToBackend(fb);

      // IMPORTANT: Always return the frame buffer after use
      // Failing to do this will exhaust the camera's memory and cause crashes
      esp_camera_fb_return(fb);
    }
  }
}