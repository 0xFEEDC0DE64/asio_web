#pragma once

#include <cstdint>

#pragma pack(push,1)
struct WebsocketHeader {
    uint8_t opcode:4;
    uint8_t reserved:3;
    bool fin:1;
    uint8_t payloadLength:7;
    bool mask:1;
};
#pragma pack(pop)
