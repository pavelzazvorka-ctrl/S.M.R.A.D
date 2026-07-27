#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

#include "AppConfig.h"
#include "SensorPacket.h"
#include "DebugLog.h"
#include "TimeSync.h"

// =============================================================
// SD LOGGER
// =============================================================
//
// Central SD card logging subsystem.
//
// Responsibilities:
// -------------------------------------------------------------
// - initialize SD card
// - maintain SD availability state
// - write event logs
// - write CSV measurement data
// - perform simple SD recovery
//
// Architecture:
// -------------------------------------------------------------
// All filesystem access should be centralized here.
//
// The intended design is:
//
//   sdTask
//       -> SdLogger
//
// Other tasks should NOT directly:
// - write CSV files
// - manipulate SD filesystem
// - open measurement files
//
// This avoids:
// - concurrent filesystem access
// - blocking real-time tasks
// - SD timing contention
// - filesystem corruption risk
//
// Logging Philosophy:
// -------------------------------------------------------------
// The implementation intentionally prioritizes:
//
// - robustness
// - deterministic behavior
// - crash tolerance
//
// over maximum SD throughput.
//
// Files are intentionally:
// - opened
// - written
// - closed
//
// for every operation.
//
// This is slower,
// but significantly safer for unattended embedded systems.
//
// Files:
// -------------------------------------------------------------
//
// /data.csv
//   measurement CSV data
//
// /events.log
//   operational event log
//
// =============================================================

class SdLogger
{
public:

    // =========================================================
    // INITIALIZATION
    // =========================================================
    //
    // Initializes:
    // - SPI bus
    // - SD card
    // - filesystem access
    //
    // Returns:
    //
    // true  -> SD initialized successfully
    // false -> SD unavailable
    //
    // =========================================================

    bool begin();

    // =========================================================
    // SD AVAILABILITY STATE
    // =========================================================
    //
    // Returns current software availability state.
    //
    // This is NOT a full hardware re-test.
    //
    // Used by:
    // - sdTask
    // - watchdog diagnostics
    //
    // =========================================================

    bool available() const;

    // =========================================================
    // ENSURE CSV HEADER
    // =========================================================
    //
    // Creates CSV header if data.csv does not exist yet.
    //
    // Safe to call repeatedly.
    //
    // =========================================================

    void ensureDataHeader();

    // =========================================================
    // EVENT LOGGER
    // =========================================================
    //
    // Appends one event line into:
    //
    //   /events.log
    //
    // Format:
    //
    //   millis,iso_time,event
    //
    // Example:
    //
    //   12345,2026-05-26 14:00:00,wifi recovery
    //
    // =========================================================

    bool logEvent(const char* eventText);

    // =========================================================
    // CSV MEASUREMENT LOGGER
    // =========================================================
    //
    // Writes one SensorPacket into:
    //
    //   /data.csv
    //
    // Automatically:
    // - formats CSV
    // - inserts timestamps
    // - handles invalid values
    //
    // =========================================================

    bool logPacketCsv(const SensorPacket &packet);

    // =========================================================
    // SD RECOVERY
    // =========================================================
    //
    // Attempts SD recovery if logger is currently unavailable.
    //
    // Recovery steps:
    // ---------------------------------------------------------
    // 1. reinitialize SD
    // 2. ensure CSV header exists
    // 3. log recovery event
    //
    // =========================================================

    void recoverIfNeeded();

private:

    // =========================================================
    // GENERIC LINE APPENDER
    // =========================================================
    //
    // Appends one text line into file.
    //
    // Used internally by:
    // - event logger
    //
    // =========================================================

    bool appendLine(const char* path, const char* line);

    // =========================================================
    // SD AVAILABILITY FLAG
    // =========================================================
    //
    // Indicates current software view of SD availability.
    //
    // Set false on:
    // - initialization failure
    // - open failure
    // - filesystem problems
    //
    // =========================================================

    bool _available = false;

    // =========================================================
    // FILE PATHS
    // =========================================================
    //
    // Centralized file paths used by the logger.
    //
    // =========================================================

    const char* _dataFile = "/data.csv";
    const char* _eventFile = "/events.log";
};


// =============================================================
// GLOBAL SD LOGGER INSTANCE
// =============================================================
//
// Shared SD logger instance used by the firmware.
//
// Example:
//
//   SdLog.logEvent("boot");
//
// =============================================================

extern SdLogger SdLog;