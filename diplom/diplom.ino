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

// Настройки точки доступа
const char *ssid = "ESP32_AP";
const char *password = "12345678";

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

  // Запуск точки доступа
  WiFi.softAP(ssid, password);

  IPAddress IP = WiFi.softAPIP();
  Serial.println("Точка доступа запущена:");
  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print("Password: ");
  Serial.println(password);
  Serial.print("IP Address: ");
  Serial.println(IP);

  // Вывод данных на дисплей
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(F("Wi-Fi Access Point"));
  display.println("-------------------");
  display.print(F("SSID: "));
  display.println(ssid);
  display.print(F("Password: "));
  display.println(password);
  display.print(F("IP: "));
  display.println(IP.toString());
  display.display();

  // Настройка маршрутов веб-сервера
  server.on("/", handleRoot);
  server.on("/upload", HTTP_POST, handleFileUpload);
  server.on("/list", HTTP_GET, handleFileList);
  server.on("/read", HTTP_GET, handleFileRead);
  server.begin();

  Serial.println("Веб-сервер запущен");
}

void loop() {
  // Обработка запросов веб-сервера
  server.handleClient();
}

void handleRoot() {
  String html = R"rawliteral(
    <!DOCTYPE html>
    <meta charset="UTF-8">
    <html>
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
  server.send(200, "text/html; charset=UTF-8", html); // Указана кодировка UTF-8
}

File uploadFile; // Глобальная переменная для текущего файла

void handleFileUpload() {
  HTTPUpload &upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    String filename = "/" + upload.filename;
    Serial.printf("Начало загрузки файла: %s\n", filename.c_str());
    
    // Открываем файл на запись
    uploadFile = SD.open(filename, FILE_WRITE);
    if (!uploadFile) {
      Serial.println("Ошибка открытия файла для записи");
      server.send(500, "text/plain; charset=UTF-8", "Ошибка открытия файла для записи");
      return;
    }
  } 
  else if (upload.status == UPLOAD_FILE_WRITE) {
    // Записываем текущий блок данных
    if (uploadFile) {
      uploadFile.write(upload.buf, upload.currentSize);
    } else {
      Serial.println("Файл не открыт на запись");
      server.send(500, "text/plain; charset=UTF-8", "Ошибка записи в файл");
      return;
    }
  } 
  else if (upload.status == UPLOAD_FILE_END) {
    // Закрываем файл после завершения загрузки
    if (uploadFile) {
      uploadFile.close();
      Serial.println("Файл успешно записан на SD-карту");
      server.send(200, "text/plain; charset=UTF-8", "Файл успешно загружен");
    } else {
      Serial.println("Ошибка завершения записи");
      server.send(500, "text/plain; charset=UTF-8", "Ошибка завершения записи");
    }
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

