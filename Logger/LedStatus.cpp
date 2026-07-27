#include "LedStatus.h"

// =============================================================
// GLOBAL LED STATUS INSTANCE
// =============================================================
//
// Single shared LED controller used by the firmware.
//
// The object owns the NeoPixel instance and exposes only simple
// high-level status operations:
//
// - begin()
// - update()
// - showSentBlink()
//
// Other modules should not write to the RGB LED directly.
//
// =============================================================

LedStatus StatusLed;

// =============================================================
// BEGIN
// =============================================================
//
// Initializes the RGB status LED subsystem.
//
// Responsibilities:
// -------------------------------------------------------------
// - create mutex for NeoPixel access
// - initialize NeoPixel driver
// - clear LED state
// - physically turn LED off
//
// Why mutex:
// -------------------------------------------------------------
// NeoPixel timing is sensitive.
//
// Even though the intended design is that only heartbeatTask
// calls update(), a mutex is still used as defensive protection
// against accidental future access from another task.
//
// Design rule:
// -------------------------------------------------------------
// Only LedStatus should directly access:
//
//   _rgb
//
// =============================================================

void LedStatus::begin()
{
    _mutex = xSemaphoreCreateMutex();

    _rgb.begin();
    _rgb.clear();
    _rgb.show();
}

// =============================================================
// SHOW SENT BLINK
// =============================================================
//
// Requests a short visual indication of successful publish.
//
// This function does NOT write to the LED immediately.
//
// Instead it sets a timestamp:
//
//   _sentBlinkUntilMs
//
// heartbeatTask will later call update(), and update() will
// temporarily show LED_SENT while the timestamp is still active.
//
// Why this design:
// -------------------------------------------------------------
// networkTask can request publish feedback without directly
// touching NeoPixel hardware.
//
// This preserves LED ownership inside LedStatus / heartbeatTask.
//
// =============================================================

void LedStatus::showSentBlink()
{
    _sentBlinkUntilMs = millis() + 150UL;
}

// =============================================================
// UPDATE
// =============================================================
//
// Main LED state machine.
//
// Called periodically by heartbeatTask.
//
// Parameters:
// -------------------------------------------------------------
// wifiOk:
//   true if WiFi is currently connected
//
// mqttOk:
//   true if MQTT is currently connected
//
// State priority:
// -------------------------------------------------------------
// 1. LED_SENT
//      short publish-success flash
//
// 2. LED_WIFI
//      WiFi disconnected
//
// 3. LED_MQTT
//      MQTT disconnected
//
// 4. LED_OK
//      WiFi and MQTT connected
//
// 5. LED_OFF
//      blink off phase
//
// Blinking:
// -------------------------------------------------------------
// _blink toggles on every update() call.
//
// This creates a simple heartbeat effect without timers,
// interrupts or additional state machines.
//
// Optimization:
// -------------------------------------------------------------
// If calculated state is the same as _lastState,
// no physical LED write is performed.
//
// This reduces:
// - unnecessary NeoPixel writes
// - timing-sensitive LED operations
// - visual flicker
//
// =============================================================

void LedStatus::update(bool wifiOk, bool mqttOk)
{
    _blink = !_blink;

    LedState state = LED_OFF;

    if (millis() < _sentBlinkUntilMs)
    {
        state = LED_SENT;
    }
    else if (!wifiOk)
    {
        state = _blink ? LED_WIFI : LED_OFF;
    }
    else if (!mqttOk)
    {
        state = _blink ? LED_MQTT : LED_OFF;
    }
    else
    {
        state = _blink ? LED_OK : LED_OFF;
    }

    if (state == _lastState)
        return;

    if (_mutex && xSemaphoreTake(_mutex, pdMS_TO_TICKS(100)))
    {
        setRaw(state);
        xSemaphoreGive(_mutex);
        _lastState = state;
    }
}

// =============================================================
// SET RAW LED STATE
// =============================================================
//
// Low-level NeoPixel writer.
//
// This is the ONLY function that physically writes color values
// to the RGB LED.
//
// It is private because the rest of the firmware should work
// with logical states, not direct colors.
//
// Color mapping:
// -------------------------------------------------------------
// LED_OK:
//   system is operational
//
// LED_WIFI:
//   WiFi is disconnected
//
// LED_MQTT:
//   MQTT is disconnected
//
// LED_SENT:
//   short indication of successful MQTT publish
//
// LED_ERROR:
//   reserved for fatal / error states
//
// LED_OFF:
//   LED disabled
//
// Note about color order:
// -------------------------------------------------------------
// The actual visible color depends on the NeoPixel configuration
// in LedStatus.h:
//
//   NEO_GRB + NEO_KHZ800
//
// Therefore the numerical RGB order may not appear intuitive.
// Keep color mapping centralized here.
//
// =============================================================

void LedStatus::setRaw(LedState state)
{
    switch (state)
    {
        case LED_OK:
            _rgb.setPixelColor(0, _rgb.Color(50, 0, 0));
            break;

        case LED_WIFI:
            _rgb.setPixelColor(0, _rgb.Color(0, 0, 50));
            break;

        case LED_MQTT:
            _rgb.setPixelColor(0, _rgb.Color(35, 50, 0));
            break;

        case LED_SENT:
            _rgb.setPixelColor(0, _rgb.Color(50, 0, 50));
            break;

        case LED_ERROR:
            _rgb.setPixelColor(0, _rgb.Color(0, 50, 0));
            break;

        default:
            _rgb.setPixelColor(0, 0);
            break;
    }

    _rgb.show();
}