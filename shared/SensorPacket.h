#pragma once

#include <Arduino.h>
#include "AppConfig.h"

// =============================================================
// SENSOR PACKET
// =============================================================
//
// SensorPacket is the shared data container passed between tasks
// and converted to/from SmradPacket for RS485 transfer.
//
// NOTE:
// Device-local status is intentionally NOT part of SensorPacket.
// Probe uses ProbeStatus, Base/Logger uses BaseStatus.
//
// Index mapping:
//   0..7   -> ThingSpeak topic1 field1..field8
//   8..15  -> ThingSpeak topic2 field1..field8
//   16..23 -> ThingSpeak topic3 field1..field8
//
// =============================================================

struct SensorPacket
{
    float f[FIELD_COUNT];
    bool valid[FIELD_COUNT];
    uint32_t sequence;
    uint32_t timestampMs;
};

inline bool isGoodNumber(float value)
{
    return isfinite(value);
}
