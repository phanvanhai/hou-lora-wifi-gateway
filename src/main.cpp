#include <WiFiManager.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <Preferences.h>

#include "lora.h"

WiFiClient espClient;
PubSubClient mqttClient(espClient);
Preferences preferences;

#define EEPROM_SIZE 512       // Kich thuoc EEPROM
#define JSON_SIZE (10 * 1024) // Kich thuoc JSON

#define MQTT_BROKER_DEFAULT "broker.emqx.io"
#define MQTT_PORT_DEFAULT 1883

#define MQTT_PULISH_TOPIC_FORWARD ("gw/" + String(GATEWAY_MAC) + "/upforward")
#define MQTT_SUBSCRIBE_TOPIC_FORWARD ("gw/" + String(GATEWAY_MAC) + "/downforward")
#define MQTT_PULISH_TOPIC_CONFIG ("gw/" + String(GATEWAY_MAC) + "/upconfig")
#define MQTT_SUBSCRIBE_TOPIC_CONFIG ("gw/" + String(GATEWAY_MAC) + "/downconfig")

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
void frameToJson(const Frame_t &frame, String &jsonStr);
bool jsonToFrame(const String &jsonStr, Frame_t &frame);

void loadDevIds();
void saveDevIds();

void setup()
{
  Serial.begin(115200);
  delay(1000);
  loadDevIds();
  Config_Init();
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
      String jsonStr;
      frameToJson(frame, jsonStr);

      if (mqttClient.connected())
      {
        mqttClient.publish((MQTT_PULISH_TOPIC_FORWARD + "/" + devIdStr).c_str(), jsonStr.c_str());
        Serial.println("Publish: " + (MQTT_PULISH_TOPIC_FORWARD + "/" + devIdStr));
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
      mqttClient.subscribe(MQTT_SUBSCRIBE_TOPIC_CONFIG.c_str());
      mqttClient.subscribe(MQTT_SUBSCRIBE_TOPIC_FORWARD.c_str());
      Serial.println("Da ket noi MQTT");
      Serial.printf("Subscribe topic:%s\n", MQTT_SUBSCRIBE_TOPIC_CONFIG.c_str());
      Serial.printf("Subscribe topic:%s\n", MQTT_SUBSCRIBE_TOPIC_FORWARD.c_str());
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

void pushDevIdsToConfigTopic()
{
  StaticJsonDocument<JSON_SIZE> doc;
  JsonArray array = doc.createNestedArray("devIds");

  for (int i = 0; i < devIdCount; i++)
  {
    array.add(devIds[i]);
  }

  String jsonStr;
  serializeJson(doc, jsonStr);

  if (mqttClient.connected())
  {
    mqttClient.publish(MQTT_PULISH_TOPIC_CONFIG.c_str(), jsonStr.c_str());
    Serial.println("Publish: " + jsonStr);
  }
  else
  {
    Serial.println("Loi: MQTT chua ket noi, khong the gui danh sach devIds");
  }
}

void frameToJson(const Frame_t &frame, String &jsonStr)
{
  ArduinoJson::StaticJsonDocument<JSON_SIZE> doc;

  // Chuyển devId sang chuỗi HEX (viết hoa)
  char devIdStr[13]; // 12 ký tự HEX + null terminator
  sprintf(devIdStr, "%012llX", frame.devId);
  doc["devId"] = devIdStr;

  JsonArray commandsArray = doc.createNestedArray("commands");
  for (uint8_t i = 0; i < frame.commandSize; i++)
  {
    JsonObject cmdObj = commandsArray.createNestedObject();
    cmdObj["cmd"] = frame.commands[i].cmd;

    JsonArray dataArray = cmdObj.createNestedArray("data");
    for (uint8_t j = 0; j < frame.commands[i].dataSize; j++)
    {
      dataArray.add(frame.commands[i].data[j]);
    }
  }

  serializeJson(doc, jsonStr); // Xuất ra chuỗi JSON
}

bool jsonToFrame(const String &jsonStr, Frame_t &frame)
{
  ArduinoJson::StaticJsonDocument<JSON_SIZE> doc;
  DeserializationError error = deserializeJson(doc, jsonStr);
  if (error)
  {
    Serial.println("Loi JSON");
    return false;
  }

  // Chuyển devId từ HEX string sang uint64_t
  const char *devIdStr = doc["devId"];
  frame.devId = strtoull(devIdStr, NULL, 16); // Chuyển HEX string thành uint64_t

  JsonArray commandsArray = doc["commands"];
  frame.commandSize = min((uint8_t)commandsArray.size(), (uint8_t)MAX_COMMAND_SIZE);

  for (uint8_t i = 0; i < frame.commandSize; i++)
  {
    JsonObject cmdObj = commandsArray[i];
    frame.commands[i].cmd = cmdObj["cmd"];

    JsonArray dataArray = cmdObj["data"];
    if (dataArray.size() != Config_GetSizeByCmd(frame.commands[i].cmd))
    {
      Serial.printf("Error: Size cmd invalid: %d vs %d\n", dataArray.size(), Config_GetSizeByCmd(frame.commands[i].cmd));
      return false;
    }
    frame.commands[i].dataSize = Config_GetSizeByCmd(frame.commands[i].cmd);
    for (uint8_t j = 0; j < frame.commands[i].dataSize; j++)
    {
      frame.commands[i].data[j] = dataArray[j];
    }
  }

  return true;
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

  if (String(topic) == MQTT_SUBSCRIBE_TOPIC_CONFIG)
  {
    String message;
    for (unsigned int i = 0; i < length; i++)
    {
      message += (char)payload[i];
    }
    Serial.println("Noi dung: " + message);

    // Parse JSON
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, message);
    if (error)
    {
      Serial.println("Loi parse JSON");
      return;
    }

    JsonArray array = doc["devIds"];
    devIdCount = 0;

    for (JsonVariant value : array)
    {
      if (devIdCount < MAX_DEVICES)
      {
        devIds[devIdCount] = value.as<String>();
        devIdCount++;
      }
      else
      {
        break; // Giới hạn số lượng thiết bị
      }
    }

    // Lưu vào flash
    saveDevIds();
  }
  else if (String(topic) == MQTT_SUBSCRIBE_TOPIC_FORWARD)
  {
    String jsonStr = String((char*)payload).substring(0, length); 
    Frame_t frame;
    Serial.println(jsonStr);
    // Gọi jsonToFrame() để xử lý JSON
    if (jsonToFrame(jsonStr, frame)) {
        Lora_Send(frame);
    } else {
        Serial.println("Failed to parse JSON.");
    }
  }
}