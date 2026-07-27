#pragma once

#include <Arduino.h>
#include <time.h>

#include "AppConfig.h"
#include "DebugLog.h"

// =============================================================
// TIME SYNC
// =============================================================
//
// Lightweight UTC NTP synchronization helper.
//
// Responsibilities:
// -------------------------------------------------------------
// - synchronize ESP32 system time using NTP
// - maintain time validity state
// - provide ISO timestamp formatting
// - track synchronization statistics
//
// Architecture:
// -------------------------------------------------------------
// Time synchronization scheduling is intentionally handled
// externally by:
//
//   timeTask
//
// TimeSync itself only performs:
//
// - one synchronization attempt
// - timestamp formatting
// - synchronization diagnostics
//
// Design Philosophy:
// -------------------------------------------------------------
// The implementation intentionally prioritizes:
//
// - simplicity
// - reliability
// - deterministic behavior
//
// over advanced RTC/timezone functionality.
//
// UTC ONLY:
// -------------------------------------------------------------
// SMRAD intentionally stores ALL timestamps in UTC.
//
// Reasons:
// -------------------------------------------------------------
// - avoids DST complications
// - avoids timezone inconsistencies
// - simplifies CSV processing
// - simplifies backend parsing
// - improves long-term maintainability
//
// Typical Usage:
// -------------------------------------------------------------
//
// if (Clock.sync())
// {
//     char ts[32];
//
//     Clock.isoTime(ts, sizeof(ts));
//
//     Log.printf("Time: %s", ts);
// }
//
// =============================================================

class TimeSync
{
public:

    // =========================================================
    // NTP SYNCHRONIZATION
    // =========================================================
    //
    // Performs one NTP synchronization attempt.
    //
    // Uses:
    //
    // - pool.ntp.org
    // - time.google.com
    // - time.cloudflare.com
    //
    // Returns:
    //
    // true  -> synchronization successful
    // false -> synchronization failed
    //
    // Failure is NOT fatal.
    //
    // =========================================================

    bool sync();

    // =========================================================
    // ISO TIMESTAMP FORMATTER
    // =========================================================
    //
    // Converts current UTC system time into readable text.
    //
    // Output format:
    //
    //   YYYY-MM-DD HH:MM:SS
    //
    // Example:
    //
    //   2026-05-26 14:32:18
    //
    // If time is unavailable:
    //
    //   NO_TIME
    //
    // is returned instead.
    //
    // Used by:
    // - SD CSV logger
    // - event logger
    // - diagnostics
    //
    // =========================================================

    void isoTime(char* buffer, size_t size);

    // =========================================================
    // TIME VALIDITY STATE
    // =========================================================
    //
    // Returns:
    //
    // true  -> valid synchronized UTC time available
    // false -> time unavailable or synchronization failed
    //
    // =========================================================

    bool valid() const;

    // =========================================================
    // LAST SUCCESSFUL SYNC TIMESTAMP
    // =========================================================
    //
    // Returns millis() timestamp of last successful sync.
    //
    // Useful for:
    // - diagnostics
    // - watchdog supervision
    // - telemetry analysis
    //
    // =========================================================

    uint32_t lastSyncMs() const;

    // =========================================================
    // SUCCESSFUL SYNC COUNTER
    // =========================================================
    //
    // Number of successful NTP synchronizations since boot.
    //
    // =========================================================

    uint32_t syncCount() const;

    // =========================================================
    // FAILED SYNC COUNTER
    // =========================================================
    //
    // Number of failed synchronization attempts since boot.
    //
    // Useful for:
    // - WiFi diagnostics
    // - NTP diagnostics
    // - uptime analysis
    //
    // =========================================================

    uint32_t failCount() const;

private:

    // =========================================================
    // TIME VALIDITY FLAG
    // =========================================================
    //
    // Indicates whether synchronized UTC time is currently valid.
    //
    // =========================================================

    bool _valid = false;

    // =========================================================
    // LAST SUCCESSFUL SYNC TIMESTAMP
    // =========================================================
    //
    // millis() timestamp of last successful NTP sync.
    //
    // =========================================================

    uint32_t _lastSyncMs = 0;

    // =========================================================
    // DIAGNOSTIC COUNTERS
    // =========================================================
    //
    // Synchronization statistics accumulated since boot.
    //
    // =========================================================

    uint32_t _syncCount = 0;
    uint32_t _failCount = 0;
};


// =============================================================
// GLOBAL TIME MANAGER INSTANCE
// =============================================================
//
// Shared TimeSync instance used by the firmware.
//
// Example:
//
//   Clock.sync();
//
// =============================================================

extern TimeSync Clock;