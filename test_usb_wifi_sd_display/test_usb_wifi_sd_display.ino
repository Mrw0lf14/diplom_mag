#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "SdFat.h"
#include "USB.h"
#include "USBMSC.h"
#include "Ticker.h"

// Настройка пинов для SPI
#define CS_PIN 10
#define SCK_PIN 13
#define MISO_PIN 12
#define MOSI_PIN 11

// Настройки дисплея SSD1306
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Настройки точки доступа
const char *ssid = "ESP32_AP";
const char *password = "12345678";

// WebServer
WebServer server(80);

// Настройки USB MSC
SdFat sd;
USBMSC MSC;
static const uint16_t DISK_SECTOR_SIZE = 512;
static uint32_t sectors = 0;

// Глобальные счетчики
static uint32_t readCounter = 0, writeCounter = 0, busyCounter = 0;

// Callbacks для USB MSC
static int32_t onWrite(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize) {
  if (sd.card()->isBusy()) busyCounter++;
  while (sd.card()->isBusy());
  return sd.card()->writeSectors(lba, buffer, bufsize / DISK_SECTOR_SIZE) ? bufsize : -1;
}

static int32_t onRead(uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
  if (sd.card()->isBusy()) busyCounter++;
  while (sd.card()->isBusy());
  return sd.card()->readSectors(lba, (uint8_t *)buffer, bufsize / DISK_SECTOR_SIZE) ? bufsize : -1;
}

static bool onStartStop(uint8_t power_condition, bool start, bool load_eject) {
  return true;
}

// Обработчики WebServer
void handleRoot();
void handleFileUpload();
void handleFileList();
void handleFileRead();

// Периодическая задача
Ticker ticker;

void printReadWriteCounter() {
  Serial.printf("ReadCounter: %d WriteCounter: %d BusyCounter: %d\n", readCounter, writeCounter, busyCounter);
}

void setup() {
  Serial.begin(115200);
  
  // Инициализация дисплея
  Wire.begin(4, 5); // Настройка I2C
  display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.println(F("Инициализация..."));
  display.display();

  // Инициализация SD
  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN);
  if (!sd.begin(CS_PIN)) {
    Serial.println("SD-карта не найдена!");
    display.println(F("Ошибка SD"));
    display.display();
    while (true);
  }
  sectors = sd.card()->sectorCount();
  Serial.printf("SD sectors: %d\n", sectors);

  // Инициализация USB MSC
  MSC.onStartStop(onStartStop);
  MSC.onRead(onRead);
  MSC.onWrite(onWrite);
  MSC.mediaPresent(true);
  MSC.begin(sectors, DISK_SECTOR_SIZE);
  USB.begin();

  // Настройка WebServer
  WiFi.softAP(ssid, password);
  server.on("/", handleRoot);
  server.on("/upload", HTTP_POST, handleFileUpload);
  server.on("/list", HTTP_GET, handleFileList);
  server.on("/read", HTTP_GET, handleFileRead);
  server.begin();

  // Настройка отображения информации на дисплее
  IPAddress IP = WiFi.softAPIP();
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(F("Wi-Fi Access Point"));
  display.printf("IP: %s\n", IP.toString().c_str());
  display.display();

  // Периодическая задача
  ticker.attach(1.0, printReadWriteCounter);
}

void loop() {
  server.handleClient();
}

// Реализация обработчиков WebServer
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

void handleFileUpload() {
  HTTPUpload &upload = server.upload();
  static File uploadFile;

  if (upload.status == UPLOAD_FILE_START) {
    String filename = "/" + upload.filename;
    uploadFile = SD.open(filename, FILE_WRITE);
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) uploadFile.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) uploadFile.close();
    server.send(200, "text/plain; charset=UTF-8", "Файл успешно загружен");
  }
}

void handleFileList() {
  File root = SD.open("/");
  String list = "<h2>Список файлов:</h2><ul>";
  File file = root.openNextFile();
  while (file) {
    list += "<li>" + String(file.name()) + "</li>";
    file = root.openNextFile();
  }
  list += "</ul>";
  server.send(200, "text/html; charset=UTF-8", list);
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
  String content;
  while (file.available()) content += char(file.read());
  file.close();
  server.send(200, "text/plain; charset=UTF-8", content);
}
