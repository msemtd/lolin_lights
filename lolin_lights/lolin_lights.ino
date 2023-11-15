/*
 *  LoLin nodemcu web API lightswitch
 *
 */
#define MSEPRODUCT "LoLin nodemcu web API lightswitch"
#define MSEVERSION "v1.0"

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include "deb8.h"

ESP8266WiFiMulti WiFiMulti;

#include "config.h"
#ifndef AP_SSID
#error "You need to define some variables in config.h or perhaps here..."
#define AP_SSID         "your_access_point"
#define AP_PASSPHRASE   "your_ap_password"
// You can also set further AP definitions here - see setup function
#define SET_API_SERVER  "your_server_address"
#define SET_API_PORT    "80"
#define URL_OFF         "http://" SET_API_SERVER ":" SET_API_PORT "/off_path"
#define URL_ON          "http://" SET_API_SERVER ":" SET_API_PORT "/on_path"
#endif

wl_status_t wfs = WL_DISCONNECTED;

// Digital I/O --------------------------------------------------------------
// map lolin DIO pins to arduino IDE pin numbers...
// D0, D1, D2, D3, D4, D5, D6, D7, D8
// 16,  5,  4,  0,  2, 14, 12, 13, 15

// Here we want just the top 8 inputs
const uint8_t d_map[8] = {16, 5, 4, 0, 2, 14, 12, 13};
static uint8_t d_inputs = 0;
static debounce8_t d_deb;

// Application control ------------------------------------------------------
bool light_on = false;
// Current time read once per loop...
uint32_t now = 0;

// Diagnostic console -------------------------------------------------------
// When using a console line reading mode, append non-line-ending char to input
// string (if not already overflowed).
// Both CR and LF can terminate the line and ESC (0x1b) can clear the line.
#define DIAG_INPUT_MAX 128
String consoleInput = "";
bool consoleLineMode = false;
bool consoleDebug = false;

void version() {
    Serial.println(MSEPRODUCT " " MSEVERSION);
}

void setup() {
    inputs_setup();
    consoleInput.reserve(DIAG_INPUT_MAX);
    Serial.begin(115200);
    delay(10);
    Serial.println();
    Serial.println();
    version();
    WiFiMulti.addAP(AP_SSID, AP_PASSPHRASE);
#ifdef AP_SSID2
    WiFiMulti.addAP(AP_SSID2, AP_PASSPHRASE2);
#endif
#ifdef AP_SSID3
    WiFiMulti.addAP(AP_SSID3, AP_PASSPHRASE3);
#endif
    Serial.print("Waiting for WiFi... ");
    wfs = WiFiMulti.run();
}

void loop() {
    now = millis();
    task_inputs();
    // acting on buttons - pressed? held? how long? auto-repeat?

    // bloop(light_on);
    // light_on = !light_on;
    // Serial.println("wait 5 sec...");
    task_serial_read();
}

void bloop(bool tog) {
    const char * host = SET_API_SERVER; // ip or dns
    const char * url_on = URL_ON;
    const char * url_off = URL_OFF;

    wfs = WiFiMulti.run();
    if(wfs != WL_CONNECTED) {
        Serial.println("No AP connection");
        return;
    }

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

uint8_t inputs_read(void) {
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
  // We have a global "now" updated every loop...
  // uint32_t now = millis();
  if (now - last > 5) {
    last = now;
    uint8_t d_samp = inputs_read();
    d_samp = debouncer8(d_samp, &d_deb);
    if (d_samp != d_inputs) {
      d_inputs = d_samp;
      Serial.print("Inputs: ");
      Serial.println(d_inputs, HEX);
    }
  }
}

void task_serial_read(void) {
      while(Serial.available()){
        proc_console_input(Serial.read());
    }
}

int proc_console_input(int k) {
    // When using a console line reading mode, append non-line-ending char to input 
    // string (if not already overflowed).
    // Both CR and LF can terminate the line and ESC (0x1b) can clear the line.
    if(consoleLineMode) {
        switch(k) {
        case 10:
        case 13:
            proc_console_line();
            consoleInput = "";
            return 0;
        case 27:
            consoleInput = "";
            if(consoleDebug)
                Serial.println("ESC");
            return 0;
        default:
            // TODO backspace and arrow chars, useful stuff
            if(consoleInput.length() < DIAG_INPUT_MAX) {
                // ascii printable 0x20 to 0x7E
                if(k < 0x20 || k > 0x7E) {
                    // non-printable - do nothing
                    if(consoleDebug) {
                        Serial.print("non-ascii printable: ");
                        Serial.println(k, DEC);
                    }
                } else {
                    consoleInput += (char)k;
                }
            } else {
                if(consoleDebug) {
                    Serial.println("command tool long - cat sat on keyboard? discard data");
                }
                consoleInput = "";
            }
        }
        return 0;
    }
    return proc_command(k);
}

int proc_command(int k) {
    // Single character commands...
    uint8_t d_samp = 0;
    switch(k) {
    case 10:
    case 13:
    case 27:
        return 0;
    case 'v':
        version();
        return 0;
    case 'h':
    case '?':
        return help();
    case 'd':
        Serial.println("d = console debug on/off");
        consoleDebug = !consoleDebug;
        Serial.print("console debug is now ");
        Serial.println(consoleDebug);
        return 0;
    case 'l':
        Serial.println("l = toggle line mode");
        consoleLineMode = !consoleLineMode;
        Serial.print("line mode is now ");
        Serial.println(consoleLineMode);
        return 0;
    case 'i':
        d_samp = inputs_read();
        Serial.print("inputs (BIN):");
        Serial.println(d_samp, BIN);
        return 0;
    default:
        if(consoleDebug) {
            Serial.print("proc_command unused: ");
            Serial.println(k, DEC);
        }
    }
    return 0;
}

int help() {
    // this should return all the known commmands
    Serial.println("h/? = help");
    Serial.println("v = version");
    Serial.println("l = toggle line mode");
    Serial.println("d = console debug on/off");
    return 0;
}    

int proc_console_line() {
    consoleInput.trim();
    if(consoleInput.length() == 0)
        return 0;
    if(consoleDebug) {
        Serial.print("Line mode: '");
        Serial.print(consoleInput);
        Serial.println("'");
    }
    if(consoleInput.length() == 1) {
        return proc_command(consoleInput[0]);
    }
    // TODO actually have some line commands!
    return 0;
}
