#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <string.h>

// =============================================================
// SMRAD RS485 PACKET FORMAT
// =============================================================

#define SMRAD_PACKET_MAGIC_1   0xA5
#define SMRAD_PACKET_MAGIC_2   0x5A
#define SMRAD_PACKET_MAGIC_3   0x53
#define SMRAD_PACKET_MAGIC_4   0x4D

#define SMRAD_PACKET_VERSION   1
#define SMRAD_MAX_FIELDS       24

struct __attribute__((packed)) SmradPacket
{
    uint8_t magic1;
    uint8_t magic2;
    uint8_t magic3;
    uint8_t magic4;

    uint8_t version;
    uint8_t fieldCount;

    uint32_t sequence;
    uint32_t uptimeMs;

    float values[SMRAD_MAX_FIELDS];

    uint32_t validMask;

    uint16_t crc;
};

inline void smradPacketInit(SmradPacket &p)
{
    memset(&p, 0, sizeof(SmradPacket));

    p.magic1 = SMRAD_PACKET_MAGIC_1;
    p.magic2 = SMRAD_PACKET_MAGIC_2;
    p.magic3 = SMRAD_PACKET_MAGIC_3;
    p.magic4 = SMRAD_PACKET_MAGIC_4;

    p.version = SMRAD_PACKET_VERSION;
    p.fieldCount = SMRAD_MAX_FIELDS;

    p.sequence = 0;
    p.uptimeMs = millis();

    for (uint8_t i = 0; i < SMRAD_MAX_FIELDS; i++)
        p.values[i] = NAN;
}

inline void smradPacketSetField(
    SmradPacket &p,
    uint8_t index,
    float value,
    bool valid
)
{
    if (index >= SMRAD_MAX_FIELDS)
        return;

    p.values[index] = value;

    if (valid && isfinite(value))
        p.validMask |= (1UL << index);
    else
        p.validMask &= ~(1UL << index);
}

inline bool smradPacketFieldValid(
    const SmradPacket &p,
    uint8_t index
)
{
    if (index >= SMRAD_MAX_FIELDS)
        return false;

    return (p.validMask & (1UL << index)) != 0;
}

inline bool smradPacketBasicValid(const SmradPacket &p)
{
    if (p.magic1 != SMRAD_PACKET_MAGIC_1) return false;
    if (p.magic2 != SMRAD_PACKET_MAGIC_2) return false;
    if (p.magic3 != SMRAD_PACKET_MAGIC_3) return false;
    if (p.magic4 != SMRAD_PACKET_MAGIC_4) return false;

    if (p.version != SMRAD_PACKET_VERSION)
        return false;

    if (p.fieldCount == 0 || p.fieldCount > SMRAD_MAX_FIELDS)
        return false;

    return true;
}

inline constexpr size_t smradPacketCrcOffset()
{
    return offsetof(SmradPacket, crc);
}