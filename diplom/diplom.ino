#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"

// Настройка пинов для SPI
#define CS_PIN 10   // Chip Select для SD-карты
#define SCK_PIN 13  // Clock
#define MISO_PIN 12 // Master In Slave Out
#define MOSI_PIN 11 // Master Out Slave In

// Настройки дисплея SSD1306
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

#define SDA_PIN 4 // Новый пин для SDA
#define SCL_PIN 5 // Новый пин для SCL

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Настройка режима Wi-Fi
#define WIFI_MODE_AP 0 // Установите 1 для AP, 0 для STA

// Настройки Wi-Fi в режиме клиента (STA)
const char *sta_ssid = "applied_robotics";
const char *sta_password = "listentome";

// Настройки точки доступа (AP)
const char *ap_ssid = "ESP32_AP";
const char *ap_password = "12345678";

// Веб-сервер
WebServer server(80);

void setup() {
  // Инициализация последовательного порта
  Serial.begin(115200);

  // Инициализация I2C на новых пинах
  Wire.begin(SDA_PIN, SCL_PIN);

  // Инициализация дисплея
  display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);

  display.clearDisplay();
  display.display();

  // Инициализация SD-карты
  display.setCursor(0, 0);
  display.setTextColor(WHITE);
  display.println(F("Инициализация SD..."));
  display.display();

  // Настройка подтяжки для всех линий SPI
  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH); // Включаем подтяжку на CS (HIGH как default state)
  pinMode(SCK_PIN, INPUT_PULLUP);  // Подтяжка для Clock
  pinMode(MISO_PIN, INPUT_PULLUP); // Подтяжка для MISO
  pinMode(MOSI_PIN, INPUT_PULLUP); // Подтяжка для MOSI

  // Инициализация SPI с указанными пинами
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CS_PIN);

  if (!SD.begin(CS_PIN, SPI)) {
    Serial.println(F("Ошибка инициализации SD-карты"));
    display.println(F("SD Card Error!"));
    display.display();
    return;
  }

  Serial.println(F("SD-карта успешно инициализирована"));
  display.println(F("SD Card OK!"));
  display.display();

  // Инициализация Wi-Fi
  if (WIFI_MODE_AP) {
    setupWiFiAP();
  } else {
    setupWiFiSTA();
  }

  // Настройка маршрутов веб-сервера
  server.on("/", handleRoot);
  server.on("/upload", HTTP_POST, []() {
    server.send(200, "text/plain; charset=UTF-8", "Файл успешно загружен");
  }, handleFileUpload);
  server.on("/list", HTTP_GET, handleFileList);
  server.on("/read", HTTP_GET, handleFileRead);
  server.begin();

  Serial.println("Веб-сервер запущен");
}

void setupWiFiAP() {
  WiFi.softAP(ap_ssid, ap_password);
  IPAddress IP = WiFi.softAPIP();
  Serial.println("Точка доступа запущена:");
  Serial.print("SSID: ");
  Serial.println(ap_ssid);
  Serial.print("Password: ");
  Serial.println(ap_password);
  Serial.print("IP Address: ");
  Serial.println(IP);

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Wi-Fi AP Mode");
  display.println("-------------------");
  display.print("SSID: ");
  display.println(ap_ssid);
  display.print("Password: ");
  display.println(ap_password);
  display.print("IP: ");
  display.println(IP.toString());
  display.display();
}

void setupWiFiSTA() {
  WiFi.begin(sta_ssid, sta_password);

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Wi-Fi Client Mode");
  display.println("-------------------");
  display.print("Connecting to: ");
  display.println(sta_ssid);
  display.display();

  Serial.print("Подключение к Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nПодключено к Wi-Fi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  display.setCursor(0, 30);
  display.print("IP: ");
  display.println(WiFi.localIP().toString());
  display.display();
}

void loop() {
  // Обработка запросов веб-сервера
  server.handleClient();
}

void handleRoot() {
  String html = R"rawliteral(
    <!DOCTYPE html>
    <html>
    <meta charset="UTF-8">
    <body>
      <h1>SD Card Web Server</h1>
      <h2>Загрузить файл:</h2>
      <form method="POST" action="/upload" enctype="multipart/form-data">
        <input type="file" name="file">
        <input type="submit" value="Upload">
      </form>
      <h2>Список файлов:</h2>
      <a href="/list">Просмотр файлов</a>
      <h2>Прочитать файл:</h2>
      <form method="GET" action="/read">
        <input type="text" name="filename" placeholder="Имя файла">
        <input type="submit" value="Read">
      </form>
    </body>
    </html>
  )rawliteral";
  server.send(200, "text/html; charset=UTF-8", html);
}

File uploadFile; // Глобальная переменная для текущего файла

void logRequest() {
  Serial.printf("Метод: %s\n", server.method() == HTTP_GET ? "GET" : "POST");
  Serial.printf("URI: %s\n", server.uri().c_str());
  for (uint8_t i = 0; i < server.args(); i++) {
    Serial.printf("Аргумент %s: %s\n", server.argName(i).c_str(), server.arg(i).c_str());
  }
}

void handleFileUpload() {
  HTTPUpload &upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    String filename = "/" + upload.filename;
    filename.replace(" ", "_");
    Serial.printf("Начало загрузки файла: %s\n", filename.c_str());

    File file = SD.open(filename, FILE_WRITE);
    if (!file) {
      Serial.println("Ошибка открытия файла для записи");
      return;
    }

    file.close();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    Serial.printf("Записано %d байт\n", upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    Serial.println("Загрузка завершена");
  } else {
    Serial.println("Неизвестный статус");
  }
}




void handleFileList() {
  File root = SD.open("/");
  String list = "<meta charset=\"UTF-8\"><h2>Список файлов:</h2><ul>";
  File file = root.openNextFile();
  while (file) {
    list += "<li>" + String(file.name()) + "</li>";
    file = root.openNextFile();
  }
  list += "</ul>";
  server.send(200, "text/html; charset=UTF-8", list); // Указана кодировка UTF-8
}

void handleFileRead() {
  if (!server.hasArg("filename")) {
    server.send(400, "text/plain; charset=UTF-8", "Не указано имя файла");
    return;
  }
  
  String filename = "/" + server.arg("filename");
  File file = SD.open(filename, FILE_READ);
  if (!file) {
    server.send(404, "text/plain; charset=UTF-8", "Файл не найден");
    return;
  }
  
  server.sendHeader("Content-Type", "application/octet-stream");
  server.sendHeader("Content-Disposition", "attachment; filename=" + server.arg("filename"));
  server.sendHeader("Connection", "close");

  // Отправляем файл клиенту
  server.streamFile(file, "application/octet-stream");
  file.close();
}

