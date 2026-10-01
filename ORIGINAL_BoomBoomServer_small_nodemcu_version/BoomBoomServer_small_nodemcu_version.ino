#include <ESP8266WiFi.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

const char* AP_SSID = "BoomBoom";
const char* AP_PASSWORD = "87654321";
const uint16_t WS_PORT = 8081;

// ------------ BTS7960 ------------
const int RPWM_PIN = D1;   // GPIO5
const int LPWM_PIN = D2;   // GPIO4

// ------------ L298N ------------
const int STEER_IN1_PIN = D5;   // GPIO14
const int STEER_IN2_PIN = D6;   // GPIO12
const int STEER_ENA_PIN = D7;   // GPIO13

const int AXIS_MAX = 1023;
const int DEADZONE = 40;
const int MIN_DUTY = 70;
const int MAX_DUTY = 1023;   // ESP8266 PWM range

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
    int mag = abs(x);

    if (mag < DEADZONE)
    {
        digitalWrite(STEER_IN1_PIN, LOW);
        digitalWrite(STEER_IN2_PIN, LOW);
        analogWrite(STEER_ENA_PIN, 0);
        return;
    }

    int duty = map(mag, DEADZONE, AXIS_MAX, MIN_DUTY, MAX_DUTY);

    if (x > 0)
    {
        digitalWrite(STEER_IN1_PIN, HIGH);
        digitalWrite(STEER_IN2_PIN, LOW);
    }
    else
    {
        digitalWrite(STEER_IN1_PIN, LOW);
        digitalWrite(STEER_IN2_PIN, HIGH);
    }

    analogWrite(STEER_ENA_PIN, duty);
}

void stopAll()
{
    driveThrottle(0);
    driveSteering(0);
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

    pinMode(STEER_IN1_PIN, OUTPUT);
    pinMode(STEER_IN2_PIN, OUTPUT);
    pinMode(STEER_ENA_PIN, OUTPUT);

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
}