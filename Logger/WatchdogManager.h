#pragma once

#include <Arduino.h>
#include <esp_system.h>

#include "AppConfig.h"
#include "DebugLog.h"

#include "WifiManager.h"
#include "MqttPublisher.h"
#include "BaseSensorManager.h"
#include "SdLogger.h"

// =============================================================
// WATCHDOG MANAGER
// =============================================================
//
// High-level software supervision layer for the SMRAD firmware.
//
// Responsibilities:
// -------------------------------------------------------------
// - monitor FreeRTOS task heartbeats
// - supervise WiFi health
// - supervise MQTT health
// - supervise successful publish activity
// - supervise selected sensor health
// - log diagnostics
// - perform controlled recovery
// - restart ESP32 when necessary
//
// Important:
// -------------------------------------------------------------
// This is NOT the ESP32 hardware watchdog.
//
// ESP32 already contains:
// - interrupt watchdog
// - task watchdog
// - RTC watchdog
//
// WatchdogManager adds an additional:
//   application-level supervision layer
//
// Why this layer exists:
// -------------------------------------------------------------
// Hardware watchdogs detect:
//
// - CPU lockups
// - stalled scheduler
// - deadlocked tasks
//
// but they usually do NOT detect:
//
// - dead MQTT sessions
// - stuck WiFi states
// - failed publishes
// - frozen logical tasks
// - half-dead subsystems
//
// Example half-dead state:
// -------------------------------------------------------------
// - WiFi connected
// - MQTT appears connected
// - no publishes succeed anymore
//
// Hardware watchdog would NOT reset this.
//
// WatchdogManager WILL detect it.
//
// Recovery Philosophy:
// -------------------------------------------------------------
// The system intentionally prefers:
//
//   controlled restart
//
// over:
//
//   undefined degraded operation
//
// For unattended embedded telemetry nodes,
// this is usually the safest strategy.
//
// =============================================================


// =============================================================
// TASK HEALTH STRUCTURE
// =============================================================
//
// Simple heartbeat descriptor used for task supervision.
//
// Every supervised task periodically calls:
//
//   Watchdog.markAlive(...)
//
//
// Fields:
// -------------------------------------------------------------
//
// name:
//   human-readable task name
//
// lastAliveMs:
//   last heartbeat timestamp
//
// timeoutMs:
//   maximum allowed silence interval
//
// =============================================================

struct TaskHealth
{
    // Human-readable task name.
    const char* name;

    // Updated periodically by the task itself.
    volatile uint32_t lastAliveMs;

    // Maximum allowed inactivity time.
    uint32_t timeoutMs;
};


// =============================================================
// WATCHDOG MANAGER
// =============================================================
//
// Centralized system supervisor.
//
// Architecture:
// -------------------------------------------------------------
// Only WatchdogManager should decide when:
//
// - subsystem recovery happens
// - ESP restart happens
//
// This avoids:
// - conflicting recovery logic
// - duplicated restart logic
// - hidden failure handling
//
// =============================================================

class WatchdogManager
{
public:

    // =========================================================
    // INITIALIZATION
    // =========================================================
    //
    // Responsibilities:
    // ---------------------------------------------------------
    // - increment boot counter
    // - read ESP32 reset reason
    // - initialize diagnostic timers
    // - print boot diagnostics
    //
    // =========================================================

    void begin();

    // =========================================================
    // TASK HEARTBEAT UPDATE
    // =========================================================
    //
    // Updates heartbeat timestamp for one task.
    //
    // Example:
    //
    //   Watchdog.markAlive(sensorHealth);
    //
    // =========================================================

    void markAlive(TaskHealth &health);

    // =========================================================
    // TASK SUPERVISION
    // =========================================================
    //
    // Verifies heartbeat freshness for all registered tasks.
    //
    // If a task exceeds timeout:
    //
    //   controlled restart is triggered
    //
    // =========================================================

    void checkTasks(
        TaskHealth* list[],
        size_t count
    );

    // =========================================================
    // CONNECTIVITY SUPERVISION
    // =========================================================
    //
    // Supervises:
    // - WiFi
    // - MQTT
    // - successful publish activity
    //
    // Recovery policy:
    // ---------------------------------------------------------
    // short outage:
    //   no action
    //
    // medium outage:
    //   subsystem recovery
    //
    // long outage:
    //   full ESP restart
    //
    // =========================================================

    void checkConnectivity(
        WifiManager &wifi,
        MqttPublisher &mqtt
    );

    // =========================================================
    // SENSOR SUPERVISION
    // =========================================================
    //
    // Performs logical sensor health checks.
    //
    // Current implementation:
    // ---------------------------------------------------------
    // CO2 sensor must occasionally produce valid data.
    //
    // =========================================================

    void checkSensors(
        BaseSensorManager &sensors
    );

    // =========================================================
    // PERIODIC DIAGNOSTIC LOGGING
    // =========================================================
    //
    // Periodically prints:
    //
    // - heap usage
    // - minimum heap
    // - stack high-water marks
    // - WiFi state
    // - MQTT state
    // - SD state
    //
    // Extremely useful for:
    // ---------------------------------------------------------
    // - long-term stability analysis
    // - stack tuning
    // - memory leak detection
    // - field diagnostics
    //
    // =========================================================

    void logDiagnostics(
        TaskHandle_t sensorTask,
        TaskHandle_t networkTask,
        TaskHandle_t ledTask,
        TaskHandle_t timeTask,
        TaskHandle_t sdTask,
        TaskHandle_t watchdogTask,
        WifiManager &wifi,
        MqttPublisher &mqtt,
        SdLogger &sd
    );

    // =========================================================
    // CONTROLLED RESTART
    // =========================================================
    //
    // Performs controlled ESP32 restart.
    //
    // Restart sequence:
    // ---------------------------------------------------------
    // - log reason
    // - attempt SD log
    // - short flush delay
    // - ESP.restart()
    //
    // =========================================================

    void restartNow(const char* reason);

    // =========================================================
    // RESET REASON FORMATTER
    // =========================================================
    //
    // Converts ESP32 reset reason enum into readable text.
    //
    // Useful for:
    // - boot diagnostics
    // - restart analysis
    // - watchdog debugging
    //
    // =========================================================

    const char* resetReasonToText(
        esp_reset_reason_t reason
    );

private:

    // =========================================================
    // TASK STALE CHECK
    // =========================================================
    //
    // Returns:
    //
    // true  -> task heartbeat expired
    // false -> task still healthy
    //
    // =========================================================

    bool isTaskStale(
        const TaskHealth &health,
        uint32_t nowMs
    );

    // =========================================================
    // DIAGNOSTIC TIMERS
    // =========================================================
    //
    // Internal timestamps controlling:
    // - periodic diagnostics
    // - warning throttling
    //
    // =========================================================

    uint32_t _lastDiagnosticLogMs = 0;

    // =========================================================
    // RECOVERY COUNTERS
    // =========================================================
    //
    // Counts performed subsystem recoveries.
    //
    // Useful for:
    // - stability diagnostics
    // - field telemetry
    // - recovery loop detection
    //
    // =========================================================

    uint32_t _wifiRecoveryCount = 0;
    uint32_t _mqttRecoveryCount = 0;

    // =========================================================
    // WARNING THROTTLING
    // =========================================================
    //
    // Prevents excessive repeated RS485 warning spam.
    //
    // =========================================================

    uint32_t _lastRS485WarningMs = 0;

    // =========================================================
    // RTC BOOT COUNTER
    // =========================================================
    //
    // Stored in RTC memory.
    //
    // Survives:
    // - software reset
    // - watchdog reset
    //
    // Cleared by:
    // - full power loss
    //
    // Useful for:
    // - reboot loop diagnostics
    // - long-term stability analysis
    //
    // =========================================================

    RTC_DATA_ATTR static uint32_t _bootCounter;
};


// =============================================================
// GLOBAL WATCHDOG INSTANCE
// =============================================================
//
// Shared watchdog supervisor used by the firmware.
//
// Example:
//
//   Watchdog.checkTasks(...);
//
// =============================================================

extern WatchdogManager Watchdog;