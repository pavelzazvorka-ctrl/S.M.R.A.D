#pragma once

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#include "AppConfig.h"

// =============================================================
// LED STATUS SYSTEM
// =============================================================
//
// Centralized RGB status LED controller.
//
// Responsibilities:
// -------------------------------------------------------------
// - system heartbeat indication
// - WiFi status indication
// - MQTT status indication
// - publish activity indication
// - future fatal/error indication
//
// Architecture:
// -------------------------------------------------------------
// LED ownership is intentionally centralized.
//
// Only LedStatus should directly manipulate NeoPixel hardware.
//
// This prevents:
// - concurrent NeoPixel access
// - conflicting colors
// - race conditions
// - timing glitches
//
// heartbeatTask periodically calls:
//
//   update()
//
// Other modules may only request temporary indications through:
//
//   showSentBlink()
//
// Design Philosophy:
// -------------------------------------------------------------
// The LED system intentionally prioritizes:
//
// - operational clarity
// - deterministic behavior
// - simple diagnostics
//
// over visual complexity.
//
// The goal is:
//
// immediate field diagnostics
//
// without requiring:
// - Serial console
// - MQTT monitoring
// - debugger access
//
// =============================================================


// =============================================================
// LOGICAL LED STATES
// =============================================================
//
// These are abstract logical states.
//
// Actual RGB values are defined inside:
//
//   LedStatus::setRaw()
//
// This separation keeps:
// - hardware mapping centralized
// - behavior readable
// - future color changes easy
//
// =============================================================

enum LedState
{
    // LED disabled / blink off phase
    LED_OFF,

    // System operational
    // WiFi + MQTT connected
    LED_OK,

    // SD disconnected
    LED_SD,

    // WiFi disconnected
    LED_WIFI,

    // MQTT disconnected
    LED_MQTT,

    // Reserved for fatal/error states
    LED_ERROR,

    // Short successful publish indication
    LED_SENT
};


// =============================================================
// LED STATUS CONTROLLER
// =============================================================
//
// Thread-safe RGB LED controller.
//
// Main behavior:
// -------------------------------------------------------------
// heartbeatTask periodically calls:
//
//   update(wifiOk, mqttOk)
//
// update() internally decides:
// - which logical state should be visible
// - whether blink phase is on/off
// - whether temporary publish flash is active
//
// =============================================================

class LedStatus
{
public:

    // =========================================================
    // INITIALIZATION
    // =========================================================
    //
    // Initializes:
    // - NeoPixel driver
    // - internal mutex
    // - LED hardware state
    //
    // Must be called before update().
    //
    // =========================================================

    void begin();

    // =========================================================
    // PUBLISH FEEDBACK BLINK
    // =========================================================
    //
    // Requests a short temporary LED flash indicating
    // successful MQTT publish.
    //
    // Does NOT directly write to the LED.
    //
    // Instead:
    // - stores timeout timestamp
    // - update() later displays LED_SENT
    //
    // This preserves centralized LED ownership.
    //
    // =========================================================

    void showSentBlink();

    // =========================================================
    // UPDATE LED STATE MACHINE
    // =========================================================
    //
    // Main heartbeat update function.
    //
    // Typically called periodically by heartbeatTask.
    //
    // Parameters:
    //
    // wifiOk:
    //   true if WiFi connected
    //
    // mqttOk:
    //   true if MQTT connected
    //
    // =========================================================

    void update(bool wifiOk, bool mqttOk, bool sdOk, bool sentOk);

private:

    // =========================================================
    // LOW-LEVEL LED WRITER
    // =========================================================
    //
    // Physically writes RGB values to NeoPixel hardware.
    //
    // Only this function should directly touch _rgb.
    //
    // =========================================================

    void setRaw(LedState state);

    // =========================================================
    // RGB LED DRIVER
    // =========================================================
    //
    // Single onboard NeoPixel.
    //
    // Configuration:
    //
    // - one pixel
    // - LED_PIN from AppConfig.h
    // - GRB color order
    // - 800 kHz timing
    //
    // =========================================================

    Adafruit_NeoPixel _rgb =
        Adafruit_NeoPixel(
            1,
            LED_PIN,
            NEO_GRB + NEO_KHZ800
        );

    // =========================================================
    // LED ACCESS MUTEX
    // =========================================================
    //
    // Protects NeoPixel access from concurrent task usage.
    //
    // Mostly defensive protection for future expansion.
    //
    // =========================================================

    SemaphoreHandle_t _mutex = nullptr;

    // =========================================================
    // TEMPORARY PUBLISH BLINK TIMER
    // =========================================================
    //
    // millis() timestamp until which LED_SENT remains active.
    //
    // =========================================================

    uint32_t _sentBlinkUntilMs = 0;

    // =========================================================
    // HEARTBEAT BLINK PHASE
    // =========================================================
    //
    // Toggles on every update().
    //
    // Creates simple heartbeat blinking behavior.
    //
    // =========================================================

    bool _blink = false;

    // =========================================================
    // LAST PHYSICAL LED STATE
    // =========================================================
    //
    // Used to avoid unnecessary NeoPixel updates.
    //
    // If new state equals previous state:
    // - no hardware write is performed
    //
    // This reduces:
    // - unnecessary timing-sensitive writes
    // - LED flicker
    // - CPU overhead
    //
    // =========================================================

    LedState _lastState = LED_OFF;
};


// =============================================================
// GLOBAL LED CONTROLLER INSTANCE
// =============================================================
//
// Shared LED controller instance used by the firmware.
//
// Example:
//
//   StatusLed.showSentBlink();
//
// =============================================================

extern LedStatus StatusLed;