#include <ESP8266WiFi.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

const char* AP_SSID = "BoomBoom";
const char* AP_PASSWORD = "87654321";
const uint16_t WS_PORT = 8081;

// ------------ BTS7960 ------------
const int RPWM_PIN = D1;   // GPIO5
const int LPWM_PIN = D2;   // GPIO4

// ------------ 2-Channel Relay (active-LOW) ------------
const int STEER_IN1_PIN = D5;   // GPIO14  (relay 1)
const int STEER_IN2_PIN = D6;   // GPIO12  (relay 2)

const int RELAY_ON  = LOW;
const int RELAY_OFF = HIGH;

const int AXIS_MAX = 1023;
const int DEADZONE = 40;
const int MIN_DUTY = 70;
const int MAX_DUTY = 1023;   // ESP8266 PWM range

// ------------ Battery voltage sensing (A0 via external divider) ------------
// NodeMCU's A0 pin has its own onboard divider, so it reads 0-3.3V already.
// External divider (R1 = 100K from battery+, R2 = 22K to GND, A0 at the
// R1/R2 junction) scales the battery voltage down into that 0-3.3V range.
const float ADC_MAX_VALUE = 1023.0;
const float ADC_REF_VOLTAGE = 3.3;
const float VDIV_R1 = 99900.0;   // ohms (measured)
const float VDIV_R2 = 21400.0;   // ohms (measured)
const float VDIV_RATIO = (VDIV_R1 + VDIV_R2) / VDIV_R2;
const int BATTERY_SAMPLES = 8;

// Fine-tune multiplier to correct for the ESP8266's imprecise 3.3V reference
// and any residual error. Re-derive as: (multimeter reading) / (remote reading)
// using the current value below, then replace it with the new result.
const float CALIBRATION_FACTOR = 1.0228;

const unsigned long BATTERY_REPORT_INTERVAL_MS = 2000;
unsigned long lastBatteryReportAt = 0;

WebSocketsServer webSocket(WS_PORT);

void driveThrottle(int y)
{
    int mag = abs(y);

    if (mag < DEADZONE)
    {
        analogWrite(RPWM_PIN, 0);
        analogWrite(LPWM_PIN, 0);
        return;
    }

    int duty = map(mag, DEADZONE, AXIS_MAX, MIN_DUTY, MAX_DUTY);

    if (y > 0)
    {
        analogWrite(RPWM_PIN, duty);
        analogWrite(LPWM_PIN, 0);
    }
    else
    {
        analogWrite(RPWM_PIN, 0);
        analogWrite(LPWM_PIN, duty);
    }
}

void driveSteering(int x)
{
    if (abs(x) < DEADZONE)
    {
        digitalWrite(STEER_IN1_PIN, RELAY_OFF);
        digitalWrite(STEER_IN2_PIN, RELAY_OFF);
        return;
    }

    if (x > 0)
    {
        digitalWrite(STEER_IN2_PIN, RELAY_OFF);
        digitalWrite(STEER_IN1_PIN, RELAY_ON);
    }
    else
    {
        digitalWrite(STEER_IN1_PIN, RELAY_OFF);
        digitalWrite(STEER_IN2_PIN, RELAY_ON);
    }
}

void stopAll()
{
    driveThrottle(0);
    driveSteering(0);
}

float readBatteryVoltage()
{
    long total = 0;
    for (int i = 0; i < BATTERY_SAMPLES; i++)
    {
        total += analogRead(A0);
    }
    float raw = total / (float)BATTERY_SAMPLES;

    float adcVoltage = (raw / ADC_MAX_VALUE) * ADC_REF_VOLTAGE;
    return adcVoltage * VDIV_RATIO * CALIBRATION_FACTOR;
}

void broadcastBatteryVoltage()
{
    float voltage = readBatteryVoltage();

    StaticJsonDocument<64> doc;
    doc["battery"] = round(voltage * 100) / 100.0;

    char buf[64];
    size_t len = serializeJson(doc, buf, sizeof(buf));
    webSocket.broadcastTXT(buf, len);
}

void onWebSocketEvent(uint8_t clientId,
                      WStype_t type,
                      uint8_t *payload,
                      size_t length)
{
    switch(type)
    {
        case WStype_CONNECTED:
        {
            IPAddress ip = webSocket.remoteIP(clientId);
            Serial.printf("Client %u connected: %s\n",
                          clientId,
                          ip.toString().c_str());
            break;
        }

        case WStype_DISCONNECTED:
            Serial.println("Client disconnected");
            stopAll();
            break;

        case WStype_TEXT:
        {
            StaticJsonDocument<64> doc;

            if(deserializeJson(doc, payload, length))
                return;

            int x = doc["x"] | 0;
            int y = doc["y"] | 0;

            Serial.printf("X=%d  Y=%d\n", x, y);

            driveThrottle(y);
            driveSteering(x);
            break;
        }

        default:
            break;
    }
}

void setup()
{
    Serial.begin(115200);

    pinMode(RPWM_PIN, OUTPUT);
    pinMode(LPWM_PIN, OUTPUT);

    // Set relays OFF before enabling outputs so they don't click on at boot
    digitalWrite(STEER_IN1_PIN, RELAY_OFF);
    digitalWrite(STEER_IN2_PIN, RELAY_OFF);
    pinMode(STEER_IN1_PIN, OUTPUT);
    pinMode(STEER_IN2_PIN, OUTPUT);

    analogWriteRange(1023);
    analogWriteFreq(1000);

    stopAll();

    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);

    Serial.println();
    Serial.println("Access Point Started");
    Serial.print("IP: ");
    Serial.println(WiFi.softAPIP());

    webSocket.begin();
    webSocket.onEvent(onWebSocketEvent);
}

void loop()
{
    webSocket.loop();

    unsigned long now = millis();
    if (now - lastBatteryReportAt >= BATTERY_REPORT_INTERVAL_MS)
    {
        lastBatteryReportAt = now;
        broadcastBatteryVoltage();
    }
}