#include <Arduino.h>
#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include "ArduinoJson.h"
#include "SPIFFS.h"
#include "MQTTBroker.h"

// WiFi AP configuration
#define AP_SSDI "AML Debug Dashboard"
#define AP_PASSWORD "aml305b4"
#define AP_LOCAL_IP 192, 168, 4, 1
#define AP_GATEWAY_IP 192, 168, 4, 1
#define AP_NETWORK_MASK 255, 255, 255, 0

// Constants
#define SERIAL_BAUD_RATE 115200
#define WEB_SERVER_PORT 80

// Global Variables
AsyncWebServer server(WEB_SERVER_PORT);
AsyncWebSocket ws("/ws");
MQTTBroker mqttBroker;

unsigned long previousMillis = 0;
unsigned long count = 0;

// Box structure
struct BoxInfo
{
  String name;
  int id;
};

std::vector<BoxInfo> boxes;

void handleSerialInput();
void loadBoxesFromSPIFFS();
void saveBoxesToSPIFFS();
void setupWebServer();

void setup()
{
  // Tăng ưu tiên cho CPU core xử lý Serial
  setCpuFrequencyMhz(240); // Đặt tần số CPU tối đa

  Serial.begin(SERIAL_BAUD_RATE);
  Serial.println("Booting up ESP32 Debug Dashboard");

  // Initialize SPIFFS
  if (!SPIFFS.begin(true))
  {
    Serial.println("SPIFFS error, continuing without persistent storage");
  }
  else
  {
    Serial.println("SPIFFS mounted successfully");
    // Load box configurations
    loadBoxesFromSPIFFS();
  }

  // Configure WiFi AP with efficient settings
  IPAddress AP_StaticIP(AP_LOCAL_IP);
  IPAddress AP_Gateway(AP_GATEWAY_IP);
  IPAddress AP_NetMask(AP_NETWORK_MASK);

  WiFi.setTxPower(WIFI_POWER_19_5dBm); // Tăng công suất phát WiFi
  WiFi.setSleep(false);                // Tắt chế độ sleep để giảm độ trễ

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_StaticIP, AP_Gateway, AP_NetMask);
  WiFi.softAP(AP_SSDI, AP_PASSWORD);

  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  // Start MQTT broker (hiệu suất cao)
  mqttBroker.begin();
  Serial.println("MQTT Broker running");

  // Configure WebSocket with low latency settings
  ws.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
             {
    if (type == WS_EVT_CONNECT) {
      Serial.printf("WebSocket client #%u connected\n", client->id());
      
      // Send initial box configuration
      DynamicJsonDocument doc(4096);
      doc["type"] = "boxes";
      JsonArray boxArray = doc["boxes"].to<JsonArray>();
      
      for (auto &box : boxes) {
        JsonObject boxObj = boxArray.createNestedObject();
        boxObj["name"] = box.name;
        boxObj["id"] = box.id;
        boxObj["value"] = "0"; // Default value
      }
      
      String jsonStr;
      serializeJson(doc, jsonStr);
      client->text(jsonStr);
    } });

  server.addHandler(&ws);

  // Setup web server routes
  setupWebServer();

  // Start web server
  server.begin();
  Serial.println("HTTP server started");
}

void loop()
{
  // Process MQTT broker - độ ưu tiên cao
  mqttBroker.loop();

  // Check for serial data - độ ưu tiên cao nhất
  handleSerialInput();

  // Clean up WebSocket clients - độ ưu tiên thấp
  static unsigned long lastCleanup = 0;
  if (millis() - lastCleanup > 30000)
  {
    ws.cleanupClients();
    lastCleanup = millis();
  }

  // Zero delay để tối ưu tốc độ đọc Serial
  yield();
}

// Tối ưu cho tốc độ thực
void handleSerialInput()
{
  if (Serial2.available())
  {
    String data = Serial2.readStringUntil('\r');
    data.trim();

    // Format: "id,value"
    int commaIndex = data.indexOf(',');
    if (commaIndex > 0)
    {
      String idStr = data.substring(0, commaIndex);
      String valueStr = data.substring(commaIndex + 1);

      int id = idStr.toInt();

      // Kiểm tra nhanh xem ID có tồn tại
      bool boxExists = false;
      for (auto &box : boxes)
      {
        if (box.id == id)
        {
          boxExists = true;
          break;
        }
      }

      if (boxExists)
      {
        // Gửi cập nhật ngay lập tức qua MQTT - độ trễ thấp nhất
        String topic = "dashboard/update/" + String(id);
        mqttBroker.publish(topic, valueStr, false);

        // Đồng thời gửi qua WebSocket - độ trễ thấp
        DynamicJsonDocument doc(64); // Giữ nhỏ cho hiệu suất cao
        doc["type"] = "update";
        doc["id"] = id;
        doc["value"] = valueStr;

        String wsMessage;
        serializeJson(doc, wsMessage);
        ws.textAll(wsMessage); // Gửi đến tất cả client
      }
    }
  }

  // if (millis() - previousMillis > 20)
  // {
  //   previousMillis = millis();
  //   count++;

  //   Serial.println(count);

  //   // Gửi cập nhật ngay lập tức qua MQTT - độ trễ thấp nhất
  //   String topic = "dashboard/update/" + String(1);
  //   mqttBroker.publish(topic, String(count), false);

  //   // Đồng thời gửi qua WebSocket - độ trễ thấp
  //   DynamicJsonDocument doc(64); // Giữ nhỏ cho hiệu suất cao
  //   doc["type"] = "update";
  //   doc["id"] = 1;
  //   doc["value"] = String(count);

  //   String wsMessage;
  //   serializeJson(doc, wsMessage);
  //   ws.textAll(wsMessage); // Gửi đến tất cả client

  // }
}

void setupWebServer()
{
  // Serve static files from SPIFFS
  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

  // Simple API to get box configurations
  server.on("/getBoxes", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    DynamicJsonDocument doc(4096);
    JsonArray boxArray = doc.to<JsonArray>();
    
    for (auto &box : boxes) {
      JsonObject boxObj = boxArray.createNestedObject();
      boxObj["name"] = box.name;
      boxObj["id"] = box.id;
      boxObj["value"] = "0"; // Default value
    }
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response); });

  // API to add a new box
  server.on("/addBox", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
            {
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, data, len);
      
      String name = doc["name"].as<String>();
      int id = doc["id"].as<int>();
      
      // Check if ID already exists
      bool idExists = false;
      for (auto &box : boxes) {
        if (box.id == id) {
          idExists = true;
          break;
        }
      }
      
      DynamicJsonDocument responseDoc(128);
      if (idExists) {
        responseDoc["success"] = false;
        responseDoc["message"] = "ID already exists";
      } else {
        BoxInfo newBox = {name, id};
        boxes.push_back(newBox);
        saveBoxesToSPIFFS();
        
        // Notify all clients of the new box
        DynamicJsonDocument notifyDoc(4096);
        notifyDoc["type"] = "boxes";
        JsonArray boxArray = notifyDoc["boxes"].to<JsonArray>();
        
        for (auto &box : boxes) {
          JsonObject boxObj = boxArray.createNestedObject();
          boxObj["name"] = box.name;
          boxObj["id"] = box.id;
          boxObj["value"] = "0"; // Default value
        }
        
        String jsonStr;
        serializeJson(notifyDoc, jsonStr);
        ws.textAll(jsonStr);
        
        responseDoc["success"] = true;
      }
      
      String response;
      serializeJson(responseDoc, response);
      request->send(200, "application/json", response); });

  // API to delete a box
  server.on("/deleteBox", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
            {
      DynamicJsonDocument doc(128);
      deserializeJson(doc, data, len);
      
      int id = doc["id"].as<int>();
      bool found = false;
      
      for (auto it = boxes.begin(); it != boxes.end(); ++it) {
        if (it->id == id) {
          boxes.erase(it);
          found = true;
          saveBoxesToSPIFFS();
          
          // Notify all clients of the updated box list
          DynamicJsonDocument notifyDoc(4096);
          notifyDoc["type"] = "boxes";
          JsonArray boxArray = notifyDoc["boxes"].to<JsonArray>();
          
          for (auto &box : boxes) {
            JsonObject boxObj = boxArray.createNestedObject();
            boxObj["name"] = box.name;
            boxObj["id"] = box.id;
            boxObj["value"] = "0"; // Default value
          }
          
          String jsonStr;
          serializeJson(notifyDoc, jsonStr);
          ws.textAll(jsonStr);
          
          break;
        }
      }
      
      DynamicJsonDocument responseDoc(128);
      responseDoc["success"] = found;
      
      String response;
      serializeJson(responseDoc, response);
      request->send(200, "application/json", response); });
}

void loadBoxesFromSPIFFS()
{
  boxes.clear();

  if (SPIFFS.exists("/boxes.json"))
  {
    File file = SPIFFS.open("/boxes.json", "r");
    if (file)
    {
      DynamicJsonDocument doc(4096);
      DeserializationError error = deserializeJson(doc, file);
      file.close();

      if (!error)
      {
        JsonArray boxArray = doc["boxes"].as<JsonArray>();
        for (JsonObject boxObj : boxArray)
        {
          BoxInfo box;
          box.name = boxObj["name"].as<String>();
          box.id = boxObj["id"].as<int>();
          boxes.push_back(box);
        }
        Serial.printf("Loaded %d boxes from SPIFFS\n", boxes.size());
      }
    }
  }
}

void saveBoxesToSPIFFS()
{
  DynamicJsonDocument doc(4096);
  JsonArray boxArray = doc["boxes"].to<JsonArray>();

  for (auto &box : boxes)
  {
    JsonObject boxObj = boxArray.createNestedObject();
    boxObj["name"] = box.name;
    boxObj["id"] = box.id;
  }

  File file = SPIFFS.open("/boxes.json", "w");
  if (file)
  {
    serializeJson(doc, file);
    file.close();
    Serial.println("Boxes saved to SPIFFS");
  }
}