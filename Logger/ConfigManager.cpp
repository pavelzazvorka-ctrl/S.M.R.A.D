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

    doc.clear();
    Log.printf("CFG Reading calibration file");       
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
        Log.printf("CFG no calibration records");
        return false;
    } 
    if (arr.size() > 8)
    {
        Log.printf("CFG Expected upp to 24 calibration records, got %u", arr.size());
        return false;
    }
    Log.printf("CFG calibration records, got %u", arr.size());
 
    for (int i = 0; i < 8 && i < arr.size(); i++)
    {

        JsonObject o = arr[i];

        _cfg.calibration[i].cIn = o["in"];
        _cfg.calibration[i].cOut = o["out"];
        _cfg.calibration[i].cType = o["type"];
        _cfg.calibration[i].lowerLimit = o["lowerlimit"];
        _cfg.calibration[i].upperLimit = o["upperlimit"];
        _cfg.calibration[i].a = o["a"];
        _cfg.calibration[i].b = o["b"];
        _cfg.calibration[i].c = o["c"];
        _cfg.calibration[i].d = o["d"];
        _cfg.calibration[i].e = o["e"];
        _cfg.calibration[i].f = o["f"];
        _cfg.calibration[i].g = o["g"];
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

    
    for (int i = 0; i < 8 ; i++)
    {
        if (_cfg.calibration[i].cType > 0)
        {
            Log.printf(
                "Cal[%d]: In=%lu Out=%lu Type=%lu",
                i,
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
    for (int i = 0; i < 8; i++)
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