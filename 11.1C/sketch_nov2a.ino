#include <WiFiNINA.h>
#include <PubSubClient.h>
#include <Servo.h>

// ====== USER CONFIG ======
const char* WIFI_SSID = "laptoppi";
const char* WIFI_PASS = "12345678";

const char* MQTT_HOST = "192.168.137.13";  // Pi broker on laptoppi subnet
const uint16_t MQTT_PORT = 1883;
const char* MQTT_USER = "door";
const char* MQTT_PASS = "ags";

const char* TOPIC_CMD    = "door/lock/cmd";
const char* TOPIC_STATUS = "door/lock/status";
const char* LWT_TOPIC    = "door/lock/lwt";

#define SERVO_PIN 9
const int LOCK_POS   = 10;     // tune for your latch
const int UNLOCK_POS = 170;    // tune for your latch
const int STEP_DELAY_MS = 5;   // smooth motion speed
// ==========================

WiFiClient wifi;
PubSubClient mqtt(wifi);
Servo lockServo;

int currentPos = LOCK_POS;
unsigned long lastWiFiCheck = 0;
unsigned long lastMQTTCheck = 0;

String makeClientId() {
  byte mac[6];
  WiFi.macAddress(mac);
  char buf[32];
  // nano33iot-XXYYZZ from MAC
  snprintf(buf, sizeof(buf), "nano33iot-%02X%02X%02X", mac[3], mac[4], mac[5]);
  return String(buf);
}

void moveTo(int target) {
  target = constrain(target, 0, 180);
  int step = (target > currentPos) ? 1 : -1;
  for (int p = currentPos; p != target; p += step) {
    lockServo.write(p);
    delay(STEP_DELAY_MS);
  }
  currentPos = target;
  lockServo.write(currentPos);
}

void publishStatus(const char* s) {
  mqtt.publish(TOPIC_STATUS, s, true); // retained status
}

void ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

void onMsg(char* topic, byte* payload, unsigned int len) {
  String cmd;
  cmd.reserve(len);
  for (unsigned int i = 0; i < len; i++) cmd += (char)payload[i];
  cmd.trim();
  cmd.toUpperCase();

  if (cmd == "LOCK") {
    moveTo(LOCK_POS);
    publishStatus("OK LOCK");
  } else if (cmd == "UNLOCK") {
    moveTo(UNLOCK_POS);
    publishStatus("OK UNLOCK");
  } else if (cmd == "STATUS") {
    String s = "POS " + String(currentPos);
    publishStatus(s.c_str());
  } else if (cmd.startsWith("SET ")) {
    int v = cmd.substring(4).toInt();
    moveTo(v);
    String s = "OK SET " + String(v);
    publishStatus(s.c_str());
  } else {
    publishStatus("ERR BAD_CMD");
  }
}

void ensureMQTT() {
  if (mqtt.connected()) return;
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMsg);

  String cid = makeClientId();
  // Connect with LWT
  while (!mqtt.connect(cid.c_str(), MQTT_USER, MQTT_PASS, LWT_TOPIC, 0, true, "OFFLINE")) {
    delay(1000);
  }
  publishStatus("ONLINE");
  mqtt.subscribe(TOPIC_CMD);
}

void setup() {
  lockServo.attach(SERVO_PIN);
  moveTo(LOCK_POS);       // start locked

  ensureWiFi();
  ensureMQTT();
}

void loop() {
  // Lightweight periodic checks
  unsigned long now = millis();

  if (now - lastWiFiCheck > 1000) {
    lastWiFiCheck = now;
    if (WiFi.status() != WL_CONNECTED) {
      ensureWiFi();
    }
  }
  if (now - lastMQTTCheck > 1000) {
    lastMQTTCheck = now;
    if (!mqtt.connected()) {
      ensureMQTT();
    }
  }

  mqtt.loop();
}