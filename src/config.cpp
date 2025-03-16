#include "config.h"

static int8_t cmdLenTable[256];

void Config_Init()
{
    memset(cmdLenTable, -1, sizeof(cmdLenTable));

    // ================ Table {cmd, data size} ==================
    cmdLenTable[TemperatureReport] = 1;
    cmdLenTable[HumidityReport] = 1;
    cmdLenTable[SoilReport] = 1;
    cmdLenTable[LuxReport] = 2;
    cmdLenTable[Relay1Report] = 1;
    cmdLenTable[Relay2Report] = 1;

    cmdLenTable[Relay1Get] = 1;
    cmdLenTable[Relay2Get] = 1;

    cmdLenTable[Relay1Set] = 1;
    cmdLenTable[Relay2Set] = 1;
}

int8_t Config_GetSizeByCmd(uint8_t cmd)
{
    return cmdLenTable[cmd];
}