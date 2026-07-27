#include "TimeSync.h"

// =============================================================
// GLOBAL TIME SYNCHRONIZATION INSTANCE
// =============================================================
//
// Shared system time manager used by the firmware.
//
// Responsibilities:
// -------------------------------------------------------------
// - NTP synchronization
// - UTC system clock maintenance
// - ISO timestamp formatting
// - synchronization diagnostics
//
// Used by:
// - SD logger
// - watchdog diagnostics
// - telemetry timestamps
//
// =============================================================

TimeSync Clock;

// =============================================================
// NTP SYNCHRONIZATION
// =============================================================
//
// Performs one NTP synchronization attempt.
//
// Timezone Strategy:
// -------------------------------------------------------------
// UTC is intentionally used:
//
//   GMT offset = 0
//   DST offset = 0
//
// Reasons:
// -------------------------------------------------------------
// - avoids DST complications
// - avoids timezone inconsistencies
// - simplifies CSV analysis
// - simplifies backend processing
// - improves long-term maintainability
//
// NTP Servers:
// -------------------------------------------------------------
// Multiple servers improve reliability:
//
// - pool.ntp.org
// - time.google.com
// - time.cloudflare.com
//
// Failure Handling:
// -------------------------------------------------------------
// Failure is NOT fatal.
//
// The firmware continues operating even without valid time.
//
// =============================================================

bool TimeSync::sync()
{
    // Configure SNTP subsystem.
    configTime(
        0, // GMT offset
        0, // DST offset
        "pool.ntp.org",
        "time.google.com",
        "time.cloudflare.com"
    );

    struct tm timeInfo;

    // Wait for valid NTP time.
    if (!getLocalTime(&timeInfo, TIME_SYNC_TIMEOUT_MS))
    {
        Log.printf("NTP sync failed");

        _valid = false;
        _failCount++;

        return false;
    }

    // Read synchronized Unix timestamp.
    time_t now;
    time(&now);

    // Generate readable ISO timestamp for diagnostics.
    char ts[32];
    isoTime(ts, sizeof(ts));

    Log.printf(
        "NTP sync OK: %lu %s",
        (uint32_t)now,
        ts
    );

    _valid = true;
    _syncCount++;
    _lastSyncMs = millis();

    return true;
}

// =============================================================
// ISO TIMESTAMP FORMATTER
// =============================================================
//
// Converts current system time into ISO-like text format.
//
// Output example:
//
//   2026-05-26 14:32:18
//
// Used by:
// - CSV logger
// - event logger
// - diagnostics
//
// Failure Handling:
// -------------------------------------------------------------
// If system time is unavailable:
//
//   "NO_TIME"
//
// is returned.
//
// This intentionally makes invalid timestamps obvious
// inside logs and CSV files.
//
// =============================================================

void TimeSync::isoTime(char* buffer, size_t size)
{
    if (!buffer || size == 0)
        return;

    struct tm timeInfo;

    if (!getLocalTime(&timeInfo))
    {
        snprintf(buffer, size, "NO_TIME");
        return;
    }

    strftime(
        buffer,
        size,
        "%Y-%m-%d %H:%M:%S",
        &timeInfo
    );
}

// =============================================================
// TIME VALIDITY STATE
// =============================================================
//
// Returns:
//
// true  -> valid synchronized system time available
// false -> time unavailable or synchronization failed
//
// =============================================================

bool TimeSync::valid() const
{
    return _valid;
}

// =============================================================
// LAST SUCCESSFUL SYNC TIMESTAMP
// =============================================================
//
// Returns millis() timestamp of last successful NTP sync.
//
// Used by:
// - diagnostics
// - watchdog supervision
// - telemetry monitoring
//
// =============================================================

uint32_t TimeSync::lastSyncMs() const
{
    return _lastSyncMs;
}

// =============================================================
// SUCCESSFUL SYNC COUNTER
// =============================================================
//
// Number of successful NTP synchronizations
// since boot.
//
// Useful for:
// - diagnostics
// - uptime analysis
//
// =============================================================

uint32_t TimeSync::syncCount() const
{
    return _syncCount;
}

// =============================================================
// FAILED SYNC COUNTER
// =============================================================
//
// Number of failed NTP synchronization attempts
// since boot.
//
// Useful for:
// - WiFi diagnostics
// - NTP reliability analysis
// - backend troubleshooting
//
// =============================================================

uint32_t TimeSync::failCount() const
{
    return _failCount;
}