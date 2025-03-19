#include <WiFiManager.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <Preferences.h>

#include "utils.h"
#include "lora.h"

WiFiClient espClient;
PubSubClient mqttClient(espClient);
Preferences preferences;

#define MQTT_BROKER_DEFAULT "broker.emqx.io"
#define MQTT_PORT_DEFAULT 1883

#define MQTT_PULISH_TOPIC_RESOURCES ("gw/" + String(GATEWAY_MAC) + "/report/resources")
#define MQTT_SUBSCRIBE_TOPIC_GET_RESOUCES ("gw/" + String(GATEWAY_MAC) + "/get/resources")
#define MQTT_SUBSCRIBE_TOPIC_SET_RESOUCES ("gw/" + String(GATEWAY_MAC) + "/set/resources")
#define MQTT_PULISH_TOPIC_DEVLIST ("gw/" + String(GATEWAY_MAC) + "/report/devlist")
#define MQTT_SUBSCRIBE_TOPIC_DEVLIST ("gw/" + String(GATEWAY_MAC) + "/set/devlist")

String devIds[MAX_DEVICES]; // Mảng lưu devId
int devIdCount = 0;         // Số lượng thiết bị hiện tại

struct Config
{
  char mqttBroker[50];
  int mqttPort;
};

void mqttCallback(char *topic, byte *payload, unsigned int length);
void readConfig(Config &config);
void saveConfig(const Config &config);
void connectMQTT(const Config &config);
void pushDevIdsToConfigTopic();

void loadDevIds();
void saveDevIds();

void setup()
{
  Serial.begin(115200);
  delay(1000);
  loadDevIds();
  Lora_Init();
  Config config;

  // Doc cau hinh MQTT tu EEPROM
  readConfig(config);

  // Su dung WiFiManager de cau hinh WiFi va MQTT
  WiFiManager wm;
  WiFiManagerParameter mqttBrokerParam("mqttBroker", "MQTT Broker", config.mqttBroker, 50);
  WiFiManagerParameter mqttPortParam("mqttPort", "MQTT Port", String(config.mqttPort).c_str(), 6);
  wm.addParameter(&mqttBrokerParam);
  wm.addParameter(&mqttPortParam);

  if (!wm.autoConnect("Gateway_Config", "12345678"))
  {
    Serial.println("Ket noi WiFi that bai, khoi dong lai!");
    ESP.restart();
  }

  Serial.println("WiFi da ket noi!");

  // Luu cau hinh MQTT moi tu Web Config
  strlcpy(config.mqttBroker, mqttBrokerParam.getValue(), sizeof(config.mqttBroker));
  config.mqttPort = atoi(mqttPortParam.getValue());
  saveConfig(config);

  // Ket noi MQTT
  connectMQTT(config);
}

Frame_t test_frame = {
    .devId = 0xAABBCCDDEE01,
    .commands = {
        {0x11, {30}, 1},
        {0x12, {80}, 1},
        {0x13, {70}, 1},
        {0x14, {1, 200}, 2},
        {0x30, {1}, 1},
        {0x33, {0}, 1}},
    .commandSize = 6};
unsigned long previousMillis = 0;
const long interval = 3000; // 3 giây
unsigned long currentMillis = 0;

void loop()
{
  if (!mqttClient.connected())
  {
    Config config;
    readConfig(config);
    connectMQTT(config);
  }
  mqttClient.loop();

  Frame_t frame;
  if (Lora_Receive(&frame))
  {
    char devIdChars[13]; // 12 ký tự HEX + null terminator
    sprintf(devIdChars, "%012llX", frame.devId);
    String devIdStr = String(devIdChars);

    bool isExist = false;
    for (int i = 0; i < devIdCount; i++)
    {
      if (devIds[i] == devIdStr)
      {
        isExist = true;
        break;
      }
    }

    if (!isExist)
    {
      Serial.println("Ignore frame from: " + devIdStr);
    }
    else
    {
      if (mqttClient.connected())
      {
        String jsonStr;
        frameToJson(frame, jsonStr);

        mqttClient.publish((MQTT_PULISH_TOPIC_RESOURCES + "/" + devIdStr).c_str(), jsonStr.c_str());
        Serial.println("Publish: " + (MQTT_PULISH_TOPIC_RESOURCES + "/" + devIdStr));
        Serial.println(jsonStr);
      }
    }
  }

  // currentMillis = millis();
  // if (currentMillis - previousMillis >= interval)
  // {
  //   previousMillis = currentMillis;
  //   Lora_Send(test_frame);
  // }
}

// Doc du lieu tu EEPROM
void readConfig(Config &config)
{
  EEPROM.begin(EEPROM_SIZE);
  String jsonStr;
  for (int i = 0; i < EEPROM_SIZE; i++)
  {
    char ch = EEPROM.read(i);
    if (ch == '\0')
      break;
    jsonStr += ch;
  }
  EEPROM.end();

  ArduinoJson::StaticJsonDocument<JSON_SIZE> doc;
  DeserializationError error = deserializeJson(doc, jsonStr);
  if (!error)
  {
    if (doc.containsKey("mqttBroker"))
    {
      strlcpy(config.mqttBroker, doc["mqttBroker"], sizeof(config.mqttBroker));
    }
    else
    {
      strlcpy(config.mqttBroker, MQTT_BROKER_DEFAULT, sizeof(config.mqttBroker));
    }
    config.mqttPort = doc.containsKey("mqttPort") ? doc["mqttPort"].as<int>() : MQTT_PORT_DEFAULT;
  }
  else
  {
    strlcpy(config.mqttBroker, MQTT_BROKER_DEFAULT, sizeof(config.mqttBroker));
    config.mqttPort = MQTT_PORT_DEFAULT;
  }
}

// Ghi du lieu vao EEPROM
void saveConfig(const Config &config)
{
  ArduinoJson::StaticJsonDocument<JSON_SIZE> doc;
  doc["mqttBroker"] = config.mqttBroker;
  doc["mqttPort"] = config.mqttPort;

  String jsonStr;
  serializeJson(doc, jsonStr);

  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < jsonStr.length(); i++)
  {
    EEPROM.write(i, jsonStr[i]);
  }
  EEPROM.write(jsonStr.length(), '\0'); // Ket thuc chuoi
  EEPROM.commit();
  EEPROM.end();
}

// Ket noi MQTT
void connectMQTT(const Config &config)
{
  mqttClient.setServer(config.mqttBroker, config.mqttPort);
  mqttClient.setCallback(mqttCallback);
  while (!mqttClient.connected())
  {
    Serial.print("Ket noi MQTT...");
    String clientID = "LoraWifiGW_" + String(random(1000, 9999));
    if (mqttClient.connect(clientID.c_str()))
    {
      mqttClient.subscribe(MQTT_SUBSCRIBE_TOPIC_DEVLIST.c_str());
      mqttClient.subscribe(MQTT_SUBSCRIBE_TOPIC_GET_RESOUCES.c_str());
      mqttClient.subscribe(MQTT_SUBSCRIBE_TOPIC_SET_RESOUCES.c_str());
      Serial.println("Da ket noi MQTT");
      Serial.printf("Subscribe topic:%s\n", MQTT_SUBSCRIBE_TOPIC_DEVLIST.c_str());
      Serial.printf("Subscribe topic:%s\n", MQTT_SUBSCRIBE_TOPIC_GET_RESOUCES.c_str());
      Serial.printf("Subscribe topic:%s\n", MQTT_SUBSCRIBE_TOPIC_SET_RESOUCES.c_str());
      pushDevIdsToConfigTopic();
    }
    else
    {
      Serial.print("Loi: ");
      Serial.println(mqttClient.state());
      delay(2000);
    }
  }
}

// Hàm gửi danh sách devId qua MQTT
void pushDevIdsToConfigTopic()
{
  String jsonStr;
  convertConfigToJson(devIds, devIdCount, jsonStr); // Chuyển đổi danh sách devId thành JSON

  if (mqttClient.connected())
  {
    mqttClient.publish(MQTT_PULISH_TOPIC_DEVLIST.c_str(), jsonStr.c_str());
    Serial.println("Publish: " + jsonStr);
  }
  else
  {
    Serial.println("Lỗi: MQTT chưa kết nối, không thể gửi danh sách devIds");
  }
}

// Hàm lưu devId vào Flash
void saveDevIds()
{
  preferences.begin("config", false);
  preferences.putInt("devIdCount", devIdCount);

  for (int i = 0; i < devIdCount; i++)
  {
    String key = "devId_" + String(i);
    Serial.print("Key: ");
    Serial.print(key);
    Serial.print(" | Value: ");
    Serial.println(devIds[i]);

    preferences.putString(("devId_" + String(i)).c_str(), devIds[i]);
  }

  preferences.end();
  Serial.println("Da luu devId vao Flash");
}

// Hàm đọc devId từ Flash
void loadDevIds()
{
  preferences.begin("config", true);
  devIdCount = preferences.getInt("devIdCount", 0);

  Serial.print("So luong devId doc duoc: ");
  Serial.println(devIdCount);

  for (int i = 0; i < devIdCount; i++)
  {
    String key = "devId_" + String(i);
    devIds[i] = preferences.getString(key.c_str(), "");

    Serial.print("Key: ");
    Serial.print(key);
    Serial.print(" | Value: ");
    Serial.println(devIds[i]);
  }

  preferences.end();

  Serial.println("Danh sach devId tu Flash:");
  for (int i = 0; i < devIdCount; i++)
  {
    Serial.println(devIds[i]);
  }
}

// Callback khi nhận tin nhắn từ MQTT
void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  Serial.println("Nhan du lieu MQTT tu topic: " + String(topic));
  String jsonStr = String((char *)payload).substring(0, length);
  Serial.println(jsonStr);

  if (String(topic) == MQTT_SUBSCRIBE_TOPIC_DEVLIST)
  {
    if (convertJsonToConfig(jsonStr, devIds, devIdCount, MAX_DEVICES))
    {
      Serial.println("Danh sách thiết bị:");
      for (int i = 0; i < devIdCount; i++)
      {
        Serial.println(devIds[i]);
      }

      // Lưu vào flash
      saveDevIds();
    }
    else
    {
      Serial.println("Chuyển đổi JSON thất bại!");
    }
  }
  else if (String(topic) == MQTT_SUBSCRIBE_TOPIC_SET_RESOUCES)
  {
    Frame_t frame;
    Serial.println(jsonStr);
    // Gọi jsonToFrame() để xử lý JSON
    if (jsonToFrame(jsonStr, frame, false))
    {
      Lora_Send(frame);
    }
    else
    {
      Serial.println("Failed to parse JSON.");
    }
  }
  else if (String(topic) == MQTT_SUBSCRIBE_TOPIC_GET_RESOUCES)
  {
    Frame_t frame;
    Serial.println(jsonStr);
    // Gọi jsonToFrame() để xử lý JSON
    if (jsonToFrame(jsonStr, frame, true))
    {
      Lora_Send(frame);
    }
    else
    {
      Serial.println("Failed to parse JSON.");
    }
  }
}
