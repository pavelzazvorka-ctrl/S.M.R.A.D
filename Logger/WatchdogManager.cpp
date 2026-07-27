#include "WatchdogManager.h"

// =============================================================
// GLOBAL WATCHDOG INSTANCE
// =============================================================
//
// Single shared watchdog supervisor used by the whole firmware.
//
// Responsibilities:
// -------------------------------------------------------------
// - monitor FreeRTOS task heartbeats
// - supervise WiFi health
// - supervise MQTT health
// - supervise successful publish activity
// - supervise selected sensor health
// - perform soft recovery
// - perform controlled ESP32 restart if needed
//
// =============================================================

WatchdogManager Watchdog;

// =============================================================
// RTC BOOT COUNTER
// =============================================================
//
// Stored in RTC memory.
//
// Survives software reset and watchdog reset,
// but is cleared by full power loss.
//
// Useful for identifying restart loops.
//
// =============================================================

RTC_DATA_ATTR uint32_t WatchdogManager::_bootCounter = 0;

// =============================================================
// BEGIN
// =============================================================
//
// Initializes watchdog diagnostics.
//
// Responsibilities:
// -------------------------------------------------------------
// - increment RTC boot counter
// - read ESP32 reset reason
// - print boot diagnostics
// - initialize diagnostic log timer
//
// This does NOT start hardware watchdog.
// This is a software supervision layer.
//
// =============================================================

void WatchdogManager::begin()
{
    _bootCounter++;

    esp_reset_reason_t reason = esp_reset_reason();

    Log.printf("Boot #%lu", _bootCounter);

    Log.printf(
        "Reset reason: %s (%d)",
        resetReasonToText(reason),
        (int)reason
    );

    _lastDiagnosticLogMs = millis();    
    _lastRS485WarningMs = millis();
}

// =============================================================
// MARK TASK ALIVE
// =============================================================
//
// Updates heartbeat timestamp for one task.
//
// Each supervised task should call this regularly.
//
// Example:
//
//   Watchdog.markAlive(sensorHealth);
//
// =============================================================

void WatchdogManager::markAlive(TaskHealth &health)
{
    health.lastAliveMs = millis();
}

// =============================================================
// TASK STALE CHECK
// =============================================================
//
// Returns true if task heartbeat is older than configured timeout.
//
// lastAliveMs == 0 means task has not reported yet,
// therefore it is not considered stale.
//
// =============================================================

bool WatchdogManager::isTaskStale(
    const TaskHealth &health,
    uint32_t nowMs
)
{
    if (health.lastAliveMs == 0)
        return false;

    return (nowMs - health.lastAliveMs) > health.timeoutMs;
}

// =============================================================
// CHECK TASK HEARTBEATS
// =============================================================
//
// Verifies all registered task heartbeat structures.
//
// If any task misses its timeout,
// a controlled ESP32 restart is triggered.
//
// Why restart instead of trying to recover task:
// -------------------------------------------------------------
// Recreating individual FreeRTOS tasks safely is possible,
// but introduces more complexity and failure modes.
//
// For unattended sensor node:
//
//   controlled restart
//
// is usually safer than continuing in a partially broken state.
//
// =============================================================

void WatchdogManager::checkTasks(TaskHealth* list[], size_t count)
{
    uint32_t nowMs = millis();

    for (size_t i = 0; i < count; i++)
    {
        TaskHealth* health = list[i];

        if (!health)
            continue;

        if (!isTaskStale(*health, nowMs))
            continue;

        char reason[96];

        snprintf(
            reason,
            sizeof(reason),
            "%s timeout",
            health->name ? health->name : "unknown task"
        );

        restartNow(reason);
    }
}

// =============================================================
// CHECK CONNECTIVITY
// =============================================================
//
// Supervises WiFi, MQTT and successful publish activity.
//
// Recovery model:
// -------------------------------------------------------------
// 1. short outage:
//      do nothing, normal reconnect logic handles it
//
// 2. longer outage:
//      force subsystem recovery
//
// 3. excessive outage:
//      restart ESP32
//
// This avoids unnecessary resets during short network glitches,
// but prevents the node from staying half-dead forever.
//
// =============================================================

void WatchdogManager::checkConnectivity(
    WifiManager &wifi,
    MqttPublisher &mqtt
)
{
    uint32_t nowMs = millis();

    // ---------------------------------------------------------
    // WiFi supervision
    // ---------------------------------------------------------

    if (!wifi.connected())
    {
        uint32_t lastWifiOk = wifi.lastOkMs();

        if (lastWifiOk && (nowMs - lastWifiOk > WIFI_OFFLINE_RESTART_MS))
        {
            restartNow("WiFi offline too long");
        }

        if (lastWifiOk && (nowMs - lastWifiOk > WIFI_OFFLINE_RECOVERY_MS))
        {
            _wifiRecoveryCount++;

            Log.printf(
                "WATCHDOG: WiFi recovery #%lu",
                _wifiRecoveryCount
            );

            SdLog.logEvent("wifi recovery");

            wifi.recover();
        }
    }

    // ---------------------------------------------------------
    // MQTT supervision
    // ---------------------------------------------------------
    //
    // MQTT is checked only when WiFi is connected.
    // If WiFi is offline, MQTT cannot recover anyway.
    //
    // ---------------------------------------------------------

    if (!mqtt.connected() && wifi.connected())
    {
        uint32_t lastMqttOk = mqtt.lastOkMs();

        if (lastMqttOk && (nowMs - lastMqttOk > MQTT_OFFLINE_RESTART_MS))
        {
            restartNow("MQTT offline too long");
        }

        if (lastMqttOk && (nowMs - lastMqttOk > MQTT_OFFLINE_RECOVERY_MS))
        {
            _mqttRecoveryCount++;

            Log.printf(
                "WATCHDOG: MQTT recovery #%lu",
                _mqttRecoveryCount
            );

            SdLog.logEvent("mqtt recovery");

            mqtt.recover();
        }
    }

    // ---------------------------------------------------------
    // Successful publish supervision
    // ---------------------------------------------------------
    //
    // This detects half-dead states where:
    //
    // - WiFi may be connected
    // - MQTT may appear connected
    // - but publishes no longer succeed
    //
    // ---------------------------------------------------------

    uint32_t lastPublish = mqtt.lastPublishOkMs();

    if (lastPublish && (nowMs - lastPublish > NO_PUBLISH_RESTART_MS))
    {
        restartNow("No successful publish too long");
    }
}

// =============================================================
// CHECK SENSOR HEALTH
// =============================================================

void WatchdogManager::checkSensors(BaseSensorManager &sensors)
{
    if (!sensors.status().rs485)
        return;

    uint32_t lastRS485Ms = sensors.lastValidRS485Ms();

   if (!lastRS485Ms)
        return;

    uint32_t nowMs = millis();

    if (nowMs - lastRS485Ms <= RS485_NO_VALID_WARN_MS)
        return;

    if (nowMs - _lastRS485WarningMs <= RS485_NO_VALID_WARN_MS)
        return;

    _lastRS485WarningMs = nowMs;

    Log.printf( "WATCHDOG WARNING: RS485 no valid data for %lu seconds", (nowMs - lastRS485Ms) / 1000UL );
    SdLog.logEvent("RS485 no valid data warning");
}

// =============================================================
// STACK FREE HELPER
// =============================================================
//
// Returns FreeRTOS stack high-water mark for a task.
//
// Value is useful for tuning stack sizes.
//
// 0 means:
// - invalid handle
// - task not created yet
//
// =============================================================

static uint32_t stackFree(TaskHandle_t handle)
{
    if (!handle)
        return 0;

    return (uint32_t)uxTaskGetStackHighWaterMark(handle);
}

// =============================================================
// LOG DIAGNOSTICS
// =============================================================
//
// Periodically prints system health diagnostics.
//
// Logged information:
// -------------------------------------------------------------
// - free heap
// - minimum heap since boot
// - stack high-water marks
// - WiFi state
// - MQTT state
// - SD state
//
// This is one of the most useful tools for long-term stability
// tuning.
//
// =============================================================

void WatchdogManager::logDiagnostics(
    TaskHandle_t sensorTask,
    TaskHandle_t networkTask,
    TaskHandle_t ledTask,
    TaskHandle_t timeTask,
    TaskHandle_t sdTask,
    TaskHandle_t watchdogTask,
    WifiManager &wifi,
    MqttPublisher &mqtt,
    SdLogger &sd
)
{
    uint32_t nowMs = millis();

    if (nowMs - _lastDiagnosticLogMs < DIAGNOSTIC_LOG_PERIOD_MS)
        return;

    _lastDiagnosticLogMs = nowMs;

    Log.printf(
        "DIAG heap free=%lu min=%lu",
        (uint32_t)ESP.getFreeHeap(),
        (uint32_t)ESP.getMinFreeHeap()
    );

    Log.printf(
        "DIAG stack sensor=%lu network=%lu led=%lu time=%lu sd=%lu watchdog=%lu",
        stackFree(sensorTask),
        stackFree(networkTask),
        stackFree(ledTask),
        stackFree(timeTask),
        stackFree(sdTask),
        stackFree(watchdogTask)
    );

    Log.printf(
        "DIAG WiFi=%s MQTT=%s SD=%s",
        wifi.connected() ? "OK" : "OFF",
        mqtt.connected() ? "OK" : "OFF",
        sd.available() ? "OK" : "OFF"
    );
}

// =============================================================
// CONTROLLED RESTART
// =============================================================
//
// Performs controlled ESP32 restart.
//
// Restart sequence:
// -------------------------------------------------------------
// 1. print restart reason
// 2. attempt SD event log
// 3. wait briefly so Serial/SD can flush
// 4. restart ESP32
//
// This is safer than silent crash or undefined state.
//
// =============================================================

void WatchdogManager::restartNow(const char* reason)
{
    const char* safeReason = reason ? reason : "unknown reason";

    Log.printf("WATCHDOG RESTART: %s", safeReason);

    SdLog.logEvent(safeReason);

    delay(250);

    ESP.restart();
}

// =============================================================
// RESET REASON TO TEXT
// =============================================================
//
// Converts ESP32 reset reason enum to readable text.
//
// Used at boot to diagnose:
//
// - power loss
// - software restart
// - watchdog reset
// - panic
// - brownout
//
// =============================================================

const char* WatchdogManager::resetReasonToText(esp_reset_reason_t reason)
{
    switch (reason)
    {
        case ESP_RST_POWERON:
            return "POWERON";

        case ESP_RST_EXT:
            return "EXTERNAL";

        case ESP_RST_SW:
            return "SOFTWARE";

        case ESP_RST_PANIC:
            return "PANIC";

        case ESP_RST_INT_WDT:
            return "INT_WDT";

        case ESP_RST_TASK_WDT:
            return "TASK_WDT";

        case ESP_RST_WDT:
            return "OTHER_WDT";

        case ESP_RST_DEEPSLEEP:
            return "DEEPSLEEP";

        case ESP_RST_BROWNOUT:
            return "BROWNOUT";

        case ESP_RST_SDIO:
            return "SDIO";

        default:
            return "UNKNOWN";
    }
}