#include "DebugLog.h"
#include <stdarg.h>

// =============================================================
// GLOBAL LOGGER INSTANCE
// =============================================================
//
// Single shared logger used by the entire firmware.
//
// Example:
//
//   Log.printf("WiFi OK");
//
// =============================================================

DebugLog Log;

// =============================================================
// BEGIN
// =============================================================

void DebugLog::begin(Stream &serial, uint32_t baud)
{
    // baud intentionally unused
    // Serial.begin() is handled externally
    (void)baud;

    _serial = &serial;

    // Small startup delay helps stabilize USB CDC Serial
    // on some ESP32-S3 boards after boot.
    delay(300);

    // Mutex protects shared Serial output between tasks.
    _mutex = xSemaphoreCreateMutex();
}

// =============================================================
// THREAD-SAFE PART
// =============================================================

void DebugLog::vprintf(const char* fmt, va_list args)
{
    // Logger not initialized yet.
    if (!_serial || !_mutex)
        return;

    // Local formatting buffer.
    char buffer[384];

    // Standard printf-style formatting.
    vsnprintf(
        buffer,
        sizeof(buffer),
        fmt,
        args
    );

    // Wait briefly for exclusive Serial access.
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(100)))
    {
        // Print entire line atomically.
        _serial->println(buffer);

        // Release Serial ownership.
        xSemaphoreGive(_mutex);
    }
}

void DebugLog::monitor(const char* fmt, ...)
{
#if SERIAL_MONITOR
    va_list args;

    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
#else
    (void)fmt;
#endif
}

void DebugLog::printf(const char* fmt, ...)
{
#if SERIAL_DEBUG
    va_list args;

    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
#else
    (void)fmt;
#endif
}