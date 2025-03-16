#ifndef _LORA_H_
#define _LORA_H_

#include <Arduino.h>
#include "config.h"

#define MAX_COMMAND_SIZE 10
#define MAX_COMMAND_DATA_SIZE 256

typedef struct
{
    uint8_t cmd;
    uint8_t data[MAX_COMMAND_DATA_SIZE];
    int8_t dataSize;
} Command_t;

typedef struct
{
    uint64_t devId;
    Command_t commands[MAX_COMMAND_SIZE];
    int8_t commandSize;
} Frame_t;

void Lora_Init();
void Lora_Send(Frame_t frame);
bool Lora_Receive(Frame_t *frame);

#endif
