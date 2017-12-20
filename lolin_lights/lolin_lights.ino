/*
 *  LoLin nodemcu web API lightswitch
 *
 */

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include "deb8.h"

ESP8266WiFiMulti WiFiMulti;

#include "config.h"

// map lolin DIO pins to arduino IDE pin numbers...
// D0, D1, D2, D3, D4, D5, D6, D7, D8
// 16,  5,  4,  0,  2, 14, 12, 13, 15

// Here we want just the top 8 inputs
const uint8_t d_map[8] = {16, 5, 4, 0, 2, 14, 12, 13};
static uint8_t d_inputs = 0;
static debounce8_t d_deb;
bool light_on = false;

void setup() {
    inputs_setup();
    Serial.begin(115200);
    delay(10);
    // Connect to a WiFi network...
    WiFiMulti.addAP(AP_SSID, AP_PASSPHRASE);
    Serial.println();
    Serial.println();
    Serial.print("Wait for WiFi... ");
    while(WiFiMulti.run() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
    }
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    delay(500);
}

void loop() {
  bloop(light_on);
  light_on = !light_on;
  Serial.println("wait 5 sec...");
  delay(5000);
}

void bloop(bool tog) {
    const char * host = SET_API_SERVER; // ip or dns
    const char * url_on = URL_ON;
    const char * url_off = URL_OFF;
    
    Serial.print("connecting to ");
    Serial.println(host);
    HTTPClient http;
    Serial.print("to switch light to ");
    Serial.println(tog);
    http.begin(tog ? url_on : url_off); //HTTP
    Serial.println("[HTTP] GET...");
    // start connection and send HTTP header
    int httpCode = http.GET();
    // httpCode will be negative on error
    if(httpCode > 0) {
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTP] GET... code: %d\n", httpCode);
        // file found at server
        if(httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_FOUND) {
            String payload = http.getString();
            Serial.println(payload);
        }
    } else {
        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
}

void inputs_setup(void) {
    for (int i = 0; i < 8; i++) {
        pinMode(d_map[i], INPUT_PULLUP);
    }
    d_inputs = inputs_read();
}

uint16_t inputs_read(void) {
    uint8_t d_samp = 0;
    for (int i = 0; i < 8; i++) {
        bitWrite(d_samp, i, (digitalRead(d_map[i]) ? 0 : 1));
    }
    return d_samp;
}

/*
 * The inputs are sampled every 5 ms (max)
 */
void task_inputs(void)
{
  static uint32_t last;
  uint32_t now = millis();
  if (now - last > 5) {
    last = now;
    uint8_t d_samp = inputs_read();
    d_samp = debouncer8(d_samp, &d_deb);
    if (d_samp != d_inputs) {
      d_inputs = d_samp;
      Serial.print("Inputs: ");
      Serial.println(d_inputs, BIN);
    }
  }
}

