#ifndef _UTILS_H_
#define _UTILS_H_

#include <Arduino.h>
#include "lora.h"

#define JSON_SIZE 1024  // Kich thuoc JSON
#define EEPROM_SIZE 512 // Kich thuoc EEPROM


bool jsonToFrame(const String &jsonStr, Frame_t &frame, bool isGetReq);
void frameToJson(const Frame_t &frame, String &jsonStr);

void convertConfigToJson(const String configList[], int configCount, String &jsonStr);
bool convertJsonToConfig(const String &jsonStr, String configList[], int &configCount, int maxConfig);

#endif