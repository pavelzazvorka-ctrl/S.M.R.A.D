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

// Logical field indexes shared by Probe and Logger.
// Keeping the mapping named prevents accidental slot collisions while
// preserving the existing 24-field wire/MQTT format.
enum SensorFieldIndex : uint8_t
{
    FIELD_TEMPERATURE = 0,
    FIELD_PRESSURE = 1,
    FIELD_HUMIDITY = 2,
    FIELD_AIRFLOW = 3,
    FIELD_O2_RAW = 4,
    FIELD_CO2_RAW = 5,
    FIELD_METHANE_RAW = 6,
    FIELD_H2S_RAW = 7,

    FIELD_CAL_1 = 8,
    FIELD_CAL_2 = 9,
    FIELD_CAL_3 = 10,
    FIELD_CAL_4 = 11,
    FIELD_CAL_5 = 12,
    FIELD_CAL_6 = 13,
    FIELD_CAL_7 = 14,
    FIELD_CAL_8 = 15,

    FIELD_TIMESTAMP_MIN = 16,
    FIELD_PROBE_POWER = 17,
    FIELD_LOGGER_AC = 18,
    FIELD_LOGGER_BAT = 19,
    FIELD_RTC_TEMPERATURE = 20
};

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
