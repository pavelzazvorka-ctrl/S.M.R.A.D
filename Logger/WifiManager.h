#pragma once

#include <Arduino.h>
#include <WiFi.h>

#include "AppConfig.h"
#include "DebugLog.h"

// =============================================================
// WIFI MANAGER
// =============================================================
//
// Lightweight non-blocking WiFi connection manager.
//
// Responsibilities:
// -------------------------------------------------------------
// - maintain WiFi connection
// - perform reconnect attempts
// - track WiFi health
// - support watchdog recovery
//
// Architecture:
// -------------------------------------------------------------
// WiFiManager intentionally centralizes ALL WiFi control.
//
// Other modules should NOT directly:
//
// - call WiFi.begin()
// - disconnect WiFi
// - manipulate reconnect logic
//
// This prevents:
// - duplicated reconnect attempts
// - conflicting WiFi state handling
// - hidden networking behavior
//
// Design Philosophy:
// -------------------------------------------------------------
// The implementation intentionally prioritizes:
//
// - simplicity
// - readability
// - deterministic behavior
// - operational stability
//
// WiFi handling is implemented as:
//
//   non-blocking state machine
//
// inside:
//
//   tick()
//
// This avoids:
// - blocking loops
// - scheduler stalls
// - sensor timing disruption
//
// Typical Usage:
// -------------------------------------------------------------
//
// networkTask:
//
//   while (true)
//   {
//       Wifi.tick();
//
//       vTaskDelay(...);
//   }
//
// =============================================================

class WifiManager
{
public:

    // =========================================================
    // WIFI STATE MACHINE
    // =========================================================
    //
    // Non-blocking WiFi maintenance function.
    //
    // Typically called periodically by:
    //
    //   networkTask
    //
    // Responsibilities:
    // ---------------------------------------------------------
    // - detect connected state
    // - initiate reconnect attempts
    // - retry failed connections
    // - update WiFi health timestamp
    //
    // Behavior:
    // ---------------------------------------------------------
    //
    // connected:
    //   update last-known-good timestamp
    //
    // disconnected:
    //   start WiFi.begin()
    //
    // timeout:
    //   reset WiFi and retry later
    //
    // =========================================================

    void tick();

    // =========================================================
    // CONNECTION STATE
    // =========================================================
    //
    // Returns:
    //
    // true  -> WiFi connected
    // false -> WiFi disconnected
    //
    // Used by:
    // - networkTask
    // - heartbeatTask
    // - watchdogTask
    // - timeTask
    //
    // =========================================================

    bool connected() const;

    // =========================================================
    // LAST HEALTHY WIFI TIMESTAMP
    // =========================================================
    //
    // Returns millis() timestamp of last known connected state.
    //
    // Used by:
    // - watchdog supervision
    // - recovery logic
    // - diagnostics
    //
    // =========================================================

    uint32_t lastOkMs() const;

    // =========================================================
    // SOFT WIFI RECOVERY
    // =========================================================
    //
    // Performs controlled WiFi reset.
    //
    // Used by watchdog after extended outage.
    //
    // Recovery sequence:
    // ---------------------------------------------------------
    // - disconnect WiFi
    // - clear internal connection state
    // - allow next tick() to reconnect
    //
    // This does NOT restart ESP32.
    //
    // =========================================================

    void recover();

private:

    // =========================================================
    // CONNECTION-IN-PROGRESS FLAG
    // =========================================================
    //
    // Indicates whether WiFi.begin() has already been started.
    //
    // Prevents repeated WiFi.begin() spam every tick().
    //
    // =========================================================

    bool _connecting = false;

    // =========================================================
    // CONNECTION ATTEMPT START TIMESTAMP
    // =========================================================
    //
    // millis() timestamp of current connection attempt start.
    //
    // Used for timeout/retry logic.
    //
    // =========================================================

    uint32_t _startMs = 0;

    // =========================================================
    // LAST HEALTHY WIFI TIMESTAMP
    // =========================================================
    //
    // millis() timestamp of last known connected state.
    //
    // Used by watchdog to determine:
    //
    // - how long WiFi has been offline
    // - when recovery should happen
    // - when ESP32 restart is required
    //
    // =========================================================

    uint32_t _lastOkMs = 0;
};


// =============================================================
// GLOBAL WIFI MANAGER INSTANCE
// =============================================================
//
// Shared WiFi manager instance used by the firmware.
//
// Example:
//
//   Wifi.tick();
//
// =============================================================

extern WifiManager Wifi;