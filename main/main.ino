#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <DHT.h>

/* Параметри запуску */
const char* ssid = "example"; // Ім'я мережі (якщо connectToExistingNetwork = true, то це ім'я мережі, до якої підключаємося)
const char* password = "example1234"; // Пароль мережі (якщо connectToExistingNetwork = true, то це пароль мережі, до якої підключаємося)

const char* ssidAP = "SmartGreenhouse_AP"; // Ім'я точки доступу (якщо connectToExistingNetwork = false, то це ім'я точки доступу)
const char* passwordAP = "12345678"; // Пароль точки доступу (якщо connectToExistingNetwork = false, то це пароль точки доступу)

const bool connectToExistingNetwork = false; // False - Точка доступу, True - Підключення до наявної мережі

const char* footerText = "Курсова робота здобувача освіти<br>Мусієнко Олександра, група 3-013<br>";

/* Піни для підключення реле та датчиків */
#define RELAY1_PIN 5  // GPIO5 (D1) -- Освітлення
#define RELAY2_PIN 4  // GPIO4 (D2) -- Вентиляція
#define RELAY3_PIN 0  // GPIO0 (D3) -- Обігрів
#define RELAY4_PIN 13 // GPIO13 (D7) -- Полив

#define SOIL_SENSOR_PIN 14 // GPIO14 (D5) -- Датчик вологості ґрунту
#define LIGHT_SENSOR_PIN A0 // A0 -- Датчик освітленості
#define DHT_PIN 12 // GPIO12 (D6) -- Датчик DHT11

#define DHTTYPE DHT11 // Визначення типу датчика

DHT dht(DHT_PIN, DHTTYPE); // Ініціалізація датчика DHT11

ESP8266WebServer server(80); 

/* Адреса в EEPROM для зберігання значень порогів */
#define EEPROM_SIZE 8
#define LIGHT_THRESHOLD_ADDR 0
#define TEMP_THRESHOLD_ADDR 2
#define HUMIDITY_THRESHOLD_ADDR 4

/* Значення порогів */
int lightThreshold;
int tempThreshold;
int humidityThreshold;
bool manualMode = false;

unsigned long previousMillis = 0;
const long interval = 2000;

void setup() {
  Serial.begin(115200);

  EEPROM.begin(EEPROM_SIZE);
  lightThreshold = EEPROM.read(LIGHT_THRESHOLD_ADDR) + (EEPROM.read(LIGHT_THRESHOLD_ADDR + 1) << 8);
  if (lightThreshold == 0xFFFF) lightThreshold = 750; // Встановити значення за замовчуванням, якщо EEPROM порожній
  tempThreshold = EEPROM.read(TEMP_THRESHOLD_ADDR) + (EEPROM.read(TEMP_THRESHOLD_ADDR + 1) << 8);
  if (tempThreshold == 0xFFFF) tempThreshold = 25; // Встановити значення за замовчуванням, якщо EEPROM порожній
  humidityThreshold = EEPROM.read(HUMIDITY_THRESHOLD_ADDR) + (EEPROM.read(HUMIDITY_THRESHOLD_ADDR + 1) << 8);
  if (humidityThreshold == 0xFFFF) humidityThreshold = 60; // Встановити значення за замовчуванням, якщо EEPROM порожній

  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(RELAY3_PIN, OUTPUT);
  pinMode(RELAY4_PIN, OUTPUT);

  pinMode(SOIL_SENSOR_PIN, INPUT);
  pinMode(LIGHT_SENSOR_PIN, INPUT);

  dht.begin(); // Запуск датчика DHT11

  /* Вимкнути всі реле при запуску */ 
  digitalWrite(RELAY1_PIN, HIGH); // Освітлення вимкнено
  digitalWrite(RELAY2_PIN, HIGH); // Вентиляція вимкнена
  digitalWrite(RELAY3_PIN, HIGH); // Обігрів вимкнено
  digitalWrite(RELAY4_PIN, HIGH); // Полив вимкнено

  /* Налаштування WiFi */
  if (connectToExistingNetwork) {
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");
  } else {
    WiFi.softAP(ssidAP, passwordAP);
    Serial.println("Access Point started");
  }

  /* Налаштування маршрутів для веб-сервера */
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

    /* Зчитування даних з датчиків */
    int lightLevel = analogRead(LIGHT_SENSOR_PIN);
    int soilMoisture = digitalRead(SOIL_SENSOR_PIN);
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    Serial.print("Light Level: ");
    Serial.print(lightLevel);
    Serial.println(" lx");
    Serial.print("Soil Moisture: ");
    Serial.println(soilMoisture ? "Dry" : "Wet");
    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.println(" °C");
    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.println(" %");

    /* Управління освітленням */
    if (lightLevel < lightThreshold) {
      digitalWrite(RELAY1_PIN, LOW); // Увімкнути освітлення
    } else {
      digitalWrite(RELAY1_PIN, HIGH); // Вимкнути освітлення
    }

    /* Управління системою поливу */
    if (soilMoisture == 1) { // Якщо ґрунт сухий
      digitalWrite(RELAY4_PIN, LOW); // Увімкнути полив
    } else {
      digitalWrite(RELAY4_PIN, HIGH); // Вимкнути полив
    }

    /* Управління обігрівом */
    if (temperature < tempThreshold) {
      digitalWrite(RELAY3_PIN, LOW); // Увімкнути обігрів
    } else {
      digitalWrite(RELAY3_PIN, HIGH); // Вимкнути обігрів
    }

    /* Управління вентиляцією */
    if (humidity > humidityThreshold) {
      digitalWrite(RELAY2_PIN, LOW); // Увімкнути вентиляцію
    } else {
      digitalWrite(RELAY2_PIN, HIGH); // Вимкнути вентиляцію
    }
  }
}

void handleRoot() {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");
  server.sendContent("<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'><title>Управління теплицею</title>");
  server.sendContent("<style>");
  server.sendContent("body { font-family: Arial, sans-serif; margin: 0; padding: 0; display: flex; flex-direction: column; align-items: center; background-color: #f4f4f9; }");
  server.sendContent("h1 { color: #333; font-size: 24px; text-align: center; }");
  server.sendContent("form { margin-bottom: 20px; width: 100%; text-align: center; }");
  server.sendContent("input[type='number'], input[type='submit'], button { padding: 10px; font-size: 14px; margin: 5px; border-radius: 5px; border: 1px solid #ccc; width: 100%; max-width: 300px; box-sizing: border-box; }");
  server.sendContent("input[type='submit'], button { background-color: #007BFF; color: white; border: none; cursor: pointer; }");
  server.sendContent("input[type='submit']:hover, button:hover { background-color: #0056b3; }");
  server.sendContent(".status { background-color: #fff; padding: 15px; border-radius: 10px; box-shadow: 0 0 10px rgba(0, 0, 0, 0.1); margin: 20px 0; width: 100%; max-width: 700px; text-align: left; box-sizing: border-box; }");
  server.sendContent(".status p { margin: 10px 0; font-size: 14px; white-space: nowrap; }");
  server.sendContent(".relay-button { display: block; width: 100%; max-width: 300px; margin: 10px auto; text-align: center; }");
  server.sendContent(".relay-button button { width: 100%; transition: background-color 0.3s; white-space: nowrap; box-sizing: border-box; }");
  server.sendContent(".relay-button button.on { background-color: #28a745; color: white; }");
  server.sendContent(".relay-button button.off { background-color: #dc3545; color: white; }");
  server.sendContent(".tab { display: none; }");
  server.sendContent(".tab.active { display: block; }");
  server.sendContent(".tabs { display: flex; justify-content: space-around; width: 100%; background-color: #007BFF; }");
  server.sendContent(".tabs button { flex: 1; padding: 10px; font-size: 14px; color: white; border: none; background-color: #007BFF; cursor: pointer; white-space: nowrap; box-sizing: border-box; }");
  server.sendContent(".tabs button:hover, .tabs button.active { background-color: #0056b3; }");
  server.sendContent(".container { display: flex; flex-direction: column; align-items: center; width: 100%; padding: 10px; box-sizing: border-box; }");
  server.sendContent(".footer { margin: 20px; font-size: 14px; text-align: center; white-space: nowrap; }");
  server.sendContent("</style></head><body>");
  server.sendContent("<h1>Управління теплицею</h1>");
  server.sendContent("<div class='tabs'>");
  server.sendContent("<button class='tablink' onclick=\"openTab(event, 'monitoring')\" id='defaultOpen'>Моніторинг</button>");
  server.sendContent("<button class='tablink' onclick=\"openTab(event, 'control')\">Управління</button>");
  server.sendContent("<button class='tablink' onclick=\"openTab(event, 'config')\">Конфігурація</button>");
  server.sendContent("</div>");
  server.sendContent("<div class='container'>");

  /* Вкладка Моніторинг */ 
  server.sendContent("<div id='monitoring' class='tab active'>");
  server.sendContent("<h2>Дані з датчиків</h2>");
  server.sendContent("<div class='status'><p>Освітленість: <span id='lightLevel'></span></p>");
  server.sendContent("<p>Вологість ґрунту: <span id='soilMoisture'></span></p>");
  server.sendContent("<p>Температура: <span id='temperature'></span></p>");
  server.sendContent("<p>Вологість: <span id='humidity'></span></p></div>");
  server.sendContent("<h2>Стан реле</h2>");
  server.sendContent("<div class='status'><p>Освітлення: <span id='relay1'></span></p>");
  server.sendContent("<p>Вентиляція: <span id='relay2'></span></p>");
  server.sendContent("<p>Обігрів: <span id='relay3'></span></p>");
  server.sendContent("<p>Полив: <span id='relay4'></span></p></div>");
  server.sendContent("</div>");

  /* Вкладка Управління */ 
  server.sendContent("<div id='control' class='tab'>");
  server.sendContent("<h2>Режим</h2>");
  server.sendContent("<form action='/mode' method='POST'>");
  server.sendContent("<input type='radio' name='mode' value='auto'" + String(manualMode ? "" : " checked") + "> Автоматичний<br>");
  server.sendContent("<input type='radio' name='mode' value='manual'" + String(manualMode ? " checked" : "") + "> Ручний<br>");
  server.sendContent("<input type='submit' value='Встановити режим'>");
  server.sendContent("</form>");
  if (manualMode) {
    server.sendContent("<h2>Ручне управління</h2>");
    server.sendContent("<div class='relay-button'><button onclick=\"toggleRelay('1')\" id='relay1_button' class='off'>Увімкнути освітлення</button></div>");
    server.sendContent("<div class='relay-button'><button onclick=\"toggleRelay('2')\" id='relay2_button' class='off'>Увімкнути вентиляцію</button></div>");
    server.sendContent("<div class='relay-button'><button onclick=\"toggleRelay('3')\" id='relay3_button' class='off'>Увімкнути обігрів</button></div>");
    server.sendContent("<div class='relay-button'><button onclick=\"toggleRelay('4')\" id='relay4_button' class='off'>Увімкнути полив</button></div>");
  }
  server.sendContent("</div>");

  /* Вкладка Конфігурація */ 
  server.sendContent("<div id='config' class='tab'>");
  server.sendContent("<h2>Конфігурація</h2>");
  server.sendContent("<form action='/update' method='POST'>");
  server.sendContent("Поріг освітленості: <input type='number' name='lightThreshold' value='" + String(lightThreshold) + "'> lx<br>");
  server.sendContent("Поріг температури: <input type='number' name='tempThreshold' value='" + String(tempThreshold) + "'> °C<br>");
  server.sendContent("Поріг вологості: <input type='number' name='humidityThreshold' value='" + String(humidityThreshold) + "'> %<br>");
  server.sendContent("<input type='submit' value='Оновити'>");
  server.sendContent("</form>");
  server.sendContent("<h2>Інформація</h2>");
  server.sendContent("<ul>");
  server.sendContent("<li>Освітлення вмикається, коли рівень освітленості нижчий за " + String(lightThreshold) + " lx.</li>");
  server.sendContent("<li>Обігрів вмикається, коли температура нижча за " + String(tempThreshold) + " °C.</li>");
  server.sendContent("<li>Вентиляція вмикається, коли вологість вища за " + String(humidityThreshold) + " %.</li>");
  server.sendContent("<li>Полив вмикається, коли ґрунт сухий.</li>");
  server.sendContent("</ul>");
  server.sendContent("</div>");

  server.sendContent("</div>");
  server.sendContent("<div class='footer'>");
  server.sendContent(footerText);
  server.sendContent("Переглянути проєкт на <a href='https://github.com/cr1ma/smart-greenhouse' target='_blank'>GitHub</a>");
  server.sendContent("</div>");

  server.sendContent("<script>");
  server.sendContent("function openTab(evt, tabName) {");
  server.sendContent("var i, tabcontent, tablinks;");
  server.sendContent("tabcontent = document.getElementsByClassName('tab');");
  server.sendContent("for (i = 0; i < tabcontent.length; i++) {");
  server.sendContent("tabcontent[i].style.display = 'none';");
  server.sendContent("}");
  server.sendContent("tablinks = document.getElementsByClassName('tablink');");
  server.sendContent("for (i = 0; i < tablinks.length; i++) {");
  server.sendContent("tablinks[i].className = tablinks[i].className.replace(' active', '');");
  server.sendContent("}");
  server.sendContent("document.getElementById(tabName).style.display = 'block';");
  server.sendContent("evt.currentTarget.className += ' active';");
  server.sendContent("}");
  server.sendContent("document.getElementById('defaultOpen').click();");

  server.sendContent("function updateStatus() {");
  server.sendContent("fetch('/status').then(response => response.json()).then(data => {");
  server.sendContent("document.getElementById('lightLevel').innerText = data.lightLevel + ' lx';");
  server.sendContent("document.getElementById('soilMoisture').innerText = data.soilMoisture ? 'Сухий' : 'Вологий';");
  server.sendContent("document.getElementById('temperature').innerText = data.temperature + ' °C';");
  server.sendContent("document.getElementById('humidity').innerText = data.humidity + ' %';");
  server.sendContent("document.getElementById('relay1').innerText = data.relay1 ? 'Увімкнено' : 'Вимкнено';");
  server.sendContent("document.getElementById('relay2').innerText = data.relay2 ? 'Увімкнено' : 'Вимкнено';");
  server.sendContent("document.getElementById('relay3').innerText = data.relay3 ? 'Увімкнено' : 'Вимкнено';");
  server.sendContent("document.getElementById('relay4').innerText = data.relay4 ? 'Увімкнено' : 'Вимкнено';");
  server.sendContent("updateButtons(data);");
  server.sendContent("});");
  server.sendContent("}");

  server.sendContent("function toggleRelay(relay) {");
  server.sendContent("fetch('/relay?relay=' + relay + '_toggle').then(updateStatus);");
  server.sendContent("}");

  server.sendContent("function updateButtons(data) {");
  server.sendContent("document.getElementById('relay1_button').innerText = data.relay1 ? 'Вимкнути освітлення' : 'Увімкнути освітлення';");
  server.sendContent("document.getElementById('relay1_button').className = data.relay1 ? 'relay-button button on' : 'relay-button button off';");
  server.sendContent("document.getElementById('relay2_button').innerText = data.relay2 ? 'Вимкнути вентиляцію' : 'Увімкнути вентиляцію';");
  server.sendContent("document.getElementById('relay2_button').className = data.relay2 ? 'relay-button button on' : 'relay-button button off';");
  server.sendContent("document.getElementById('relay3_button').innerText = data.relay3 ? 'Вимкнути обігрів' : 'Увімкнути обігрів';");
  server.sendContent("document.getElementById('relay3_button').className = data.relay3 ? 'relay-button button on' : 'relay-button button off';");
  server.sendContent("document.getElementById('relay4_button').innerText = data.relay4 ? 'Вимкнути полив' : 'Увімкнути полив';");
  server.sendContent("document.getElementById('relay4_button').className = data.relay4 ? 'relay-button button on' : 'relay-button button off';");
  server.sendContent("}");

  server.sendContent("setInterval(updateStatus, 2000);");
  server.sendContent("updateStatus();");

  server.sendContent("</script>");
  server.sendContent("</body></html>");
  server.client().stop();
}

/* Обробник оновлення порогу освітленості та даних з DHT11 */ 
void handleUpdate() {
  if (server.hasArg("lightThreshold")) {
    lightThreshold = server.arg("lightThreshold").toInt();
    EEPROM.write(LIGHT_THRESHOLD_ADDR, lightThreshold & 0xFF);
    EEPROM.write(LIGHT_THRESHOLD_ADDR + 1, (lightThreshold >> 8) & 0xFF);
  }
  if (server.hasArg("tempThreshold")) {
    tempThreshold = server.arg("tempThreshold").toInt();
    EEPROM.write(TEMP_THRESHOLD_ADDR, tempThreshold & 0xFF);
    EEPROM.write(TEMP_THRESHOLD_ADDR + 1, (tempThreshold >> 8) & 0xFF);
  }
  if (server.hasArg("humidityThreshold")) {
    humidityThreshold = server.arg("humidityThreshold").toInt();
    EEPROM.write(HUMIDITY_THRESHOLD_ADDR, humidityThreshold & 0xFF);
    EEPROM.write(HUMIDITY_THRESHOLD_ADDR + 1, (humidityThreshold >> 8) & 0xFF);
  }
  EEPROM.commit();
  server.sendHeader("Location", "/");
  server.send(303);
}

/* Обробник керування реле */
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

/* Обробник зміни режиму */
void handleMode() {
  if (server.hasArg("mode")) {
    String mode = server.arg("mode");
    manualMode = (mode == "manual");

    /* Вимкнути всі реле при переключенні режимів */
    digitalWrite(RELAY1_PIN, HIGH);
    digitalWrite(RELAY2_PIN, HIGH);
    digitalWrite(RELAY3_PIN, HIGH);
    digitalWrite(RELAY4_PIN, HIGH);
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

/* Обробник статусу */
void handleStatus() {
  int lightLevel = analogRead(LIGHT_SENSOR_PIN);
  int soilMoisture = digitalRead(SOIL_SENSOR_PIN);
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  bool relay1State = digitalRead(RELAY1_PIN);
  bool relay2State = digitalRead(RELAY2_PIN); 
  bool relay3State = digitalRead(RELAY3_PIN);
  bool relay4State = digitalRead(RELAY4_PIN);

  String json = "{";
  json += "\"lightLevel\":" + String(lightLevel) + ",";
  json += "\"soilMoisture\":" + String(soilMoisture) + ",";
  json += "\"temperature\":" + String(temperature) + ",";
  json += "\"humidity\":" + String(humidity) + ",";
  json += "\"relay1\":" + String(!relay1State) + ",";
  json += "\"relay2\":" + String(!relay2State) + ",";
  json += "\"relay3\":" + String(!relay3State) + ",";
  json += "\"relay4\":" + String(!relay4State);
  json += "}";

  server.send(200, "application/json", json);
}