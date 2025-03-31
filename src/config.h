#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <Arduino.h>

#define GATEWAY_MAC "001122334455"      // Dia chi MAC cua Gateway, viet hoa, lien nhau, khong dau :

#define MAX_DEVICES 100     // So luong toi da thiet bi duoc quan ly boi Gateway

#define LORA_RX 17  // Chân RX của LoRa kết nối với TX ESP32
#define LORA_TX 16  // Chân TX của LoRa kết nối với RX ESP32

typedef enum {
    Report = 0,
    Get,
    Set
} ResourceType_t;

typedef enum {
    Bool = 0,
    Int8,
    Uint8,
    Int16,
    Uint16,
    Int32,
    Uint32,
    Int64,
    Uint64,
    Float,
    Double,
    Array
} ValueType_t;

typedef struct {
    String name;
    ValueType_t type;
    uint8_t size;
    uint8_t cmds[3];   // report, get, set
} ResourceInfo_t;

ResourceInfo_t Config_GetInfoByCmd(uint8_t cmd);
ResourceInfo_t Config_GetInfoByName(String name);

#endif
