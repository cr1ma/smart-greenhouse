#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

#define RELAY1_PIN 5  // GPIO5 (D1)
#define RELAY2_PIN 4  // GPIO4 (D2)
#define RELAY3_PIN 0  // GPIO0 (D3)
#define RELAY4_PIN 13 // GPIO13 (D7)
#define SOIL_SENSOR_PIN 14 // GPIO14 (D5)
#define LIGHT_SENSOR_PIN A0

ESP8266WebServer server(80);

#define EEPROM_SIZE 4
#define LIGHT_THRESHOLD_ADDR 0

int lightThreshold;
bool manualMode = false;

unsigned long previousMillis = 0;
const long interval = 2000;

void setup() {
  Serial.begin(115200);

  EEPROM.begin(EEPROM_SIZE);
  lightThreshold = EEPROM.read(LIGHT_THRESHOLD_ADDR) + (EEPROM.read(LIGHT_THRESHOLD_ADDR + 1) << 8);
  if (lightThreshold == 0xFFFF) lightThreshold = 750;

  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(RELAY3_PIN, OUTPUT);
  pinMode(RELAY4_PIN, OUTPUT);

  pinMode(SOIL_SENSOR_PIN, INPUT);
  pinMode(LIGHT_SENSOR_PIN, INPUT);

  digitalWrite(RELAY1_PIN, HIGH);
  digitalWrite(RELAY2_PIN, HIGH);
  digitalWrite(RELAY3_PIN, HIGH);
  digitalWrite(RELAY4_PIN, HIGH);

  WiFi.begin("ssid", "password");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  server.on("/", handleRoot);
  server.on("/update", handleUpdate);
  server.on("/relay", handleRelay);
  server.on("/mode", handleMode);
  server.on("/status", handleStatus);

  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();

  unsigned long currentMillis = millis();
  if (!manualMode && currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    int lightLevel = analogRead(LIGHT_SENSOR_PIN);
    int soilMoisture = digitalRead(SOIL_SENSOR_PIN);

    Serial.print("Light Level: ");
    Serial.println(lightLevel);
    Serial.print("Soil Moisture: ");
    Serial.println(soilMoisture);

    if (lightLevel < lightThreshold) {
      digitalWrite(RELAY1_PIN, LOW);
    } else {
      digitalWrite(RELAY1_PIN, HIGH);
    }

    if (soilMoisture == 1) {
      digitalWrite(RELAY4_PIN, LOW);
    } else {
      digitalWrite(RELAY4_PIN, HIGH);
    }
  }
}

void handleRoot() {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");
  server.sendContent("<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Управління теплицею</title>");
  server.sendContent("<style>");
  server.sendContent("body { font-family: Arial, sans-serif; margin: 0; padding: 0; display: flex; flex-direction: column; align-items: center; background-color: #f4f4f9; }");
  server.sendContent("h1 { color: #333; }");
  server.sendContent("form { margin-bottom: 20px; }");
  server.sendContent("input[type='number'], input[type='submit'], button { padding: 10px; font-size: 16px; margin: 5px; border-radius: 5px; border: 1px solid #ccc; }");
  server.sendContent("input[type='submit'], button { background-color: #007BFF; color: white; border: none; cursor: pointer; }");
  server.sendContent("input[type='submit']:hover, button:hover { background-color: #0056b3; }");
  server.sendContent(".status { margin: 20px 0; }");
  server.sendContent(".relay-button { display: inline-block; width: 45%; margin: 5px; text-align: center; }");
  server.sendContent(".relay-button button { width: 100%; }");
  server.sendContent("</style></head><body>");
  server.sendContent("<h1>Управління теплицею</h1>");
  server.sendContent("<form action='/update' method='POST'>");
  server.sendContent("Поріг освітленості: <input type='number' name='lightThreshold' value='" + String(lightThreshold) + "'><br>");
  server.sendContent("<input type='submit' value='Оновити'>");
  server.sendContent("</form>");
  server.sendContent("<h2>Режим</h2>");
  server.sendContent("<form action='/mode' method='POST'>");
  server.sendContent("<input type='radio' name='mode' value='auto'" + String(manualMode ? "" : " checked") + "> Автоматичний<br>");
  server.sendContent("<input type='radio' name='mode' value='manual'" + String(manualMode ? " checked" : "") + "> Ручний<br>");
  server.sendContent("<input type='submit' value='Встановити режим'>");
  server.sendContent("</form>");
  if (manualMode) {
    server.sendContent("<h2>Ручне управління</h2>");
    server.sendContent("<div class='relay-button'><button onclick=\"toggleRelay('1')\" id='relay1_button'>Увімкнути освітлення</button></div>");
    server.sendContent("<div class='relay-button'><button onclick=\"toggleRelay('2')\" id='relay2_button'>Увімкнути вентиляцію</button></div>");
    server.sendContent("<div class='relay-button'><button onclick=\"toggleRelay('3')\" id='relay3_button'>Увімкнути обігрів</button></div>");
    server.sendContent("<div class='relay-button'><button onclick=\"toggleRelay('4')\" id='relay4_button'>Увімкнути полив</button></div>");
  }
  server.sendContent("<h2>Стан</h2>");
  server.sendContent("<div class='status'><p>Освітленість: <span id='lightLevel'></span></p>");
  server.sendContent("<p>Вологість ґрунту: <span id='soilMoisture'></span></p>");
  server.sendContent("<p>Освітлення: <span id='relay1'></span></p>");
  server.sendContent("<p>Вентиляція: <span id='relay2'></span></p>");
  server.sendContent("<p>Обігрів: <span id='relay3'></span></p>");
  server.sendContent("<p>Полив: <span id='relay4'></span></p></div>");
  server.sendContent("<script>");
  server.sendContent("function updateStatus() {");
  server.sendContent("fetch('/status').then(response => response.json()).then(data => {");
  server.sendContent("document.getElementById('lightLevel').innerText = data.lightLevel;");
  server.sendContent("document.getElementById('soilMoisture').innerText = data.soilMoisture ? 'Сухий' : 'Вологий';");
  server.sendContent("document.getElementById('relay1').innerText = data.relay1 ? 'Вимкнено' : 'Увімкнено';");
  server.sendContent("document.getElementById('relay2').innerText = data.relay2 ? 'Вимкнено' : 'Увімкнено';");
  server.sendContent("document.getElementById('relay3').innerText = data.relay3 ? 'Вимкнено' : 'Увімкнено';");
  server.sendContent("document.getElementById('relay4').innerText = data.relay4 ? 'Вимкнено' : 'Увімкнено';");
  server.sendContent("updateButtons(data);");
  server.sendContent("});");
  server.sendContent("}");
  server.sendContent("function toggleRelay(relay) {");
  server.sendContent("fetch('/relay?relay=' + relay + '_toggle').then(updateStatus);");
  server.sendContent("}");
  server.sendContent("function updateButtons(data) {");
  server.sendContent("document.getElementById('relay1_button').innerText = data.relay1 ? 'Увімкнути освітлення' : 'Вимкнути освітлення';");
  server.sendContent("document.getElementById('relay2_button').innerText = data.relay2 ? 'Увімкнути вентиляцію' : 'Вимкнути вентиляцію';");
  server.sendContent("document.getElementById('relay3_button').innerText = data.relay3 ? 'Увімкнути обігрів' : 'Вимкнути обігрів';");
  server.sendContent("document.getElementById('relay4_button').innerText = data.relay4 ? 'Увімкнути полив' : 'Вимкнути полив';");
  server.sendContent("}");
  server.sendContent("setInterval(updateStatus, 2000);");
  server.sendContent("</script>");
  server.sendContent("</body></html>");
  server.client().stop();
}

void handleUpdate() {
  if (server.hasArg("lightThreshold")) {
    lightThreshold = server.arg("lightThreshold").toInt();
    EEPROM.write(LIGHT_THRESHOLD_ADDR, lightThreshold & 0xFF);
    EEPROM.write(LIGHT_THRESHOLD_ADDR + 1, (lightThreshold >> 8) & 0xFF);
    EEPROM.commit();
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleRelay() {
  String relayAction = server.arg("relay");
  int relayPin;

  if (relayAction == "1_toggle") {
    relayPin = RELAY1_PIN;
    digitalWrite(relayPin, !digitalRead(relayPin));
  } else if (relayAction == "2_toggle") {
    relayPin = RELAY2_PIN;
    digitalWrite(relayPin, !digitalRead(relayPin));
  } else if (relayAction == "3_toggle") {
    relayPin = RELAY3_PIN;
    digitalWrite(relayPin, !digitalRead(relayPin));
  } else if (relayAction == "4_toggle") {
    relayPin = RELAY4_PIN;
    digitalWrite(relayPin, !digitalRead(relayPin));
  } else {
    server.send(400, "text/plain", "Неправильна дія реле");
    return;
  }

  server.sendHeader("Location", "/");
  server.send(303);
}

void handleMode() {
  if (server.hasArg("mode")) {
    String mode = server.arg("mode");
    manualMode = (mode == "manual");

    digitalWrite(RELAY1_PIN, HIGH);
    digitalWrite(RELAY2_PIN, HIGH);
    digitalWrite(RELAY3_PIN, HIGH);
    digitalWrite(RELAY4_PIN, HIGH);
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleStatus() {
  int lightLevel = analogRead(LIGHT_SENSOR_PIN);
  int soilMoisture = digitalRead(SOIL_SENSOR_PIN);
  bool relay1State = digitalRead(RELAY1_PIN);
  bool relay2State = digitalRead(RELAY2_PIN);
  bool relay3State = digitalRead(RELAY3_PIN);
  bool relay4State = digitalRead(RELAY4_PIN);

  String json = "{";
  json += "\"lightLevel\":" + String(lightLevel) + ",";
  json += "\"soilMoisture\":" + String(soilMoisture) + ",";
  json += "\"relay1\":" + String(relay1State) + ",";
  json += "\"relay2\":" + String(relay2State) + ",";
  json += "\"relay3\":" + String(relay3State) + ",";
  json += "\"relay4\":" + String(relay4State);
  json += "}";

  server.send(200, "application/json", json);
}
