#include <Wire.h>
#include <BH1750.h>
#include <WiFiNINA.h>
#include <ArduinoHttpClient.h>

// ==================== USER SETTINGS ====================
const char* WIFI_SSID = "Sumit 4g";       
const char* WIFI_PASS = "Kller@257";   

const char* IFTTT_HOST = "maker.ifttt.com";
const int   IFTTT_PORT = 443; // HTTPS
const char* IFTTT_KEY  = "3GXlbFpIO5gywtG-QO-3aog-xp3dl_zBc1i_Vshtt5"; // your Webhooks key

const char* EVENT_START = "sunlight_started";   // event names created in IFTTT
const char* EVENT_STOP  = "sunlight_stopped";

// ---------------- Indoor Test Thresholds ----------------
// Room light ~150 lux, torchlight can go above 200 lux
const float SUNLIGHT_THRESHOLD_HIGH = 200.0;  // trigger START when lux >= this
const float SUNLIGHT_THRESHOLD_LOW  = 150.0;  // trigger STOP when lux <= this
// --------------------------------------------------------

BH1750 lightMeter;
WiFiSSLClient ssl;
HttpClient http(ssl, IFTTT_HOST, IFTTT_PORT);

bool inSun = false;
unsigned long stateChangeMs = 0;
unsigned long sunAccumTodayMs = 0;
unsigned long dayStartMs = 0;

// ==================== WIFI ====================
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

// ==================== SENSOR ====================
float readLux() {
  return lightMeter.readLightLevel(); // returns lux
}

// ==================== IFTTT ====================
bool sendIFTTTEvent(const char* event, const String& v1="", const String& v2="", const String& v3="") {
  connectWiFi();

  String path = "/trigger/";
  path += event;
  path += "/with/key/";
  path += IFTTT_KEY;

  String body = "{";
  if (v1.length()) { body += "\"value1\":\"" + v1 + "\""; }
  if (v2.length()) { if (v1.length()) body += ","; body += "\"value2\":\"" + v2 + "\""; }
  if (v3.length()) { if (v1.length() || v2.length()) body += ","; body += "\"value3\":\"" + v3 + "\""; }
  body += "}";

  http.beginRequest();
  http.post(path);
  http.sendHeader("Content-Type", "application/json");
  http.sendHeader("Content-Length", body.length());
  http.endRequest();
  http.print(body);

  int statusCode = http.responseStatusCode();
  String response = http.responseBody();

  Serial.print("IFTTT status: ");
  Serial.println(statusCode);

  return (statusCode >= 200 && statusCode < 300);
}

// ==================== STATE MACHINE ====================
void checkSunlight() {
  float lux = readLux();
  unsigned long now = millis();

  Serial.print("Lux: ");
  Serial.println(lux);

  if (!inSun && lux >= SUNLIGHT_THRESHOLD_HIGH) {
    // Sunlight just started
    inSun = true;
    stateChangeMs = now;
    sendIFTTTEvent(EVENT_START, "Lux=" + String(lux));
    Serial.println("☀️ Sunlight STARTED");
  }
  else if (inSun && lux <= SUNLIGHT_THRESHOLD_LOW) {
    // Sunlight just stopped
    inSun = false;
    unsigned long thisBurst = now - stateChangeMs;
    sunAccumTodayMs += thisBurst;

    sendIFTTTEvent(EVENT_STOP, 
                   "Total minutes today=" + String(sunAccumTodayMs/60000),
                   "Last burst seconds=" + String(thisBurst/1000),
                   "Lux=" + String(lux));
    Serial.println("🌑 Sunlight STOPPED");
  }

  // Reset daily counter every 24 hours
  if (now - dayStartMs >= 24UL*60UL*60UL*1000UL) {
    sunAccumTodayMs = 0;
    dayStartMs = now;
  }
}

// ==================== SETUP + LOOP ====================
void setup() {
  Serial.begin(115200);
  Wire.begin();

  lightMeter.begin();
  Serial.println("BH1750 started.");

  connectWiFi();

  dayStartMs = millis();
}

void loop() {
  checkSunlight();
  delay(1000); // check every 1 second
}
