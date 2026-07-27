#include "MqttPublisher.h"
#include <WiFi.h>
#include "ConfigManager.h"

// =============================================================
// MQTT PUBLISHER
// =============================================================
//
// MqttPublisher owns the complete MQTT / ThingSpeak publishing
// pipeline.
//
// Responsibilities:
// -------------------------------------------------------------
// - configure MQTT broker
// - maintain MQTT connection
// - reconnect when needed
// - format ThingSpeak payloads
// - split SensorPacket into multiple ThingSpeak channels
// - track last successful MQTT and publish activity
//
// Design Philosophy:
// -------------------------------------------------------------
// This class intentionally does NOT know anything about sensors.
//
// It receives a complete SensorPacket and publishes it.
//
// This keeps the architecture simple:
//
//   SensorManager  -> creates data
//   MqttPublisher  -> publishes data
//
// =============================================================


// =============================================================
// CONSTRUCTOR
// =============================================================
//
// The class does not own the network client.
// It receives an existing Client reference.
//
// In SMRAD this is usually:
//
//   WiFiClient wifiClient;
//   MqttPublisher Mqtt(wifiClient);
//
// =============================================================

MqttPublisher::MqttPublisher(Client &client)
    :
    _mqtt(client)
{
}

// =============================================================
// BEGIN
// =============================================================
//
// Initializes MQTT publisher configuration.
//
// Responsibilities:
// -------------------------------------------------------------
// - build ThingSpeak publish topics
// - configure broker address
// - configure MQTT keepalive
// - configure socket timeout
//
// No connection attempt is performed here.
//
// Actual connection is handled later by:
//
//   tick()
//
// This keeps startup non-blocking.
//
// =============================================================

void MqttPublisher::begin()
{
    const RuntimeConfig& cfg = Config.get();

    snprintf(_topic1, sizeof(_topic1), "channels/%s/publish", cfg.mqtt.channel1);
    snprintf(_topic2, sizeof(_topic2), "channels/%s/publish", cfg.mqtt.channel2);
    snprintf(_topic3, sizeof(_topic3), "channels/%s/publish", cfg.mqtt.channel3);

    _mqtt.setServer(cfg.mqtt.server, cfg.mqtt.port);
    // KeepAlive:
    // MQTT broker considers connection dead if no traffic is seen
    // within this interval.
    _mqtt.setKeepAlive(30);

    // Socket timeout:
    // prevents MQTT operations from blocking too long.
    _mqtt.setSocketTimeout(3);
}

// =============================================================
// TICK
// =============================================================
//
// Non-blocking MQTT state machine.
//
// Called periodically from networkTask.
//
// Responsibilities:
// -------------------------------------------------------------
// - do nothing if WiFi is offline
// - keep MQTT loop alive if connected
// - reconnect periodically if disconnected
//
// Reconnect pacing:
// -------------------------------------------------------------
// MQTT_RETRY_MS prevents aggressive reconnect loops.
//
// This protects:
// - broker
// - WiFi stack
// - ESP32 CPU time
//
// =============================================================

void MqttPublisher::tick()
{
    if (WiFi.status() != WL_CONNECTED)
        return;

    if (_mqtt.connected())
    {
        _lastOkMs = millis();
        _mqtt.loop();
        return;
    }

    uint32_t nowMs = millis();

    if (nowMs - _lastAttemptMs < MQTT_RETRY_MS)
        return;

    _lastAttemptMs = nowMs;

    Log.printf("MQTT connecting...");

    const RuntimeConfig& cfg = Config.get();

    if (_mqtt.connect(
        cfg.mqtt.clientId,
        cfg.mqtt.username,
        cfg.mqtt.password
    ))
    {
        _lastOkMs = nowMs;
        Log.printf("MQTT OK");
    }
    else
    {
        Log.printf("MQTT failed, state: %d", _mqtt.state());
    }
}

// =============================================================
// CONNECTED
// =============================================================
//
// Returns current MQTT connection state.
//
// Used by:
// - networkTask
// - heartbeatTask
// - watchdogTask
//
// =============================================================

bool MqttPublisher::connected()
{
    return _mqtt.connected();
}

// =============================================================
// LAST MQTT OK TIMESTAMP
// =============================================================
//
// Timestamp of last known healthy MQTT connection.
//
// Used by watchdog recovery logic.
//
// =============================================================

uint32_t MqttPublisher::lastOkMs() const
{
    return _lastOkMs;
}

// =============================================================
// LAST SUCCESSFUL PUBLISH TIMESTAMP
// =============================================================
//
// Timestamp of last completely successful packet publish.
//
// A packet is considered successful only if all required
// ThingSpeak channels were published successfully.
//
// Used by watchdog to detect half-dead MQTT states.
//
// =============================================================

uint32_t MqttPublisher::lastPublishOkMs() const
{
    return _lastPublishOkMs;
}

// =============================================================
// RECOVER
// =============================================================
//
// Soft recovery called by watchdog.
//
// Does not restart ESP32.
// Only resets MQTT client state.
//
// Next tick() call will attempt reconnect.
//
// =============================================================

void MqttPublisher::recover()
{
    _mqtt.disconnect();
    _lastAttemptMs = 0;
}

// =============================================================
// APPEND FIELD
// =============================================================
//
// Safely appends one ThingSpeak field into payload buffer.
//
// Example output:
//
//   field1=123.456&
//
// or:
//
//   field1=123.456
//
// Invalid values:
// -------------------------------------------------------------
// If value is NAN or invalid, empty field is emitted:
//
//   field1=
//
// This keeps field order stable while indicating missing data.
//
// Safety:
// -------------------------------------------------------------
// snprintf() result is checked.
//
// If payload buffer would overflow:
// - function returns false
// - caller aborts publishing that payload
//
// =============================================================

bool MqttPublisher::appendField(
    char* payload,
    size_t payloadSize,
    int &len,
    int fieldNumber,
    float value,
    bool addAmpersand
)
{
    if (!payload || payloadSize == 0)
        return false;

    if (len < 0 || len >= (int)payloadSize)
        return false;

    int written = 0;

    if (isGoodNumber(value))
    {
        written = snprintf(
            payload + len,
            payloadSize - len,
            "field%d=%.3f%s",
            fieldNumber,
            value,
            addAmpersand ? "&" : ""
        );
    }
    else
    {
        written = snprintf(
            payload + len,
            payloadSize - len,
            "field%d=%s",
            fieldNumber,
            addAmpersand ? "&" : ""
        );
    }

    if (written < 0)
        return false;

    if (written >= (int)(payloadSize - len))
        return false;

    len += written;
    return true;
}

// =============================================================
// PUBLISH PACKET
// =============================================================
//
// Publishes one SensorPacket to ThingSpeak.
//
// ThingSpeak limitation:
// -------------------------------------------------------------
// Maximum 8 fields per channel.
//
// Therefore SensorPacket is split into chunks:
//
//   indexes 0..7    -> topic1 field1..field8
//   indexes 8..15   -> topic2 field1..field8
//   indexes 16..23  -> topic3 field1..field8
//
// Important:
// -------------------------------------------------------------
// Field numbering restarts for every ThingSpeak channel.
//
// That means:
//
//   packet.f[8]
//
// is published as:
//
//   topic2 field1
//
// not:
//
//   field9
//
// Timing:
// -------------------------------------------------------------
// A short delay is inserted between channel publishes.
//
// Reason:
// - ThingSpeak may reject or miss too-fast bursts
// - slower publishing is more reliable
//
// Return value:
// -------------------------------------------------------------
// true:
//   all required channel publishes succeeded
//
// false:
//   at least one publish failed
//
// =============================================================

bool MqttPublisher::publishPacket(const SensorPacket &packet)
{
    if (!_mqtt.connected())
        return false;

    const char* topics[] =
    {
        _topic1,
        _topic2,
        _topic3
    };

    const int channelCount =
        (FIELD_COUNT + FIELDS_PER_CHANNEL - 1) / FIELDS_PER_CHANNEL;

    if (channelCount > MAX_CHANNELS)
    {
        Log.printf("MQTT config error: channelCount=%d", channelCount);
        return false;
    }

    bool allOk = true;

    for (int channel = 0; channel < channelCount; channel++)
    {
        char payload[256];
        payload[0] = '\0';

        int len = 0;

        int startIndex = channel * FIELDS_PER_CHANNEL;
        int endIndex = min(startIndex + FIELDS_PER_CHANNEL, FIELD_COUNT);

        for (int i = startIndex; i < endIndex; i++)
        {
            int fieldNumber = (i - startIndex) + 1;
            bool addAmpersand = (i < endIndex - 1);

            if (!appendField(
                    payload,
                    sizeof(payload),
                    len,
                    fieldNumber,
                    packet.f[i],
                    addAmpersand
                ))
            {
                Log.printf("MQTT payload overflow");
                allOk = false;
                break;
            }
        }

        if (!allOk)
            break;

        Log.printf("MQTT Tx topic %d: %s", channel + 1, payload);

        if (!_mqtt.publish(topics[channel], payload))
        {
            Log.printf("MQTT publish failed topic %d", channel + 1);
            allOk = false;
        }

        if (channel < channelCount - 1)
            vTaskDelay(pdMS_TO_TICKS(Config.get().timing.mqttSpacingMs));
    }

    if (allOk)
        _lastPublishOkMs = millis();

    return allOk;
}