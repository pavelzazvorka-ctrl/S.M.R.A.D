#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "AppConfig.h"
#include "SensorPacket.h"
#include "BaseStatus.h"
#include "DebugLog.h"

#include "AnalogMeasure.h"

// =============================================================
// SENSOR MANAGER
// =============================================================

class BaseSensorManager
{
public:

    // =========================================================
    // INITIALIZATION
    // =========================================================

    void begin();

    // =========================================================
    // READ ALL SENSORS
    // =========================================================

    void addLocalData(SensorPacket &packet);
    void addLocalSlotData(SensorPacket &packet, int slot, float value);
    // =========================================================
    // SENSOR STATUS
    // =========================================================

    BaseStatus& status();

    void markRs485Valid();
    uint32_t lastValidRS485Ms() const;
    bool initPowerTelemetry();

    AnalogMeasure _powerVin = AnalogMeasure(POWER_VIN_PIN);
    AnalogMeasure _powerBat = AnalogMeasure(POWER_BAT_PIN);

private:
    
    // =========================================================
    // PACKET HELPERS
    // =========================================================

    void clearPacket(SensorPacket &packet);

    void setField(
        SensorPacket &packet,
        uint8_t index,
        float value,
        bool valid
    );

    float getEpochMinutes();

    float keepLastGoodValue(
        uint8_t index,
        float value,
        bool valid
    );

    // =========================================================
    // SENSOR STATUS FLAGS
    // =========================================================
    //
    // Tracks initialization state of all sensor groups.
    //
    // =========================================================

    BaseStatus _status =
    {
        false, // rs485
        false, // power
        false  // sd
    };

    uint32_t _packetSequence = 0;
    uint32_t _lastValidRS485Ms = 0;
    float _lastGood[FIELD_COUNT];
    bool _hasLastGood[FIELD_COUNT];
};

// =============================================================
// GLOBAL SENSOR MANAGER INSTANCE
// =============================================================

extern BaseSensorManager Sensors;