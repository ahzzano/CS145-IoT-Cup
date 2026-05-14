#include <WiFi.h>
#include <WebServer.h>
#include "esp_camera.h"

const char* ssid = "s3wifi";
const char* password = "Com9L3x!";

WebServer server(8080);

// AI Thinker ESP32-CAM pin map
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

void initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
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
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size   = FRAMESIZE_QQVGA;
  config.jpeg_quality = 7;
  config.fb_count     = 1;
  config.fb_location  = CAMERA_FB_IN_DRAM;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return;
  }
  Serial.println("Camera initialized.");
}

// ← NEW: webpage that shows the captured image
void handlePage() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<title>ESP32-CAM</title>";
  html += "<style>";
  html += "body { font-family: Arial; text-align: center; background: #f0f0f0; padding: 20px; }";
  html += "img { width: 320px; height: 240px; border: 2px solid #333; }";
  html += "button { margin-top: 10px; padding: 10px 20px; font-size: 16px; cursor: pointer; }";
  html += "</style></head><body>";
  html += "<h2>ESP32-CAM Capture</h2>";
  html += "<img src='/capture' id='camImage'><br>";
  html += "<button onclick=\"document.getElementById('camImage').src='/capture?t='+Date.now()\">";
  html += "Capture Again</button>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleCapture() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    server.send(500, "text/plain", "Camera capture failed");
    return;
  }

  // ── Image metadata ──────────────────────────────
  Serial.println("===== IMAGE DATA =====");
  Serial.printf("Width:       %d px\n", fb->width);
  Serial.printf("Height:      %d px\n", fb->height);
  Serial.printf("Size:        %d bytes\n", fb->len);
  Serial.printf("Format:      %d (3=JPEG)\n", fb->format);

  // ── JPEG header validation ───────────────────────
  Serial.printf("JPEG Header: %02X %02X (should be FF D8)\n", fb->buf[0], fb->buf[1]);
  Serial.printf("JPEG Footer: %02X %02X (should be FF D9)\n", fb->buf[fb->len-2], fb->buf[fb->len-1]);

  // ── Base64 preview (first 64 bytes) ─────────────
  Serial.println("First 64 bytes (hex):");
  for (int i = 0; i < 64 && i < fb->len; i++) {
    Serial.printf("%02X ", fb->buf[i]);
    if ((i + 1) % 16 == 0) Serial.println();
  }
  Serial.println();

  // ── How it will be sent to backend ──────────────
  Serial.println("Transmission format: raw JPEG bytes over HTTP");
  Serial.println("Content-Type: image/jpeg");
  Serial.printf("Content-Length: %d\n", fb->len);
  Serial.println("======================");

  server.sendHeader("Content-Type", "image/jpeg");
  server.sendHeader("Content-Length", String(fb->len));
  server.send_P(200, "image/jpeg", (const char*)fb->buf, fb->len);

  esp_camera_fb_return(fb);
}

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("ESP32-CAM IP: ");
  Serial.println(WiFi.localIP());

  initCamera();

  server.on("/", HTTP_GET, handlePage);           // ← NEW
  server.on("/capture", HTTP_GET, handleCapture);
  server.begin();
  Serial.println("HTTP server started on port 8080");
}

void loop() {
  server.handleClient();
}