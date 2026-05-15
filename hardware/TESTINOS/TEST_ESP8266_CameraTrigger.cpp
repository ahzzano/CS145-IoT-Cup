#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>

const char* ssid     = "s3wifi";
const char* password = "Com9L3x!";

const char* esp32IP   = "192.168.60.236";
const int   esp32Port = 8080;

WiFiClient wifiClient;
ESP8266WebServer server(80);

uint8_t* imageBuffer = nullptr;
size_t   imageSize   = 0;

// ── Fetch image from ESP32-CAM and store in buffer ──
void fetchImage() {
  HTTPClient http;
  String url = "http://" + String(esp32IP) + ":" + String(esp32Port) + "/capture";

  Serial.println("Fetching image from ESP32-CAM...");
  http.begin(wifiClient, url);
  http.setTimeout(10000);

  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    int contentLength = http.getSize();
    Serial.printf("Image size: %d bytes\n", contentLength);

    // Free old buffer if exists
    if (imageBuffer != nullptr) {
      free(imageBuffer);
      imageBuffer = nullptr;
    }

    // Allocate new buffer
    imageBuffer = (uint8_t*)malloc(contentLength);
    if (!imageBuffer) {
      Serial.println("Memory allocation failed!");
      http.end();
      return;
    }

    // Read image into buffer
    WiFiClient* stream = http.getStreamPtr();
    size_t bytesRead = stream->readBytes(imageBuffer, contentLength);
    imageSize = bytesRead;

    Serial.printf("Bytes read: %d\n", bytesRead);
    Serial.printf("JPEG valid: %s\n", (imageBuffer[0] == 0xFF && imageBuffer[1] == 0xD8) ? "YES" : "NO");

  } else {
    Serial.printf("Fetch failed, code: %d\n", httpCode);
    Serial.printf("Error: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
}

// ── Serve the webpage ────────────────────────────────
void handlePage() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<title>ESP8266 Viewer</title>";
  html += "<style>";
  html += "body { font-family: Arial; text-align: center; background: #1a1a1a; color: white; padding: 20px; }";
  html += "img { width: 320px; height: 240px; border: 3px solid #444; border-radius: 8px; }";
  html += "button { margin-top: 15px; padding: 10px 25px; font-size: 16px;";
  html += "background: #0077ff; color: white; border: none; border-radius: 6px; cursor: pointer; }";
  html += "button:hover { background: #0055cc; }";
  html += "p { color: #aaa; font-size: 13px; }";
  html += "</style></head><body>";
  html += "<h2>ESP32-CAM Viewer</h2>";
  html += "<img src='/image' id='camImg'><br>";
  html += "<button onclick=\"fetch('/refresh').then(()=>{ document.getElementById('camImg').src='/image?t='+Date.now(); })\">Capture New Image</button>";
  html += "<p>ESP32-CAM: " + String(esp32IP) + ":" + String(esp32Port) + "</p>";
  html += "<p>Image size: " + String(imageSize) + " bytes</p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

// ── Serve the stored image ───────────────────────────
void handleImage() {
  if (imageBuffer == nullptr || imageSize == 0) {
    server.send(404, "text/plain", "No image captured yet.");
    return;
  }
  server.sendHeader("Content-Type", "image/jpeg");
  server.sendHeader("Content-Length", String(imageSize));
  server.sendHeader("Cache-Control", "no-cache");
  server.send_P(200, "image/jpeg", (const char*)imageBuffer, imageSize);
}

// ── Trigger a new capture ────────────────────────────
void handleRefresh() {
  fetchImage();
  server.send(200, "text/plain", "OK");
}

void setup() {
  Serial.begin(115200);

  IPAddress local_IP(192, 168, 60, 131);
  IPAddress gateway(192, 168, 60, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.config(local_IP, gateway, subnet);

  WiFi.begin(ssid, password);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("ESP8266 IP: ");
  Serial.println(WiFi.localIP());

  // Fetch first image on boot
  fetchImage();

  server.on("/",        HTTP_GET, handlePage);
  server.on("/image",   HTTP_GET, handleImage);
  server.on("/refresh", HTTP_GET, handleRefresh);
  server.begin();
  Serial.println("Web server started on port 80");
}

void loop() {
  server.handleClient();
}