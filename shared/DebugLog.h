#pragma once

#include <Arduino.h>
#include <stdarg.h>

// AppConfig.h may define SERIAL_DEBUG before this header is included.
#ifndef SERIAL_DEBUG
#define SERIAL_DEBUG 1
#endif

#ifndef SERIAL_MONITOR
#define SERIAL_MONITOR 1
#endif

// =============================================================
// DEBUG LOG
// =============================================================

class DebugLog
{
public:

    void begin(Stream &serial, uint32_t baud);
    void printf(const char* fmt, ...);
    void monitor(const char* fmt, ...);
    
private:

    Stream* _serial = nullptr;
    SemaphoreHandle_t _mutex = nullptr;
    void vprintf(const char* fmt, va_list args);
};

// =============================================================
// GLOBAL LOGGER INSTANCE
// =============================================================

extern DebugLog Log;