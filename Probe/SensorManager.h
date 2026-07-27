#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "AppConfig.h"
#include "SensorPacket.h"
#include "ProbeStatus.h"
#include "DebugLog.h"

#include "DFRobot_MultiGasSensor.h"
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <SparkFun_FS3000_Arduino_Library.h>
#include "ExplorIR_CO2.h"
#include "AnalogMeasure.h"

// =============================================================
// SENSOR MANAGER
// =============================================================
//
// Centralized physical sensor subsystem.

class SensorManager
{
public:

    void begin();
    void read(SensorPacket &packet);
    ProbeStatus& status();

    uint32_t lastValidCo2Ms() const;

    void logSelfTest();
    bool initPowerTelemetry();

    AnalogMeasure _powerVin = AnalogMeasure(POWER_VIN_PIN);
    AnalogMeasure _powerBat = AnalogMeasure(POWER_BAT_PIN);

private:
    bool initBme();
    bool initGas();
    bool initCo2();
    bool initFS3000();
    void initAnalogSensors();    

    void clearPacket(SensorPacket &packet);

    void setField(
        SensorPacket &packet,
        uint8_t index,
        float value,
        bool valid
    );

    float keepLastGoodValue(
        uint8_t index,
        float value,
        bool valid
    );

    ProbeStatus _status =
    {
        false, // bme
        false, // o2
        false, // co2
        false, // mq4
        false, // H2S
        false  // fs3000
    };

    FS3000 _fs;
    Adafruit_BME280 _bme;
    DFRobot_GAS_I2C _o2 = DFRobot_GAS_I2C(&Wire, 0x74);
    HardwareSerial _co2Uart = HardwareSerial(2);
    ExplorIR_CO2 _co2 = ExplorIR_CO2( _co2Uart, CO2_RX, CO2_TX );
    AnalogMeasure _mq4 = AnalogMeasure(MQ4_PIN);
    AnalogMeasure _h2s = AnalogMeasure(H2S_PIN);

    uint32_t _packetSequence = 0;
    uint32_t _lastValidCo2Ms = 0;
    float _lastGood[FIELD_COUNT];
    bool _hasLastGood[FIELD_COUNT];
};

extern SensorManager Sensors;