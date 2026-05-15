#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

// TODO CHANGE ME!!
#ifndef STASSID
#define STASSID "TP-Link_4390"
#define STAPSK "88123691" 

#endif

const char * ssid = STASSID;
const char * password = STAPSK;

ESP8266WebServer server(1145);

const int LDR = A0;
int reading = 888;

String indexFile;

void handleRoot() {
  reading = analogRead(LDR);
indexFile = "<html>\
  <head>\
    <title>CS 145 YO!</title>\
  </head>\
  <body>\
  ALOHA\
<p>Current LDR Reading: <span id=\"darkCount\">" + String(reading) + "</span> </p>\
<a href=/secondpage/>\
  Credits\
</a>\
  </body>\
</html>";  
  server.send(200, "text/html", indexFile);
}

void goSecond() {
indexFile = "<html>\
  <head>\
    <title>CS 145 YO!</title>\
  </head>\
  <body>\
  <h3>Group Members</h3>\
  <p>ANCF | RLVT | RKBM</p>\
  </body>\
</html>";  
  server.send(200, "text/html", indexFile);
}



void handleNotFound() {
  String message = "PAGE NOT FOUND\n\n";
  server.send(404, "text/plain", message);
}

void setup(void) {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.println("");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }
  server.on("/", handleRoot);
  server.on("/secondpage/", goSecond);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");
}

void loop(void) {
  server.handleClient();
//  reading = analogRead(LDR);
}
