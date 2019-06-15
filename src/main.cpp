/**
 * esp-slack-notifier
 *
 * Shows Slack presence status through icons on a NeoPixel display
 * 
 */

#include "Arduino.h"
#include "icons.h"
#include <SPI.h>    // remember to research why this is needed
#include <Adafruit_NeoPixel.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_GFX.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>

// set the pin to be used by the NeoMatrix display
#define PIN 1

// object for NeoMatrix display
Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(8, 8, PIN,
  NEO_MATRIX_TOP     + NEO_MATRIX_LEFT +
  NEO_MATRIX_ROWS    + NEO_MATRIX_ZIGZAG,
  NEO_GRB            + NEO_KHZ800
);

// variables for WiFi
ESP8266WiFiMulti WiFiMulti;
#define WIFI_SSID "your-wifi-ssid"
#define WIFI_PASSWORD "your-wifi-password"

// variables for builtin LED
int ledState = LOW;
long previousLedMillis = 0;
long ledInterval = 1000;

// variables for polling presence
long previousPollMillis = 0;
long pollInterval = 10000;

// variables for Slack
const uint8_t fingerprint[20] = {0xC1, 0x0D, 0x53, 0x49, 0xD2, 0x3E, 0xE5, 0x2B, 0xA2, 0x61, 0xD5, 0x9E, 0x6F, 0x99, 0x0D, 0x3D, 0xFD, 0x8B, 0xB2, 0xB3};
#define SLACK_BOT_TOKEN "your-bot-token"
#define SLACK_USR_ID "your-user-id"

// object for parsing presence JSON
StaticJsonDocument<200> jsonDoc;
const char active[] = "active";

bool pollSlackPresence();
void drawIcon(bool presence);

void setup ()
{
  Serial.begin(9600);

  // initialize built in LED pin as output for status
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_BUILTIN, HIGH);

  // initialize NeoPixel matrix display
  matrix.begin();
  matrix.setBrightness(20);
  matrix.fillScreen(0);
  matrix.show();

  // initialize WiFi connection
  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
}

void loop ()
{
  long currentMillis = millis();

  if (currentMillis - previousLedMillis > ledInterval) {
    previousLedMillis = currentMillis;   

    // if the LED is off turn it on and vice-versa:
    if (ledState == LOW)
      ledState = HIGH;
    else
      ledState = LOW; 

    // set the LED with the ledState of the variable:
    digitalWrite(LED_BUILTIN, ledState);
  }

  if (currentMillis - previousPollMillis > pollInterval) {
    previousPollMillis = currentMillis;

    drawIcon(pollSlackPresence());
  }
}

void drawIcon (bool presence) 
{
  if (presence) {
    // draw a red cross mark
    matrix.fillScreen(0);
    matrix.drawBitmap(0, 0, crossMark, 8, 8, matrix.Color(255,0,0));
    matrix.show();
  }
  else {
    // draw a green check mark
    matrix.fillScreen(0);
    matrix.drawBitmap(0, 0, checkMark, 8, 8, matrix.Color(0,255,0));
    matrix.show();
  }
  return;
}

bool pollSlackPresence () 
{
  const char* presence = "away";
  // wait for WiFi connection
  if ((WiFiMulti.run() == WL_CONNECTED)) {

    std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
    client->setFingerprint(fingerprint);

    HTTPClient https;

    Serial.print("HTTPS begin\n");
    if (https.begin(*client, "https://slack.com/api/users.getPresence?token="SLACK_BOT_TOKEN"&user="SLACK_USR_ID"&pretty=1")) {

      // start connection and send HTTP request
      int httpCode = https.GET();

      // httpCode will be negative on error
      if (httpCode > 0) {
        // HTTP header has been send and Server response header has been handled
        Serial.printf("HTTPS GET: %d\n", httpCode);

        // JSON response from Slack
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          String payload = https.getString();
          DeserializationError error = deserializeJson(jsonDoc, payload);
          if (error) {
            Serial.print(F("JSON parse failed: "));
            Serial.println(error.c_str());
          }
          presence = jsonDoc["presence"];
          Serial.println(presence);
        }
      } else {
        Serial.printf("HTTPS GET error: %s\n", https.errorToString(httpCode).c_str());
      }

      https.end();
    } else {
      Serial.printf("HTTPS unable to connect\n");
    }
  }
  return strcmp(presence, active);
}