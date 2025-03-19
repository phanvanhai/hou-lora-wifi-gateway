#include "config.h"

static ResourceInfo_t resourceInfoTable[] = {
    {"NhietDo",
     ValueType_t::Int8,
     1,
     {0x11, 0, 0}},
    {"DoAm",
     ValueType_t::Uint8,
     1,
     {0x12, 0, 0}},
    {"DoAmDat",
     ValueType_t::Uint8,
     1,
     {0x13, 0, 0}},
    {"AnhSang",
     ValueType_t::Uint16,
     2,
     {0x14, 0, 0}},
    {"Relay1",
     ValueType_t::Bool,
     1,
     {0x30, 0x31, 0x32}},
    {"Relay2",
     ValueType_t::Bool,
     1,
     {0x33, 0x34, 0x35}}};

ResourceInfo_t Config_GetInfoByCmd(uint8_t cmd)
{
    int len = sizeof(resourceInfoTable) / sizeof(ResourceInfo_t);

    for (int i = 0; i < len; i++)
    {
        if (resourceInfoTable[i].cmds[ResourceType_t::Report] == cmd || resourceInfoTable[i].cmds[ResourceType_t::Get] == cmd || resourceInfoTable[i].cmds[ResourceType_t::Set] == cmd)
        {
            return resourceInfoTable[i];
        }
    }

    return ResourceInfo_t{};
}

ResourceInfo_t Config_GetInfoByName(String name)
{
    int len = sizeof(resourceInfoTable) / sizeof(ResourceInfo_t);

    for (int i = 0; i < len; i++)
    {
        if (resourceInfoTable[i].name == name)
        {
            return resourceInfoTable[i];
        }
    }

    return ResourceInfo_t{};
}