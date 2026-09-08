#pragma once

#include <Arduino.h>
#include <PubSubClient.h>

#include "AppConfig.h"
#include "SensorPacket.h"
#include "DebugLog.h"
#include "mqtt_secrets.h"

// =============================================================
// MQTT PUBLISHER
// =============================================================
//
// Central MQTT / ThingSpeak publishing subsystem.
//
// Responsibilities:
// -------------------------------------------------------------
// - configure MQTT client
// - maintain MQTT connection
// - reconnect automatically
// - format ThingSpeak payloads
// - split SensorPacket across multiple channels
// - track publish health
//
// Architecture:
// -------------------------------------------------------------
// MqttPublisher intentionally owns the entire MQTT pipeline.
//
// Other modules should NOT directly:
// - publish MQTT messages
// - reconnect MQTT
// - manipulate MQTT state
//
// This centralization:
// - prevents race conditions
// - simplifies reconnect handling
// - simplifies watchdog supervision
// - improves debugging clarity
//
// ThingSpeak Limitation:
// -------------------------------------------------------------
// ThingSpeak allows:
//
//   max 8 fields per channel
//
// SMRAD therefore automatically splits SensorPacket into:
//
//   topic1 -> fields 1..8
//   topic2 -> fields 1..8
//   topic3 -> fields 1..8
//
// depending on:
//
//   FIELD_COUNT
//
// Design Philosophy:
// -------------------------------------------------------------
// This class intentionally focuses ONLY on:
//
// - transport
// - formatting
// - connection handling
//
// It intentionally knows NOTHING about:
//
// - sensor internals
// - sensor timing
// - SD logging
// - task management
//
// This keeps the system modular and readable.
//
// =============================================================

class MqttPublisher
{
public:

    // =========================================================
    // CONSTRUCTOR
    // =========================================================
    //
    // Receives externally created network client.
    //
    // Example:
    //
    //   WiFiClient wifiClient;
    //   MqttPublisher mqtt(wifiClient);
    //
    // The class does NOT own the client.
    //
    // =========================================================

    explicit MqttPublisher(Client &client);

    // =========================================================
    // INITIALIZATION
    // =========================================================
    //
    // Configures:
    // - MQTT broker
    // - keepalive
    // - socket timeout
    // - ThingSpeak topics
    //
    // No network connection is performed here.
    //
    // =========================================================

    void begin();

    // =========================================================
    // MQTT STATE MACHINE
    // =========================================================
    //
    // Non-blocking MQTT maintenance function.
    //
    // Typically called periodically by:
    //
    //   networkTask
    //
    // Responsibilities:
    // ---------------------------------------------------------
    // - maintain MQTT loop
    // - reconnect if disconnected
    // - throttle reconnect attempts
    //
    // =========================================================

    void tick();

    // =========================================================
    // CONNECTION STATE
    // =========================================================
    //
    // Returns:
    //
    // true  -> MQTT connected
    // false -> MQTT disconnected
    //
    // =========================================================

    bool connected();

    // =========================================================
    // PUBLISH SENSOR PACKET
    // =========================================================
    //
    // Publishes SensorPacket to ThingSpeak.
    //
    // Automatically:
    // - splits fields across channels
    // - formats payloads
    // - handles invalid numeric values
    //
    // Returns:
    //
    // true  -> all publishes successful
    // false -> at least one publish failed
    //
    // =========================================================

    bool publishPacket(const SensorPacket &packet);

    // =========================================================
    // LAST MQTT HEALTH TIMESTAMP
    // =========================================================
    //
    // Timestamp of last known healthy MQTT connection.
    //
    // Used by:
    // - watchdog supervision
    // - reconnect logic
    //
    // =========================================================

    uint32_t lastOkMs() const;

    // =========================================================
    // LAST SUCCESSFUL PUBLISH TIMESTAMP
    // =========================================================
    //
    // Timestamp of last fully successful packet publish.
    //
    // Used to detect:
    // - half-dead MQTT states
    // - stalled publishing
    //
    // =========================================================

    uint32_t lastPublishOkMs() const;

    // =========================================================
    // FORCE RECOVERY
    // =========================================================
    //
    // Performs soft MQTT recovery.
    //
    // Current implementation:
    // - disconnect MQTT
    // - clear reconnect timer
    //
    // Next tick() will attempt reconnect.
    //
    // Used by:
    // - watchdogTask
    //
    // =========================================================

    void recover();

private:

    // =========================================================
    // PAYLOAD FIELD APPENDER
    // =========================================================
    //
    // Safely appends one ThingSpeak field into payload buffer.
    //
    // Handles:
    // - buffer overflow protection
    // - formatting
    // - invalid numeric values
    //
    // Returns:
    //
    // true  -> append successful
    // false -> overflow or formatting error
    //
    // =========================================================

    bool appendField(
        char* payload,
        size_t payloadSize,
        int &len,
        int fieldNumber,
        float value,
        bool valid,
        bool addAmpersand
    );

    // =========================================================
    // MQTT CLIENT
    // =========================================================
    //
    // Underlying PubSubClient instance.
    //
    // =========================================================

    PubSubClient _mqtt;

    // =========================================================
    // CONNECTION TIMESTAMPS
    // =========================================================
    //
    // _lastAttemptMs:
    //   last reconnect attempt
    //
    // _lastOkMs:
    //   last known healthy MQTT state
    //
    // _lastPublishOkMs:
    //   last successful publish
    //
    // =========================================================

    uint32_t _lastAttemptMs = 0;
    uint32_t _lastOkMs = 0;
    uint32_t _lastPublishOkMs = 0;

    // =========================================================
    // THINGSPEAK TOPICS
    // =========================================================
    //
    // Preformatted publish topics.
    //
    // Example:
    //
    //   channels/3389737/publish
    //
    // Prebuilt during begin() to avoid repeated formatting.
    //
    // =========================================================

    char _topic1[64];
    char _topic2[64];
    char _topic3[64];
};