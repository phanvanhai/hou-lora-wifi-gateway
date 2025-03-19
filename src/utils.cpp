#include "utils.h"
#include <ArduinoJson.h>
#include <stdint.h>

// Hàm đảo ngược byte (Endianness Swap)
template <typename T>
void swapEndian(T &value)
{
    uint8_t *ptr = reinterpret_cast<uint8_t *>(&value);
    for (size_t i = 0; i < sizeof(T) / 2; i++)
    {
        uint8_t tmp = ptr[i];
        ptr[i] = ptr[sizeof(T) - 1 - i];
        ptr[sizeof(T) - 1 - i] = tmp;
    }
}

// Chuyển dữ liệu từ JSON thành mảng uint8_t (Big Endian)
void convertToByteArray(JsonVariant value, ValueType_t type, uint8_t *data, size_t length)
{
    switch (type)
    {
    case Bool:
        data[0] = value.as<bool>() ? 1 : 0;
        break;
    case Int8:
    {
        int8_t v = value.as<int8_t>();
        memcpy(data, &v, sizeof(int8_t));
        break;
    }
    case Uint8:
        data[0] = value.as<uint8_t>();
        break;
    case Int16:
    {
        int16_t v = value.as<int16_t>();
        swapEndian(v);
        memcpy(data, &v, sizeof(int16_t));
        break;
    }
    case Uint16:
    {
        uint16_t v = value.as<uint16_t>();
        swapEndian(v);
        memcpy(data, &v, sizeof(uint16_t));
        break;
    }
    case Int32:
    {
        int32_t v = value.as<int32_t>();
        swapEndian(v);
        memcpy(data, &v, sizeof(int32_t));
        break;
    }
    case Uint32:
    {
        uint32_t v = value.as<uint32_t>();
        swapEndian(v);
        memcpy(data, &v, sizeof(uint32_t));
        break;
    }
    case Int64:
    {
        int64_t v = value.as<int64_t>();
        swapEndian(v);
        memcpy(data, &v, sizeof(int64_t));
        break;
    }
    case Uint64:
    {
        uint64_t v = value.as<uint64_t>();
        swapEndian(v);
        memcpy(data, &v, sizeof(uint64_t));
        break;
    }
    case Float:
    {
        float v = value.as<float>();
        swapEndian(v);
        memcpy(data, &v, sizeof(float));
        break;
    }
    case Double:
    {
        double v = value.as<double>();
        swapEndian(v);
        memcpy(data, &v, sizeof(double));
        break;
    }
    case Array:
    {
        JsonArray arr = value.as<JsonArray>();
        size_t i = 0;
        for (JsonVariant v : arr)
        {
            if (i >= length)
                break;
            data[i++] = v.as<uint8_t>();
        }
        break;
    }
    }
}

// Hàm chuyển đổi từ JSON sang mảng uint8_t
void convertToObject(JsonVariant &value, ValueType_t type, const uint8_t *data, size_t length)
{
    switch (type)
    {
    case Bool:
        value.set(data[0] != 0);
        break;
    case Int8:
    {
        int8_t v;
        memcpy(&v, data, sizeof(int8_t));
        value.set(v);
        break;
    }
    case Uint8:
        value.set(data[0]);
        break;
    case Int16:
    {
        int16_t v;
        memcpy(&v, data, sizeof(int16_t));
        swapEndian(v);
        value.set(v);
        break;
    }
    case Uint16:
    {
        uint16_t v;
        memcpy(&v, data, sizeof(uint16_t));
        swapEndian(v);
        value.set(v);
        break;
    }
    case Int32:
    {
        int32_t v;
        memcpy(&v, data, sizeof(int32_t));
        swapEndian(v);
        value.set(v);
        break;
    }
    case Uint32:
    {
        uint32_t v;
        memcpy(&v, data, sizeof(uint32_t));
        swapEndian(v);
        value.set(v);
        break;
    }
    case Int64:
    {
        int64_t v;
        memcpy(&v, data, sizeof(int64_t));
        swapEndian(v);
        value.set(v);
        break;
    }
    case Uint64:
    {
        uint64_t v;
        memcpy(&v, data, sizeof(uint64_t));
        swapEndian(v);
        value.set(v);
        break;
    }
    case Float:
    {
        float v;
        memcpy(&v, data, sizeof(float));
        swapEndian(v);
        value.set(v);
        break;
    }
    case Double:
    {
        double v;
        memcpy(&v, data, sizeof(double));
        swapEndian(v);
        value.set(v);
        break;
    }
    case Array:
    {
        JsonArray array = value.to<JsonArray>();
        for (size_t i = 0; i < length; i++)
        {
            array.add(data[i]);
        }
        break;
    }
    default:
        value.set(nullptr);
    }
}

bool jsonToFrame(const String &jsonStr, Frame_t &frame, bool isGetReq)
{
    JsonDocument doc;

    DeserializationError error = deserializeJson(doc, jsonStr);
    if (error)
    {
        Serial.println("Loi JSON");
        return false;
    }

    // Chuyển devId từ HEX string sang uint64_t
    const char *devIdStr = doc["devId"];
    frame.devId = strtoull(devIdStr, NULL, 16); // Chuyển HEX string thành uint64_t

    // Kiểm tra key "resource"
    if (!doc.containsKey("resources"))
    {
        Serial.println("Key 'resources' không tồn tại!");
        return false;
    }

    frame.commandSize = 0;
    int i = 0;

    if (isGetReq)
    {

        // Trích xuất mảng từ key "resource"
        JsonArray arr = doc["resources"].as<JsonArray>();

        // Duyệt mảng JSON
        Serial.println("Duyệt JSON Array từ key 'resources':");
        for (JsonVariant v : arr)
        {
            String resourceName = v.as<String>();
            ResourceInfo_t info = Config_GetInfoByName(resourceName);
            frame.commands[i].cmd = info.cmds[ResourceType_t::Get];
            frame.commands[i].dataSize = 0;
            frame.commandSize++;

            i++;
        }
    }
    else
    {
        JsonObject obj = doc["resources"].as<JsonObject>();
        for (JsonPair kv : obj)
        {
            String resourceName = kv.key().c_str();
            ResourceInfo_t info = Config_GetInfoByName(resourceName);
            frame.commands[i].cmd = info.cmds[ResourceType_t::Set];
            frame.commands[i].dataSize = info.size;

            convertToByteArray(obj[resourceName], info.type, frame.commands[i].data, info.size);

            i++;
            frame.commandSize++;

            Serial.print(resourceName); // In key
            Serial.print(": ");
            Serial.println(kv.value().as<String>()); // In value dạng chuỗi
        }
    }

    return true;
}

void frameToJson(const Frame_t &frame, String &jsonStr)
{
    JsonDocument doc;

    // Chuyển devId sang chuỗi HEX (viết hoa)
    char devIdStr[13]; // 12 ký tự HEX + null terminator
    sprintf(devIdStr, "%012llX", frame.devId);
    doc["devId"] = devIdStr;

    JsonObject commandsObj = doc.createNestedObject("resources");
    for (uint8_t i = 0; i < frame.commandSize; i++)
    {
        ResourceInfo_t info = Config_GetInfoByCmd(frame.commands[i].cmd);
        JsonVariant value; // Trỏ đến phần tử trong JSON
        if (value.isNull())
        {
            if (info.type == Array)
            {
                value = commandsObj.createNestedArray(info.name); // Đảm bảo mảng tồn tại
            }
            else
            {
                value = commandsObj.createNestedObject(info.name); // Đảm bảo object tồn tại
            }
        }

        convertToObject(value, info.type, frame.commands[i].data, info.size);
    }

    serializeJson(doc, jsonStr); // Xuất ra chuỗi JSON
}

// Hàm chuyển danh sách cấu hình thành JSON string
void convertConfigToJson(const String configList[], int configCount, String &jsonStr)
{
    JsonDocument doc;

    JsonArray array = doc.createNestedArray("devIds");

    for (int i = 0; i < configCount; i++)
    {
        array.add(configList[i]);
    }

    serializeJson(doc, jsonStr);
}

// Hàm chuyển JSON string thành danh sách cấu hình
bool convertJsonToConfig(const String &jsonStr, String configList[], int &configCount, int maxConfig)
{
    JsonDocument doc;

    DeserializationError error = deserializeJson(doc, jsonStr);

    if (error)
    {
        Serial.println("Lỗi parse JSON");
        return false;
    }

    JsonArray array = doc["devIds"];
    configCount = 0;

    for (JsonVariant value : array)
    {
        if (configCount < maxConfig)
        {
            configList[configCount] = value.as<String>();
            configCount++;
        }
        else
        {
            break; // Giới hạn số lượng cấu hình
        }
    }

    return true; // Chuyển đổi thành công
}