#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

// ── Config ────────────────────────────────────────────────────────────────────
const char* SSID     = "your_wifi";
const char* PASSWORD = "your_password";
const char* BASE_URL = "http://192.168.1.100:8000";  // your backend IP

// JWT token stored after /mosip/auth
String jwtToken = "";

// Image buffer received from ESP32-CAM via Serial
uint8_t* imageBuffer  = nullptr;
size_t   imageSize    = 0;
#define  MAX_IMG_SIZE 60000

WiFiClient wifiClient;

// ── WiFi setup ────────────────────────────────────────────────────────────────
void connectWiFi() {
  WiFi.begin(SSID, PASSWORD);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected: " + WiFi.localIP().toString());
}

// ── Helper: add auth cookie to request ───────────────────────────────────────
void addAuth(HTTPClient& http) {
  if (jwtToken.length() > 0) {
    http.addHeader("Cookie", "token=" + jwtToken);
  }
}

// ── Helper: extract JWT from Set-Cookie response header ──────────────────────
void saveToken(HTTPClient& http) {
  // response header looks like: Set-Cookie: token=<jwt>; ...
  String cookie = http.header("Set-Cookie");
  if (cookie.length() == 0) return;

  int start = cookie.indexOf("token=");
  if (start == -1) return;
  start += 6;

  int end = cookie.indexOf(";", start);
  jwtToken = (end == -1) ? cookie.substring(start)
                         : cookie.substring(start, end);
  Serial.println("Token saved.");
}

// ─────────────────────────────────────────────────────────────────────────────
// POST /mosip/auth
// Inputs: uin (int), name (string)
// Returns: 200 OK + sets JWT cookie | 403 auth failed | 502 MOSIP error
// ─────────────────────────────────────────────────────────────────────────────
int mosipAuth(int uin, String name) {
  HTTPClient http;
  http.begin(wifiClient, String(BASE_URL) + "/mosip/auth");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.collectHeaders((const char*[]){"Set-Cookie"}, 1);  // capture cookie

  String body = "uin=" + String(uin) + "&name=" + name;

  int status = http.POST(body);
  Serial.println("[/mosip/auth] " + String(status));

  if (status == 200) {
    saveToken(http);
  }

  http.end();
  return status;
}

// ─────────────────────────────────────────────────────────────────────────────
// POST /examinee/
// Inputs: image file (from imageBuffer), uses JWT cookie for name+uin
// Returns: 200 created | 400 bad input | 401 not authenticated
// ─────────────────────────────────────────────────────────────────────────────
int enrollExaminee() {
  if (imageSize == 0) {
    Serial.println("No image in buffer");
    return -1;
  }

  HTTPClient http;
  http.begin(wifiClient, String(BASE_URL) + "/examinee/");
  addAuth(http);

  String boundary = "----VTBoundary";
  String head = "--" + boundary + "\r\n"
                "Content-Disposition: form-data; name=\"file\"; filename=\"id.jpg\"\r\n"
                "Content-Type: image/jpeg\r\n\r\n";
  String tail = "\r\n--" + boundary + "--\r\n";

  http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
  http.addHeader("Content-Length", String(head.length() + imageSize + tail.length()));

  WiFiClient* stream = http.getStreamPtr();
  stream->print(head);
  stream->write(imageBuffer, imageSize);
  stream->print(tail);

  int status = http.POST((uint8_t*)NULL, 0);
  Serial.println("[/examinee/] " + String(status));

  http.end();
  return status;
}

// ─────────────────────────────────────────────────────────────────────────────
// POST /examinee/timein
// Inputs: id (examinee int), image file (captured face from ESP32-CAM)
// Returns: 200 face match | 403 face mismatch | 404 examinee not found
// ─────────────────────────────────────────────────────────────────────────────
int timeIn(int examineeId) {
  if (imageSize == 0) {
    Serial.println("No image in buffer");
    return -1;
  }

  HTTPClient http;
  http.begin(wifiClient, String(BASE_URL) + "/examinee/timein");

  String boundary = "----VTBoundary";
  String head = "--" + boundary + "\r\n"
                "Content-Disposition: form-data; name=\"file\"; filename=\"face.jpg\"\r\n"
                "Content-Type: image/jpeg\r\n\r\n";
  String mid  = "\r\n--" + boundary + "\r\n"
                "Content-Disposition: form-data; name=\"id\"\r\n\r\n" +
                String(examineeId) + "\r\n";
  String tail = "--" + boundary + "--\r\n";

  http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
  http.addHeader("Content-Length", String(head.length() + imageSize + mid.length() + tail.length()));

  WiFiClient* stream = http.getStreamPtr();
  stream->print(head);
  stream->write(imageBuffer, imageSize);
  stream->print(mid);
  stream->print(tail);

  int status = http.POST((uint8_t*)NULL, 0);
  Serial.println("[/examinee/timein] " + String(status));

  http.end();
  return status;
}

// ─────────────────────────────────────────────────────────────────────────────
// POST /examinee/timeout
// Inputs: id (examinee int), image file (captured face from ESP32-CAM)
// Returns: 200 face match | 403 face mismatch | 404 examinee not found
// ─────────────────────────────────────────────────────────────────────────────
int timeOut(int examineeId) {
  if (imageSize == 0) {
    Serial.println("No image in buffer");
    return -1;
  }

  HTTPClient http;
  http.begin(wifiClient, String(BASE_URL) + "/examinee/timeout");

  String boundary = "----VTBoundary";
  String head = "--" + boundary + "\r\n"
                "Content-Disposition: form-data; name=\"file\"; filename=\"face.jpg\"\r\n"
                "Content-Type: image/jpeg\r\n\r\n";
  String mid  = "\r\n--" + boundary + "\r\n"
                "Content-Disposition: form-data; name=\"id\"\r\n\r\n" +
                String(examineeId) + "\r\n";
  String tail = "--" + boundary + "--\r\n";

  http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
  http.addHeader("Content-Length", String(head.length() + imageSize + mid.length() + tail.length()));

  WiFiClient* stream = http.getStreamPtr();
  stream->print(head);
  stream->write(imageBuffer, imageSize);
  stream->print(mid);
  stream->print(tail);

  int status = http.POST((uint8_t*)NULL, 0);
  Serial.println("[/examinee/timeout] " + String(status));

  http.end();
  return status;
}

// ─────────────────────────────────────────────────────────────────────────────
// POST /exam/link
// Inputs: examinee (int), exam_id (int), uses JWT cookie
// Returns: 200 linked | 400 already linked / not found | 401 not authenticated
// ─────────────────────────────────────────────────────────────────────────────
int linkExam(int examineeId, int examKitId) {
  HTTPClient http;
  http.begin(wifiClient, String(BASE_URL) + "/exam/link");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  addAuth(http);

  String body = "examinee=" + String(examineeId) + "&exam_id=" + String(examKitId);

  int status = http.POST(body);
  Serial.println("[/exam/link] " + String(status));

  http.end();
  return status;
}

// ─────────────────────────────────────────────────────────────────────────────
// POST /exam/submit
// Inputs: examinee (int)
// Returns: 200 submitted | 400 no linked kit
// ─────────────────────────────────────────────────────────────────────────────
int submitExam(int examineeId) {
  HTTPClient http;
  http.begin(wifiClient, String(BASE_URL) + "/exam/submit");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String body = "examinee=" + String(examineeId);

  int status = http.POST(body);
  Serial.println("[/exam/submit] " + String(status));

  http.end();
  return status;
}

// ─────────────────────────────────────────────────────────────────────────────
// Receive image from ESP32-CAM over Serial
// Protocol: CAM sends "IMG:<size>\n" → raw bytes → "END\n"
// ─────────────────────────────────────────────────────────────────────────────
bool receiveImageFromCam() {
  // tell CAM to capture
  Serial.println("CAPTURE");

  // wait for "IMG:<size>\n"
  String header = Serial.readStringUntil('\n');
  header.trim();
  if (!header.startsWith("IMG:")) {
    Serial.println("Bad header: " + header);
    return false;
  }

  imageSize = header.substring(4).toInt();
  if (imageSize == 0 || imageSize > MAX_IMG_SIZE) {
    Serial.println("Invalid size: " + String(imageSize));
    return false;
  }

  // read raw bytes
  size_t received = 0;
  unsigned long timeout = millis() + 10000;
  while (received < imageSize && millis() < timeout) {
    if (Serial.available()) {
      imageBuffer[received++] = Serial.read();
    }
  }

  // consume "END\n"
  Serial.readStringUntil('\n');

  if (received != imageSize) {
    Serial.printf("Incomplete image: %d / %d\n", received, imageSize);
    return false;
  }

  Serial.printf("Image received: %d bytes\n", imageSize);
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Setup & Loop
// ─────────────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  imageBuffer = (uint8_t*)malloc(MAX_IMG_SIZE);
  connectWiFi();
}

void loop() {
  // ── Example: Pre-Exam flow ─────────────────────────────────────────────────
  // Triggered when GM861S scans an ID and sends UIN + name over Serial

  // 1. Auth with MOSIP (UIN and name come from GM861S QR scan)
  int uin  = 5408602380;
  String name = "Juan dela Cruz";

  int authStatus = mosipAuth(uin, name);
  if (authStatus != 200) {
    // RED LED
    return;
  }

  // 4. Time in — capture face and compare with ID photo
  if (!receiveImageFromCam()) return;
  int timeinStatus = timeIn(uin);
  if (timeinStatus == 200) {
    // GREEN LED — dispense kit
    int examKitId = 1;  // scanned from internal GM861S
    linkExam(uin, examKitId);
  } else {
    // RED LED — face mismatch or error
  }

  // ── Example: Post-Exam flow ────────────────────────────────────────────────
  // Triggered when examinee returns and rescans ID

  // 5. Re-auth
  authStatus = mosipAuth(uin, name);
  if (authStatus != 200) return;

  // 6. Time out — capture face and compare with pre-exam photo
  if (!receiveImageFromCam()) return;
  int timeoutStatus = timeOut(uin);
  if (timeoutStatus == 200) {
    // GREEN LED — open submission bin
    submitExam(uin);
  } else {
    // RED LED — face mismatch
  }

  delay(5000);
}
