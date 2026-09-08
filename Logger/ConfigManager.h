#pragma once

#include <Arduino.h>
#include <SD.h>
#include <ArduinoJson.h>

#include "AppConfig.h"
#include "DebugLog.h"

// =============================================================
// CONFIG MANAGER
// =============================================================
//
// Loads runtime configuration from:
//
//   /config.json
//
// If config file is missing or invalid,
// safe defaults from AppConfig.h are used.
//
// =============================================================

struct DeviceConfig
{
    char name[32] = "SMRAD-01";
    char location[64] = "unknown";
    uint8_t hardwareRevision = 1;
};

struct WifiConfig
{
    char ssid[64] = WIFI_SSID;
    char password[64] = WIFI_PASS;
};

struct MqttConfig
{
    char server[64] = MQTT_SERVER;
    uint16_t port = MQTT_PORT;

    char clientId[96] = "";
    char username[96] = "";
    char password[96] = "";

    char channel1[24] = CHANNEL_ID_1;
    char channel2[24] = CHANNEL_ID_2;
    char channel3[24] = CHANNEL_ID_3;
};

struct TimingConfig
{
    uint32_t sensorPeriodMs = SENSOR_PERIOD_MS;
    uint32_t mqttSpacingMs = MQTT_SPACING_MS;
    uint32_t timeSyncIntervalMs = TIME_SYNC_INTERVAL_MS;
};

enum CalibrationType : uint32_t
{
    CAL_POLYNOMIAL = 1,
    CAL_EXPONENTIAL = 2,
    CAL_POWER = 3,
    CAL_LOG = 4
};

struct CalibrationConfig
{
    uint32_t cIn;
    uint32_t cOut;
    uint32_t cType;
    float lowerLimit;
    float upperLimit;
    float a;
    float b;
    float c;
    float d;
    float e;
    float f;
    float g;
};

constexpr size_t CALIBRATION_COUNT = 8;

struct RuntimeConfig
{
    DeviceConfig device;
    WifiConfig wifi;
    MqttConfig mqtt;
    TimingConfig timing;
    CalibrationConfig calibration[CALIBRATION_COUNT];
};

class ConfigManager
{
public:
    bool begin();
    const RuntimeConfig& get() const;
    const CalibrationConfig* getCalibration(uint32_t inputChannel) const;
    bool loadedFromFile() const;
    void printSummary();

private:
    void loadDefaults();
    bool loadFromSd();
    bool loadCalibrationFromSd();
    bool validateCalibration(const CalibrationConfig &cal, size_t index) const;
    void copyString(char* dst, size_t dstSize, const char* src);

    RuntimeConfig _cfg;
    bool _loadedFromFile = false;

    const char* _configFile = "/config.json";
    const char* _calibrationFile = "/calibration.json";

};

extern ConfigManager Config;