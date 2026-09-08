#include "ConfigManager.h"
#include "mqtt_secrets.h"

// =============================================================
// GLOBAL CONFIG INSTANCE
// =============================================================

ConfigManager Config;

// =============================================================
// BEGIN
// =============================================================

bool ConfigManager::begin()
{
    loadDefaults();

    _loadedFromFile = loadFromSd();

    if (_loadedFromFile)
        Log.printf("CFG Config loaded from: %s", _configFile);
    else
        Log.printf("CFG Config using defaults");

    if (!loadCalibrationFromSd())
        Log.printf("CFG Calibration disabled");

    printSummary();

    return _loadedFromFile;
}

// =============================================================
// DEFAULTS
// =============================================================

void ConfigManager::loadDefaults()
{
    copyString(_cfg.device.name, sizeof(_cfg.device.name), "SMRAD-01");
    copyString(_cfg.device.location, sizeof(_cfg.device.location), "unknown");
    _cfg.device.hardwareRevision = 1;

    copyString(_cfg.wifi.ssid, sizeof(_cfg.wifi.ssid), WIFI_SSID);
    copyString(_cfg.wifi.password, sizeof(_cfg.wifi.password), WIFI_PASS);

    copyString(_cfg.mqtt.server, sizeof(_cfg.mqtt.server), MQTT_SERVER);
    _cfg.mqtt.port = MQTT_PORT;

    copyString(_cfg.mqtt.clientId, sizeof(_cfg.mqtt.clientId), SECRET_MQTT_CLIENT_ID);
    copyString(_cfg.mqtt.username, sizeof(_cfg.mqtt.username), SECRET_MQTT_USERNAME);
    copyString(_cfg.mqtt.password, sizeof(_cfg.mqtt.password), SECRET_MQTT_PASSWORD);

    copyString(_cfg.mqtt.channel1, sizeof(_cfg.mqtt.channel1), CHANNEL_ID_1);
    copyString(_cfg.mqtt.channel2, sizeof(_cfg.mqtt.channel2), CHANNEL_ID_2);
    copyString(_cfg.mqtt.channel3, sizeof(_cfg.mqtt.channel3), CHANNEL_ID_3);

    _cfg.timing.sensorPeriodMs = SENSOR_PERIOD_MS;
    _cfg.timing.mqttSpacingMs = MQTT_SPACING_MS;
    _cfg.timing.timeSyncIntervalMs = TIME_SYNC_INTERVAL_MS;

    for (size_t i = 0; i < CALIBRATION_COUNT; i++)
        _cfg.calibration[i] = {};
}

// =============================================================
// LOAD FROM SD
// =============================================================

bool ConfigManager::loadFromSd()
{
    if (!SD.exists(_configFile))
    {
        Log.printf("CFG Config file not found: %s", _configFile);
        return false;
    }

    File file = SD.open(_configFile, FILE_READ);

    if (!file)
    {
        Log.printf("CFG Config open failed");
        return false;
    }

    //StaticJsonDocument<2048> doc;
    DynamicJsonDocument doc(4096);
    // config

    DeserializationError err = deserializeJson(doc, file);
    file.close();

    if (err)
    {
        Log.printf("CFG Config JSON error: %s", err.c_str());
        return false;
    }

    JsonObject device = doc["device"];
    JsonObject wifi = doc["wifi"];
    JsonObject mqtt = doc["mqtt"];
    JsonObject timing = doc["timing"];

    if (!device.isNull())
    {
        copyString(_cfg.device.name, sizeof(_cfg.device.name), device["name"] | _cfg.device.name);
        copyString(_cfg.device.location, sizeof(_cfg.device.location), device["location"] | _cfg.device.location);
        _cfg.device.hardwareRevision = device["hardwareRevision"] | _cfg.device.hardwareRevision;
    }

    if (!wifi.isNull())
    {
        copyString(_cfg.wifi.ssid, sizeof(_cfg.wifi.ssid), wifi["ssid"] | _cfg.wifi.ssid);
        copyString(_cfg.wifi.password, sizeof(_cfg.wifi.password), wifi["password"] | _cfg.wifi.password);
    }

    if (!mqtt.isNull())
    {
        copyString(_cfg.mqtt.server, sizeof(_cfg.mqtt.server), mqtt["server"] | _cfg.mqtt.server);
        _cfg.mqtt.port = mqtt["port"] | _cfg.mqtt.port;

        copyString(_cfg.mqtt.clientId, sizeof(_cfg.mqtt.clientId), mqtt["clientId"] | _cfg.mqtt.clientId);
        copyString(_cfg.mqtt.username, sizeof(_cfg.mqtt.username), mqtt["username"] | _cfg.mqtt.username);
        copyString(_cfg.mqtt.password, sizeof(_cfg.mqtt.password), mqtt["password"] | _cfg.mqtt.password);

        copyString(_cfg.mqtt.channel1, sizeof(_cfg.mqtt.channel1), mqtt["channel1"] | _cfg.mqtt.channel1);
        copyString(_cfg.mqtt.channel2, sizeof(_cfg.mqtt.channel2), mqtt["channel2"] | _cfg.mqtt.channel2);
        copyString(_cfg.mqtt.channel3, sizeof(_cfg.mqtt.channel3), mqtt["channel3"] | _cfg.mqtt.channel3);
    }

    if (!timing.isNull())
    {
        _cfg.timing.sensorPeriodMs = timing["sensorPeriodMs"] | _cfg.timing.sensorPeriodMs;
        _cfg.timing.mqttSpacingMs = timing["mqttSpacingMs"] | _cfg.timing.mqttSpacingMs;
        _cfg.timing.timeSyncIntervalMs = timing["timeSyncIntervalMs"] | _cfg.timing.timeSyncIntervalMs;
    }

    return true;

}

// =============================================================
// LOAD CALIBRATION FROM SD
// =============================================================
//
// Calibration is intentionally independent from config.json.
// A missing or invalid calibration file does not invalidate a
// successfully loaded runtime configuration.
//
// Invalid records are skipped individually so one typo cannot
// disable all remaining valid calibration records.
// =============================================================

bool ConfigManager::loadCalibrationFromSd()
{
    if (!SD.exists(_calibrationFile))
    {
        Log.printf("CFG Calibration file not found: %s", _calibrationFile);
        return false;
    }

    File calFile = SD.open(_calibrationFile, FILE_READ);

    if (!calFile)
    {
        Log.printf("CFG Calibration open failed");
        return false;
    }

    DynamicJsonDocument doc(4096);
    DeserializationError calErr = deserializeJson(doc, calFile);
    calFile.close();

    if (calErr)
    {
        Log.printf("CFG Calibration JSON error: %s", calErr.c_str());
        return false;
    }

    JsonArray arr = doc["calibration"];

    if (arr.isNull())
    {
        Log.printf("CFG No calibration records");
        return false;
    }

    if (arr.size() > CALIBRATION_COUNT)
    {
        Log.printf(
            "CFG Expected up to %u calibration records, got %u; extra records ignored",
            (unsigned)CALIBRATION_COUNT,
            (unsigned)arr.size()
        );
    }

    size_t loadedCount = 0;
    size_t recordCount = (arr.size() < CALIBRATION_COUNT) ? arr.size() : CALIBRATION_COUNT;

    for (size_t i = 0; i < recordCount; i++)
    {
        JsonObject o = arr[i];

        CalibrationConfig cal = {};
        cal.cIn = o["in"] | UINT32_MAX;
        cal.cOut = o["out"] | UINT32_MAX;
        cal.cType = o["type"] | 0U;
        cal.lowerLimit = o["lowerlimit"] | 0.0f;
        cal.upperLimit = o["upperlimit"] | 0.0f;
        cal.a = o["a"] | 0.0f;
        cal.b = o["b"] | 0.0f;
        cal.c = o["c"] | 0.0f;
        cal.d = o["d"] | 0.0f;
        cal.e = o["e"] | 0.0f;
        cal.f = o["f"] | 0.0f;
        cal.g = o["g"] | 0.0f;

        if (!validateCalibration(cal, i))
            continue;

        // Keep valid records packed so getCalibration() can scan
        // the fixed-size array without holes caused by bad input.
        _cfg.calibration[loadedCount++] = cal;
    }

    Log.printf(
        "CFG Calibration loaded: %u valid of %u record(s)",
        (unsigned)loadedCount,
        (unsigned)recordCount
    );

    return loadedCount > 0 || recordCount == 0;
}

bool ConfigManager::validateCalibration(
    const CalibrationConfig &cal,
    size_t index
) const
{
    if (cal.cIn >= FIELDS_PER_CHANNEL)
    {
        Log.printf(
            "CFG Invalid calibration[%u]: input field %lu out of range 0..%u",
            (unsigned)index,
            cal.cIn,
            (unsigned)(FIELDS_PER_CHANNEL - 1)
        );
        return false;
    }

    if (cal.cOut >= FIELD_COUNT)
    {
        Log.printf(
            "CFG Invalid calibration[%u]: output field %lu out of range 0..%u",
            (unsigned)index,
            cal.cOut,
            (unsigned)(FIELD_COUNT - 1)
        );
        return false;
    }

    if (cal.cType < CAL_POLYNOMIAL || cal.cType > CAL_LOG)
    {
        Log.printf(
            "CFG Invalid calibration[%u]: type %lu is not supported",
            (unsigned)index,
            cal.cType
        );
        return false;
    }

    const float values[] =
    {
        cal.lowerLimit, cal.upperLimit,
        cal.a, cal.b, cal.c, cal.d, cal.e, cal.f, cal.g
    };

    for (float value : values)
    {
        if (!isfinite(value))
        {
            Log.printf(
                "CFG Invalid calibration[%u]: non-finite coefficient/limit",
                (unsigned)index
            );
            return false;
        }
    }

    if (cal.lowerLimit != cal.upperLimit &&
        cal.lowerLimit > cal.upperLimit)
    {
        Log.printf(
            "CFG Invalid calibration[%u]: lowerlimit > upperlimit",
            (unsigned)index
        );
        return false;
    }

    return true;
}

// =============================================================
// ACCESSORS
// =============================================================

const RuntimeConfig& ConfigManager::get() const
{
    return _cfg;
}

bool ConfigManager::loadedFromFile() const
{
    return _loadedFromFile;
}

// =============================================================
// PRINT SUMMARY
// =============================================================

void ConfigManager::printSummary()
{
    Log.printf("Device: %s", _cfg.device.name);
    Log.printf("Location: %s", _cfg.device.location);
    Log.printf("HW rev: %u", _cfg.device.hardwareRevision);

    Log.printf("WiFi SSID: %s", _cfg.wifi.ssid);
    Log.printf("MQTT server: %s:%u", _cfg.mqtt.server, _cfg.mqtt.port);

    Log.printf("Channels: %s / %s / %s",
               _cfg.mqtt.channel1,
               _cfg.mqtt.channel2,
               _cfg.mqtt.channel3);

    Log.printf("Timing: sensor=%lu mqttSpacing=%lu timeSync=%lu",
               _cfg.timing.sensorPeriodMs,
               _cfg.timing.mqttSpacingMs,
               _cfg.timing.timeSyncIntervalMs);


    for (size_t i = 0; i < CALIBRATION_COUNT; i++)
    {
        if (_cfg.calibration[i].cType > 0)
        {
            Log.printf(
                "Cal[%u]: In=%lu Out=%lu Type=%lu",
                (unsigned)i,
                _cfg.calibration[i].cIn,
                _cfg.calibration[i].cOut,
                _cfg.calibration[i].cType
            );
        }
    }
}

// =============================================================
// SAFE STRING COPY
// =============================================================

void ConfigManager::copyString(char* dst, size_t dstSize, const char* src)
{
    if (!dst || dstSize == 0)
        return;

    if (!src)
    {
        dst[0] = '\0';
        return;
    }

    strncpy(dst, src, dstSize - 1);
    dst[dstSize - 1] = '\0';
}

const CalibrationConfig* ConfigManager::getCalibration(uint32_t inputChannel) const
{
    for (size_t i = 0; i < CALIBRATION_COUNT; i++)
    {
        if (_cfg.calibration[i].cType == 0)
            continue;   // prázdný záznam

        if (_cfg.calibration[i].cIn == inputChannel)
        {
            return &_cfg.calibration[i];
        }
    }

    return nullptr; // nenalezeno
}
