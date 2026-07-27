#include "WifiManager.h"
#include "ConfigManager.h"

// =============================================================
// GLOBAL WIFI MANAGER INSTANCE
// =============================================================
//
// Single shared WiFi manager used by the firmware.
//
// Responsibilities:
// -------------------------------------------------------------
// - maintain WiFi connection
// - perform reconnect attempts
// - track last healthy WiFi timestamp
// - provide soft recovery for watchdog
//
// =============================================================

WifiManager Wifi;

// =============================================================
// WIFI TICK
// =============================================================
//
// Non-blocking WiFi state machine.
//
// Called periodically from:
//
//   networkTask
//
// Responsibilities:
// -------------------------------------------------------------
// - detect connected state
// - start connection attempt when disconnected
// - retry connection after timeout
// - keep reconnect logic centralized
//
// Design Philosophy:
// -------------------------------------------------------------
// WiFi handling is intentionally simple.
//
// No blocking loops.
// No infinite wait for connection.
// No WiFi logic inside sensorTask.
//
// This prevents WiFi problems from disturbing sensor timing.
//
// State Machine:
// -------------------------------------------------------------
//
// CONNECTED:
//   - clear connecting flag
//   - update _lastOkMs
//
// DISCONNECTED + not connecting:
//   - configure WiFi STA mode
//   - disable WiFi sleep
//   - start WiFi.begin()
//
// DISCONNECTED + connecting too long:
//   - force disconnect
//   - clear connecting flag
//   - allow next retry cycle
//
// =============================================================

void WifiManager::tick()
{
    uint32_t nowMs = millis();

    // ---------------------------------------------------------
    // Connected state
    // ---------------------------------------------------------
    //
    // WiFi is healthy.
    // Update last known OK timestamp for watchdog supervision.
    //
    // ---------------------------------------------------------

    if (WiFi.status() == WL_CONNECTED)
    {
        _connecting = false;
        _lastOkMs = nowMs;
        return;
    }

    // ---------------------------------------------------------
    // Start new connection attempt
    // ---------------------------------------------------------
    //
    // We are disconnected and no connection attempt is active.
    //
    // WiFi.setSleep(false) is important for MQTT stability.
    // ESP32 WiFi power save can introduce latency and disconnects.
    //
    // ---------------------------------------------------------

    if (!_connecting)
    {
        WiFi.mode(WIFI_STA);
        WiFi.setSleep(false);
        
        const RuntimeConfig& cfg = Config.get();
        WiFi.begin(cfg.wifi.ssid, cfg.wifi.password);

        _connecting = true;
        _startMs = nowMs;

        Log.printf("WiFi connecting...");
        return;
    }

    // ---------------------------------------------------------
    // Retry connection if attempt takes too long
    // ---------------------------------------------------------
    //
    // WiFi.begin() may sometimes get stuck in a half-open state.
    //
    // Force disconnect after WIFI_RETRY_MS and let next tick()
    // start a clean attempt.
    //
    // ---------------------------------------------------------

    if (nowMs - _startMs > WIFI_RETRY_MS)
    {
        WiFi.disconnect(true);

        _connecting = false;
        _startMs = 0;

        Log.printf("WiFi retry");
    }
}

// =============================================================
// CONNECTED
// =============================================================
//
// Returns current WiFi state.
//
// Used by:
// - networkTask
// - heartbeatTask
// - watchdogTask
// - timeTask
//
// =============================================================

bool WifiManager::connected() const
{
    return WiFi.status() == WL_CONNECTED;
}

// =============================================================
// LAST OK TIMESTAMP
// =============================================================
//
// Returns millis() timestamp of last known connected state.
//
// Used by watchdog to determine:
//
// - how long WiFi has been offline
// - when to attempt recovery
// - when to restart ESP32
//
// =============================================================

uint32_t WifiManager::lastOkMs() const
{
    return _lastOkMs;
}

// =============================================================
// SOFT RECOVERY
// =============================================================
//
// Called by watchdog after extended WiFi outage.
//
// Performs a soft WiFi reset:
//
// - disconnect current WiFi state
// - clear internal connection attempt flag
// - allow next tick() to start fresh WiFi.begin()
//
// This does NOT restart ESP32.
//
// =============================================================

void WifiManager::recover()
{
    WiFi.disconnect(true);
    delay(200);

    _connecting = false;
    _startMs = 0;
}