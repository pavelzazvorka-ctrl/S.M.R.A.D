#pragma once

#include <Arduino.h>

// =============================================================
// ANALOG MEASURE
// =============================================================
//
// Lightweight helper for stable analog measurements.
//
// Features:
// -------------------------------------------------------------
// - configurable ADC resolution
// - configurable sample averaging
// - min/max rejection
// - EMA low-pass filtering
// - voltage conversion
// - voltage divider compensation
//
// Designed primarily for:
// - gas sensors
// - analog environmental sensors
// - noisy ADC inputs
//
// Design goals:
// -------------------------------------------------------------
// - simple API
// - deterministic behavior
// - readable implementation
// - low runtime overhead
//
// Example:
// -------------------------------------------------------------
//
// AnalogMeasure mq4(A0);
//
// mq4.begin();
// mq4.setSamples(21);
// mq4.setEmaAlpha(0.2f);
// mq4.setVoltageDivider(10000, 20000);
//
// auto r = mq4.read();
//
// if (r.valid)
// {
//     Serial.println(r.filteredVoltage);
// }
//
// =============================================================

class AnalogMeasure
{
public:

    // =========================================================
    // MEASUREMENT RESULT
    // =========================================================
    //
    // valid:
    //   true if measurement succeeded
    //
    // raw:
    //   averaged ADC value after min/max rejection
    //
    // voltage:
    //   converted voltage value
    //
    // filteredRaw:
    //   EMA filtered ADC value
    //
    // filteredVoltage:
    //   EMA filtered voltage value
    //
    // =========================================================

    struct Reading
    {
        bool valid;

        int raw;
        float voltage;

        float filteredRaw;
        float filteredVoltage;
    };

    // =========================================================
    // CONSTRUCTOR
    // =========================================================
    //
    // pin:
    //   ADC input pin
    //
    // vRef:
    //   ADC reference voltage
    //
    // resolutionBits:
    //   ADC resolution in bits
    //
    // Typical ESP32:
    //   12 bits
    //
    // =========================================================

    AnalogMeasure(
        int pin,
        float vRef = 3.3f,
        int resolutionBits = 12
    );

    // =========================================================
    // INITIALIZATION
    // =========================================================
    //
    // Configures ADC input and attenuation.
    //
    // Must be called before read().
    //
    // =========================================================

    void begin();

    // =========================================================
    // SAMPLE COUNT
    // =========================================================
    //
    // Number of ADC samples used for averaging.
    //
    // Higher:
    // - more stable
    // - slower
    //
    // Lower:
    // - faster
    // - noisier
    //
    // =========================================================

    void setSamples(uint8_t samples);

    // =========================================================
    // EMA FILTER STRENGTH
    // =========================================================
    //
    // Exponential moving average coefficient.
    //
    // Typical values:
    //
    // 0.1 -> heavy smoothing
    // 0.5 -> moderate smoothing
    // 1.0 -> no smoothing
    //
    // =========================================================

    void setEmaAlpha(float alpha);

    // =========================================================
    // VOLTAGE DIVIDER CONFIGURATION
    // =========================================================
    //
    // Used when sensor voltage exceeds ADC input range.
    //
    // Example:
    //
    // sensor --- rTop --- ADC --- rBottom --- GND
    //
    // =========================================================

    void setVoltageDivider(float rTop, float rBottom);

    // =========================================================
    // READ MEASUREMENT
    // =========================================================
    //
    // Performs:
    //
    // - ADC sampling
    // - averaging
    // - min/max rejection
    // - EMA filtering
    // - voltage conversion
    //
    // =========================================================

    Reading read();

private:

    // ADC input pin
    int _pin;

    // ADC reference voltage
    float _vRef;

    // ADC resolution in bits
    int _resolutionBits;

    // Maximum ADC value
    int _adcMax;

    // Number of ADC samples per measurement
    uint8_t _samples = 15;

    // EMA filter coefficient
    float _emaAlpha = 0.25f;

    // Indicates whether EMA filter already contains valid state
    bool _hasFilter = false;

    // Internal EMA filtered raw ADC value
    float _filteredRaw = 0;

    // Voltage divider compensation ratio
    float _dividerRatio = 1.0f;

    // =========================================================
    // INTERNAL HELPERS
    // =========================================================

    // Read averaged ADC value
    int readAveragedRaw();

    // Convert ADC value to voltage
    float rawToVoltage(float raw) const;
};