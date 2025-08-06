#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPDash.h>
#include <ESPmDNS.h>

const int relayPins[] = {21, 19, 18, 5};
const int numRelays = sizeof(relayPins) / sizeof(int);
const int powerLEDPin = 25;
const int bubbleGunPin = 15;
Card* relays[numRelays];
Card* bubbleCard;
bool isBlasting = false;
unsigned long blastStartTime = 0;
unsigned long blastDuration = 1500;
volatile int blastCount = 0;

const char *ssid = "xxx";
const char *password = "xxx";

AsyncWebServer server(80);
ESPDash dashboard(&server);

void initPowerLED() {
  pinMode(powerLEDPin, OUTPUT);
  digitalWrite(powerLEDPin, HIGH);
}

void initRelays() {
  for (int i = 0; i < numRelays; ++i) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], LOW);
    String cardName = "Relay " + String(i + 1);
    relays[i] = new Card(&dashboard, BUTTON_CARD, cardName.c_str());
    relays[i]->attachCallback([i](bool value) {
      digitalWrite(relayPins[i], value);
      relays[i]->update(value);
      dashboard.sendUpdates();
    });
  }
}

void initBubbleGun() {
  pinMode(bubbleGunPin, OUTPUT);
  digitalWrite(bubbleGunPin, LOW);
  bubbleCard = new Card(&dashboard, BUTTON_CARD, "Bubble Gun");
  bubbleCard->attachCallback([](bool value) {
    digitalWrite(bubbleGunPin, value);
    bubbleCard->update(value);
    dashboard.sendUpdates();
  });
}

void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("WiFi Failed! Trying again...");
    delay(1000);
    WiFi.begin(ssid, password);
  }
  if (!MDNS.begin("lightbox")) {
    Serial.println("Error setting up MDNS responder!");
  }
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void startBlast(unsigned long duration) {
  if (isBlasting) {
    blastCount++;
    return;
  }
  blastDuration = duration;
  isBlasting = true;
  blastStartTime = millis();
  digitalWrite(relayPins[0], HIGH);
  for (int i = 1; i < numRelays; ++i) {
    digitalWrite(relayPins[i], LOW);
  }
  digitalWrite(bubbleGunPin, HIGH);
}

void updateBlast() {
  if (isBlasting && (millis() - blastStartTime >= blastDuration)) {
    digitalWrite(relayPins[0], LOW);
    for (int i = 1; i < numRelays; ++i) {
      digitalWrite(relayPins[i], HIGH);
    }
    digitalWrite(bubbleGunPin, LOW);
    isBlasting = false;
    if (blastCount > 0) {
      blastCount--;
      if (blastCount > 0) {
        startBlast(blastDuration);
      }
    }
  }
}

void initEndpoints() {
  for (int i = 0; i < numRelays; ++i) {
    String onEndpoint = "/" + String(i + 1) + "/on";
    String offEndpoint = "/" + String(i + 1) + "/off";
    server.on(onEndpoint.c_str(), HTTP_GET, [i](AsyncWebServerRequest *request) {
      digitalWrite(relayPins[i], HIGH);
      relays[i]->update(true);
      dashboard.sendUpdates();
      request->send(200, "text/plain", "Relay " + String(i + 1) + " turned ON");
    });
    server.on(offEndpoint.c_str(), HTTP_GET, [i](AsyncWebServerRequest *request) {
      digitalWrite(relayPins[i], LOW);
      relays[i]->update(false);
      dashboard.sendUpdates();
      request->send(200, "text/plain", "Relay " + String(i + 1) + " turned OFF");
    });
  }

  server.on("/blast", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("duration")) {
      String durationParam = request->getParam("duration")->value();
      unsigned long duration = durationParam.toInt();
      startBlast(duration);
      request->send(200, "text/plain", "Blast triggered for " + durationParam + " ms");
    } else {
      startBlast(blastDuration);
      request->send(200, "text/plain", "Blast triggered for default duration");
    }
  });
}

void setup() {
  Serial.begin(115200);
  initPowerLED();
  initRelays();
  initBubbleGun();
  initWiFi();
  initEndpoints();
  server.begin();
}

void loop() {
  updateBlast();
}
