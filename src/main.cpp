#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include "esp_system.h"
#include "SdFat.h"

#include "pins.h"

#include "htmlTemplate.h"
#include "cssStyle.h"
#include "jsScript.h"
 
// Твои настройки Wi-Fi
const char* ssid = "TP-Link_1B4F";
const char* password = "33946597";
 
WebServer server(80);
 
// --- SdFat вместо встроенной SD.h ---
// SHARED_SPI - на шине помимо SD-карты есть ещё устройство (дисплей ILI9488),
// поэтому SdFat перед каждой транзакцией сама настраивает шину под SD
// и не считает её "своей" монопольно.
#define SD_CONFIG SdSpiConfig(SD_CS, SHARED_SPI, SD_SCK_MHZ(4), &SPI)
 
SdFs sd;
FsFile uploadFile;
  
// Экранирует строку для безопасной вставки в JSON. Это НЕ лечит саму
// причину (шум на SPI-шине бьёт байты имён файлов), а лишь не даёт одному
// битому символу сломать JSON целиком и обнулить весь список на сайте.
String jsonEscape(const String& in) 
{
    String out;
    out.reserve(in.length() + 8);
    for (size_t i = 0; i < in.length(); i++) {
        uint8_t c = (uint8_t)in[i];
        if (c == '"' || c == '\\') {
            out += '\\';
            out += (char)c;
        } else if (c < 0x20) {
            // Управляющий байт - точно испорченные данные (шум на SPI),
            // заменяем на видимый маркер вместо ломания JSON
            char buf[8];
            snprintf(buf, sizeof(buf), "\\u%04X", c);
            out += buf;
        } else {
            out += (char)c;
        }
    }
    return out;
}

bool removeRecursive(const String& basePath)
{
    FsFile dir = sd.open(basePath);

    if (!dir) return false;

    if (!dir.isDirectory()) 
    {
        dir.close();
        return sd.remove(basePath.c_str());
    }
    
    FsFile entry;
    while(entry.openNext(&dir, O_RDONLY))
    {
        char nameBuf[128];
        entry.getName(nameBuf, sizeof(nameBuf));

        String path = basePath;
        if (!path.endsWith("/")) path += "/";
        path += String(nameBuf);

        bool isDir = entry.isDirectory();
        entry.close();

        if (isDir)
        {
            if (!removeRecursive(path))
            {
                dir.close();
                return false;
            }
        }
        else
        {
            if (!sd.remove(path.c_str()))
            {
                dir.close();
                return false;
            }
        }
    }
    dir.close();

    return sd.rmdir(basePath.c_str());
}

void PrintDirectory(const String& path)
{ 
    FsFile dir = sd.open(path);
    if (!dir || !dir.isDirectory())
    {
        server.send(400, "application/json", "{\"error\":\"Not a directory\"}");
        return;
    }

    FsFile entry;
    String json = "[";
    bool first = true;

    while(entry.openNext(&dir, O_RDONLY))
    {
        char nameBuf[128];
        entry.getName(nameBuf, sizeof(nameBuf));
        String fileName = String(nameBuf); 

        String fullPath = path;
        if (!fullPath.endsWith("/")) fullPath += "/";
                fullPath += fileName;

        bool looksCorrupted = false;
        for (size_t i = 0; i < strlen(nameBuf); i++) 
            if ((uint8_t)nameBuf[i] < 0x20) { looksCorrupted = true; break; } 

        if (looksCorrupted)
            Serial.printf("ВНИМАНИЕ: повреждённое имя файла в директории (шум на SPI): %s\n", fullPath.c_str());

        if (!first) json += ",";

        json += "{\"name\":\"" + jsonEscape(fileName) + 
                "\",\"path\":\"" + jsonEscape(fullPath) + 
                "\",\"size\":" + String(entry.fileSize()) +
                ",\"corrupted\":" + (looksCorrupted ? "true" : "false") + 
                ",\"isDir\":" + (entry.isDirectory() ? "true" : "false") + "}";

        first = false;
        entry.close();
    }
    dir.close();
    json += "]";
    server.send(200, "application/json", json);
}
 
void handleUpload() 
{
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        // Отключаем Nagle и держим соединение "живым" на время долгой заливки
        server.client().setNoDelay(true);
 
        String filename = upload.filename;
        if (!filename.startsWith("/")) filename = "/" + filename;
        Serial.printf("Начало загрузки: %s\n", filename.c_str());
 
        if (sd.exists(filename.c_str())) sd.remove(filename.c_str());
        uploadFile = sd.open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC);
        if (!uploadFile) {
            Serial.println("ОШИБКА: не удалось открыть файл для записи (SD переполнена/повреждена?)");
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (uploadFile) {
            size_t written = uploadFile.write(upload.buf, upload.currentSize);
            if (written != upload.currentSize) {
                Serial.printf("ОШИБКА ЗАПИСИ: записано %u из %u байт (SD карта не успевает / переполнена)\n",
                              (unsigned)written, (unsigned)upload.currentSize);
            }
        }
        // Отдаём процессорное время фоновым задачам (Wi-Fi стек, watchdog),
        // чтобы долгая серия записей на SD не приводила к зависанию/сбросу
        yield();
    } else if (upload.status == UPLOAD_FILE_END) {
        if (uploadFile) {
            uploadFile.close();
            Serial.printf("Загрузка завершена! Итоговый размер: %u байт\n", upload.totalSize);
        }
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
        Serial.println("Загрузка прервана клиентом/сетью");
        if (uploadFile) uploadFile.close();
    }
}

void readSD()
{
    const char* testPath = "/DCIM/100CANON/IMG_0001.JPG";
    FsFile f = sd.open(testPath, O_RDONLY);
    if (f) {
        size_t fsize = f.fileSize();
        uint8_t tbuf[512];
        size_t totalRead = 0;
        while (totalRead < fsize) {
            size_t toRead = min(sizeof(tbuf), fsize - totalRead);
            int n = f.read(tbuf, toRead);
            if (n <= 0) {
                Serial.printf("[ТЕСТ SdFat] ЧТЕНИЕ ОСТАНОВИЛОСЬ на %u/%u байт\n", (unsigned)totalRead, (unsigned)fsize);
                Serial.printf("[ТЕСТ SdFat] Код ошибки SD: 0x%02X, данные ошибки: 0x%02X\n",
                                sd.sdErrorCode(), sd.sdErrorData());
                break;
            }
            totalRead += n;
        }
        if (totalRead == fsize) {
            Serial.printf("[ТЕСТ SdFat] УСПЕХ: прочитан весь файл, %u байт\n", (unsigned)totalRead);
        }
        f.close();
    } else {
        Serial.println("[ТЕСТ SdFat] не удалось открыть тестовый файл");
    }
}

void handleList()
{
    String path = server.hasArg("dir") ? server.arg("dir") : "/";
    PrintDirectory(path);
}

String getContentType(String filename) 
{
    filename.toLowerCase();

    if (filename.endsWith(".html") || filename.endsWith(".htm")) return "text/html";
    else if (filename.endsWith(".css")) return "text/css";
    else if (filename.endsWith(".js")) return "text/javascript";
    else if (filename.endsWith(".png")) return "image/png";
    else if (filename.endsWith(".gif")) return "image/gif";
    else if (filename.endsWith(".jpg") || filename.endsWith(".jpeg")) return "image/jpeg";
    else if (filename.endsWith(".txt") || filename.endsWith(".json") || filename.endsWith(".log")) return "text/plain";

    return "application/octet-stream"; // Для остальных типов файлов (бинарники, архивы и т.д.)
}

void handleDownload()
{
    if (!server.hasArg("file")) {
        server.send(400, "text/plain", "Bad Request");
        return;
    }
    String path = server.arg("file");
    if (!sd.exists(path.c_str())) {
        server.send(404, "text/plain", "File Not Found");
        return;
    }

    uint32_t t0 = millis();
    FsFile dataFile = sd.open(path.c_str(), O_RDONLY);
    if (!dataFile) {
        Serial.printf("ОШИБКА: не удалось открыть на чтение %s\n", path.c_str());
        server.send(500, "text/plain", "Failed to open file");
        return;
    }
    Serial.printf("Открытие файла на чтение заняло %lu мс\n", millis() - t0);

    // Подсказываем браузеру настоящее имя файла (иначе он сохранит
    // просто как "download" без расширения)
    String baseName = path;
    int slashPos = baseName.lastIndexOf('/');
    if (slashPos >= 0) baseName = baseName.substring(slashPos + 1);

    if (server.hasArg("dl") && server.arg("dl") == "1") {
        server.sendHeader("Content-Disposition", "attachment; filename=\"" + baseName + "\"");
    } else {
        server.sendHeader("Content-Disposition", "inline; filename=\"" + baseName + "\"");
    }

    WiFiClient client = server.client();
    client.setNoDelay(true);

    size_t fileSize = dataFile.fileSize();
    server.setContentLength(fileSize);
    server.send(200, getContentType(baseName), "");

    static uint8_t buf[4096];
    size_t sent = 0;
    uint32_t lastLog = millis();
    while (sent < fileSize) {
        size_t toRead = min((size_t)sizeof(buf), fileSize - sent);
        uint32_t tRead0 = millis();
        int n = dataFile.read(buf, toRead);
        uint32_t readMs = millis() - tRead0;

        if (n <= 0) {
            Serial.printf("ЧТЕНИЕ SD ВЕРНУЛО 0 БАЙТ на позиции %u/%u\n", (unsigned)sent, (unsigned)fileSize);
            break;
        }
        if (readMs > 200) {
            Serial.printf("МЕДЛЕННОЕ ЧТЕНИЕ SD: %lu мс на %d байт (смещение %u)\n",
                            readMs, n, (unsigned)sent);
        }
        if (!client.connected()) {
            Serial.printf("КЛИЕНТ ОТКЛЮЧИЛСЯ на %u/%u байт\n", (unsigned)sent, (unsigned)fileSize);
            break;
        }

        size_t w = client.write(buf, n);
        if (w != (size_t)n) {
            Serial.printf("ОШИБКА ОТПРАВКИ: %u из %d байт, позиция %u\n", (unsigned)w, n, (unsigned)sent);
            break;
        }

        sent += w;
        if (millis() - lastLog > 3000) {
            Serial.printf("Прогресс скачивания %s: %u/%u байт\n", path.c_str(), (unsigned)sent, (unsigned)fileSize);
            lastLog = millis();
        }
        yield();
    }
    dataFile.close();
    Serial.printf("Скачивание завершено: %s -> %u/%u байт\n", path.c_str(), (unsigned)sent, (unsigned)fileSize);

}

void handleDeletion()
{
    if (!server.hasArg("file") && !server.hasArg("path")) 
    {
        server.send(400, "text/plain", "Bad Request: Missing 'path' parameter");
        return;
    }

    String path = server.hasArg("path") ? server.arg("path") : server.arg("file");

    if (path == "/" || path.isEmpty())
    {
        server.send(403, "text/plain", "Forbidden: Cannot delete root directory");
        return;
    }

    if (!sd.exists(path.c_str()))
    {
        server.send(404, "text/plain", "File or Directory Not Found");
        return;
    }

    if (removeRecursive(path))
        server.send(200, "text/plain", "OK");
    else
        server.send(500, "text/plain", "Failed to delete item");
}

void handleRename()
{
    if (server.hasArg("from") && server.hasArg("to")) {
        if (sd.rename(server.arg("from").c_str(), server.arg("to").c_str())) {
            server.send(200, "text/plain", "OK");
        } else {
            server.send(500, "text/plain", "Failed");
        }
    }
}

void handleMKDIR()
{
    if (!server.hasArg("path"))
    {
        server.send(400, "text/plain", "Missing path argument");
        return;
    }

    String path = server.arg("path");

    if (path == "/" || path.isEmpty())
    {
        server.send(400, "text/plain", "Invalid path");
        return;
    }

    if (sd.mkdir(path.c_str()))
        server.send(200, "text/plain", "ok");
    else
        server.send(500, "text/plain", "Failed to create a directory");
}

void handleServerMethods()
{
    server.on("/", HTTP_GET, []
        { server.send(200, "text/html", htmlTemplate); });
 
    server.on("/style.css", HTTP_GET, []
        { server.send(200, "text/css", cssStyle); });

    server.on("/script.js", HTTP_GET, []
        { server.send(200, "application/javascript", jsScript); });
    
    server.on("/list", HTTP_GET, handleList);
 
    server.on("/download", HTTP_GET, handleDownload);
 
    server.on("/delete", HTTP_DELETE, handleDeletion);
 
    server.on("/rename", HTTP_POST, handleRename);

    server.on("/mkdir", HTTP_POST, handleMKDIR);
 
    server.on("/upload", HTTP_POST, []() { // handle upload file
        server.send(200, "text/plain", "OK");
    }, handleUpload);

}

void setup() 
{
    Serial.begin(115200);
    delay(200);
    Serial.println("\n--- Запуск ESP32 SD Web Server (SdFat) ---");
 
    // Serial.printf("Причина последнего сброса: %d (см. esp_reset_reason_t)\n", (int)esp_reset_reason());
 
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    if (!sd.begin(SD_CONFIG)) {
        Serial.println("ОШИБКА: SD-карта не найдена или не отформатирована в FAT32/exFAT!");
        sd.initErrorHalt(&Serial);
    }
    Serial.println("SD-карта успешно смонтирована (SdFat).");
 
    // ДИАГНОСТИКА: встроенный листинг SdFat в обход нашего кода /list -
    // если файлы тут видны, значит данные на карте целы, а баг в printDirectory
    // Serial.println("--- Встроенный листинг SdFat (ls -R) ---");
    // sd.ls(&Serial, "/", LS_R | LS_SIZE | LS_DATE);
    // Serial.println("--- Конец листинга ---");
 
    // Контрольный тест: читаем файл целиком ДО запуска Wi-Fi той же
    // проблемной точкой (8192 байт), которая ломала встроенную SD.h
    // readSD();
 
    WiFi.begin(ssid, password);
    Serial.print("Подключение к Wi-Fi ");
 
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
 
    Serial.println("\nУспешно!");
    WiFi.setSleep(false);
 
    Serial.print("Открой в браузере: http://");
    Serial.println(WiFi.localIP());
 
    handleServerMethods();

    server.begin();
    Serial.println("Веб-сервер запущен и готов к работе.");
}
 
void loop() 
{
    server.handleClient();
}