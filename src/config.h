#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <Arduino.h>

#define GATEWAY_MAC "001122334455"      // Dia chi MAC cua Gateway, viet hoa, lien nhau, khong dau :

#define MAX_DEVICES 100     // So luong toi da thiet bi duoc quan ly boi Gateway

#define LORA_RX 16  // Chân RX của LoRa kết nối với TX ESP32
#define LORA_TX 17  // Chân TX của LoRa kết nối với RX ESP32

// ================ List CMD ==================
typedef enum
{
    TemperatureReport = 0x11,
    HumidityReport = 0x12,
    SoilReport = 0x13,
    LuxReport = 0x14,

    Relay1Report = 0x30,
    Relay1Get = 0x31,
    Relay1Set = 0x32,

    Relay2Report = 0x33,
    Relay2Get = 0x34,
    Relay2Set = 0x35
} Cmd_t;

void Config_Init();
int8_t Config_GetSizeByCmd(uint8_t cmd);

#endif
