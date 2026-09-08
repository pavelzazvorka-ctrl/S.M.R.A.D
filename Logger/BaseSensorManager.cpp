#include "BaseSensorManager.h"

// =============================================================
// GLOBAL SENSOR MANAGER INSTANCE
// =============================================================
//
// Centralized sensor subsystem used by the firmware.
//
// All physical sensor access should go through:
//
//   Sensors
//
// This prevents:
// - concurrent I2C access
// - UART collisions
// - duplicated sensor logic
// - timing inconsistencies
//
// =============================================================

BaseSensorManager Sensors;

// =============================================================
// BEGIN
// =============================================================

bool BaseSensorManager::initPowerTelemetry()
{
    _powerVin.begin();
    _powerVin.setSamples(21);
    _powerVin.setEmaAlpha(0.15f);
    _powerVin.setVoltageDivider(
        POWER_VIN_RTOP,
        POWER_VIN_RBOTTOM
    );

    _powerBat.begin();
    _powerBat.setSamples(21);
    _powerBat.setEmaAlpha(0.15f);
    _powerBat.setVoltageDivider(
        POWER_BAT_RTOP,
        POWER_BAT_RBOTTOM
    );

    Log.printf("* Power telemetry OK");
    return true;
}

void BaseSensorManager::begin()
{
    Log.printf("SNS Sensors init...");

    // Initialize last-known-good cache.
    // Used to stabilize telemetry when temporary invalid readings
    // occur.
    for (uint8_t i = 0; i < FIELD_COUNT; i++)
    {
        _lastGood[i] = NAN;
        _hasLastGood[i] = false;
    }

    // Initialize individual sensor groups.
    _status.power   = initPowerTelemetry();
    
    Log.printf("SNS Base sensors init done");
}

// =============================================================
// SENSOR STATUS ACCESS
// =============================================================
//
// Returns current sensor availability structure.
//
// Used by:
// - watchdog diagnostics
// - telemetry diagnostics
// - debug output
//
// =============================================================

BaseStatus& BaseSensorManager::status()
{
    return _status;
}


// =============================================================
// READ ALL SENSORS
// =============================================================
//
// Performs one complete sensor acquisition cycle.
//
// Responsibilities:
// -------------------------------------------------------------
// - clear packet
// - read all sensors
// - validate values
// - apply last-good-value stabilization
// - fill SensorPacket fields
//
// Important:
// -------------------------------------------------------------
// SensorManager intentionally performs ALL sensor access.
//
// This avoids:
// - concurrent I2C access
// - UART conflicts
// - duplicated timing logic
//
// =============================================================

float BaseSensorManager::getEpochMinutes()
{
    time_t now;
    time(&now);
    return (float)(now / 60);
}

void BaseSensorManager::addLocalSlotData(SensorPacket &packet, int slot, float value)
{
    setField(packet, slot, value, true);
    Log.printf("SNS %.2f -> %lu ", value, slot);
}

void BaseSensorManager::addLocalData(SensorPacket &packet)
{
    // 16	Čas. značka	ESP (RTC)						Unix time (float)
    // 17	Probe Uin	ESP Ain	V	V		0	4095	Ain
    // 18	U_check		ESP Ain	V	V		0	5	    U na pom. zdroji 3,3V (přítomnost 230V)
    // 19	U bat		ESP Ain	V	V		0	20	    U na baterii (zbytek kapacity), U dělič
    // 20	Teplota		RTC			°C		

    // =========================================================
    // POWER
    // =========================================================


    uint32_t nowMs = millis();
    _lastValidRS485Ms = nowMs;

    AnalogMeasure::Reading vinReading;
    AnalogMeasure::Reading batReading;

    vinReading.valid = false;
    batReading.valid = false;

    vinReading = _powerVin.read();
    batReading = _powerBat.read();

    // time stamp ( minutes since epoch)
    setField(packet, 16, getEpochMinutes(), true);

    // 17 is Probe A2 in original packet
    
    // Power
    setField(packet, 18, vinReading.filteredVoltage,
         vinReading.valid && isGoodNumber(vinReading.filteredVoltage));

    setField(packet, 19, batReading.filteredVoltage,
         batReading.valid && isGoodNumber(batReading.filteredVoltage));
 
    Log.printf("SNS Packet #%lu add local values", packet.sequence);
}

// =============================================================
// CLEAR PACKET
// =============================================================
//
// Initializes packet before filling sensor data.
//
// Responsibilities:
// -------------------------------------------------------------
// - clear all fields
// - clear valid flags
// - assign sequence number
// - assign timestamp
//
// =============================================================

void BaseSensorManager::clearPacket(SensorPacket &packet)
{
    for (uint8_t i = 0; i < FIELD_COUNT; i++)
    {
        packet.f[i] = NAN;
        packet.valid[i] = false;
    }

    packet.sequence = ++_packetSequence;
    packet.timestampMs = millis();
}

// =============================================================
// SET FIELD
// =============================================================
//
// Writes one logical telemetry field.
//
// Responsibilities:
// -------------------------------------------------------------
// - bounds checking
// - last-good-value stabilization
// - validity tracking
//
// =============================================================

void BaseSensorManager::setField(
    SensorPacket &packet,
    uint8_t index,
    float value,
    bool valid
)
{
    if (index >= FIELD_COUNT)
        return;

    packet.f[index] =
        keepLastGoodValue(index, value, valid);

    packet.valid[index] =
        valid || _hasLastGood[index];
}

// =============================================================
// LAST GOOD VALUE STABILIZATION
// =============================================================
//
// Keeps previous valid measurement if current read is invalid.
//
// Why:
// -------------------------------------------------------------
// Some sensors occasionally produce:
// - NAN
// - short glitches
// - temporary invalid readings
//
// Replacing temporary invalid values with the last known valid
// measurement produces much more stable telemetry.
//
// Important:
// -------------------------------------------------------------
// This is NOT long-term caching.
//
// Only temporary invalid reads are masked.
//
// =============================================================

float BaseSensorManager::keepLastGoodValue(
    uint8_t index,
    float value,
    bool valid
)
{
    if (index >= FIELD_COUNT)
        return NAN;

    if (valid && isGoodNumber(value))
    {
        _lastGood[index] = value;
        _hasLastGood[index] = true;

        return value;
    }

    if (_hasLastGood[index])
        return _lastGood[index];

    return NAN;
}

uint32_t BaseSensorManager::lastValidRS485Ms() const
{
    return _lastValidRS485Ms;
}
