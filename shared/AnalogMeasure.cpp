#include "AnalogMeasure.h"

// =============================================================
// ANALOG MEASURE
// =============================================================
//
// Lightweight helper for stable analog measurements on ESP32.
//
// Features:
// -------------------------------------------------------------
// - configurable ADC resolution
// - multiple samples per reading
// - min/max rejection
// - EMA low-pass filtering
// - voltage conversion
// - optional voltage divider compensation
//
// Typical use:
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

static const uint8_t  MIN_SAMPLES = 3;
static const uint8_t  MAX_SAMPLES = 50;
static const uint16_t SAMPLE_DELAY_US = 300;

static const float MIN_EMA_ALPHA = 0.01f;
static const float MAX_EMA_ALPHA = 1.00f;

// =============================================================
// CONSTRUCTOR
// =============================================================
//
// pin:
//   ADC input pin
//
// vRef:
//   reference voltage used for conversion
//
// resolutionBits:
//   ADC resolution, usually 12 bits on ESP32
//
// =============================================================

AnalogMeasure::AnalogMeasure(
    int pin,
    float vRef,
    int resolutionBits
)
    :
    _pin(pin),
    _vRef(vRef),
    _resolutionBits(resolutionBits)
{
    _adcMax = (1 << _resolutionBits) - 1;
}

// =============================================================
// BEGIN
// =============================================================
//
// Initializes ADC input.
//
// For ESP32:
// - resolution is configured globally
// - ADC_11db attenuation allows wider input voltage range
//
// Important:
// ESP32 ADC input must never exceed 3.3V.
//
// =============================================================

void AnalogMeasure::begin()
{
    analogReadResolution(_resolutionBits);

#ifdef ESP32
    analogSetPinAttenuation(_pin, ADC_11db);
#endif

    pinMode(_pin, INPUT);
}

// =============================================================
// SET SAMPLE COUNT
// =============================================================
//
// Number of ADC samples used for one measurement.
//
// The driver removes the minimum and maximum sample,
// therefore at least 3 samples are required.
//
// Higher values:
// - more stable
// - slower
//
// Lower values:
// - faster
// - noisier
//
// =============================================================

void AnalogMeasure::setSamples(uint8_t samples)
{
    if (samples < MIN_SAMPLES)
        samples = MIN_SAMPLES;

    if (samples > MAX_SAMPLES)
        samples = MAX_SAMPLES;

    _samples = samples;
}

// =============================================================
// SET EMA ALPHA
// =============================================================
//
// Exponential moving average factor.
//
// alpha close to 1.0:
// - fast response
// - less smoothing
//
// alpha close to 0.0:
// - slow response
// - more smoothing
//
// Recommended:
// - 0.1 to 0.3 for gas sensors
//
// =============================================================

void AnalogMeasure::setEmaAlpha(float alpha)
{
    if (alpha < MIN_EMA_ALPHA)
        alpha = MIN_EMA_ALPHA;

    if (alpha > MAX_EMA_ALPHA)
        alpha = MAX_EMA_ALPHA;

    _emaAlpha = alpha;
}

// =============================================================
// SET VOLTAGE DIVIDER
// =============================================================
//
// Used when sensor output voltage is higher than ESP32 ADC range.
//
// rTop:
//   resistor between sensor output and ADC pin
//
// rBottom:
//   resistor between ADC pin and GND
//
// Example:
//
//   sensor output ---- rTop ---- ADC ---- rBottom ---- GND
//
// Compensation:
//
//   measuredVoltage = adcVoltage * ((rTop + rBottom) / rBottom)
//
// =============================================================

void AnalogMeasure::setVoltageDivider(float rTop, float rBottom)
{
    if (rTop <= 0.0f || rBottom <= 0.0f)
        return;

    _dividerRatio = (rTop + rBottom) / rBottom;
}

// =============================================================
// READ
// =============================================================
//
// Performs one complete analog measurement.
//
// Steps:
// -------------------------------------------------------------
// 1. read multiple ADC samples
// 2. remove one minimum and one maximum value
// 3. average remaining values
// 4. update EMA filter
// 5. convert raw ADC value to voltage
//
// Returns:
// -------------------------------------------------------------
//
// valid:
//   true if raw ADC value is within expected range
//
// raw:
//   averaged ADC value after min/max rejection
//
// voltage:
//   current voltage value
//
// filteredRaw:
//   EMA-filtered raw ADC value
//
// filteredVoltage:
//   EMA-filtered voltage value
//
// =============================================================

AnalogMeasure::Reading AnalogMeasure::read()
{
    Reading r;

    r.valid = false;
    r.raw = 0;
    r.voltage = NAN;
    r.filteredRaw = NAN;
    r.filteredVoltage = NAN;

    int raw = readAveragedRaw();

    if (raw < 0 || raw > _adcMax)
        return r;

    if (!_hasFilter)
    {
        _filteredRaw = raw;
        _hasFilter = true;
    }
    else
    {
        _filteredRaw =
            (_emaAlpha * raw) +
            ((1.0f - _emaAlpha) * _filteredRaw);
    }

    float voltage = rawToVoltage((float)raw);
    float filteredVoltage = rawToVoltage(_filteredRaw);

    r.valid = true;
    r.raw = raw;
    r.voltage = voltage;
    r.filteredRaw = _filteredRaw;
    r.filteredVoltage = filteredVoltage;

    return r;
}

// =============================================================
// READ AVERAGED RAW
// =============================================================
//
// Reads multiple ADC samples and returns averaged value.
//
// Simple noise rejection:
// - remove lowest sample
// - remove highest sample
// - average the rest
//
// This is cheap and effective for noisy analog gas sensors.
//
// =============================================================

int AnalogMeasure::readAveragedRaw()
{
    int minVal = INT_MAX;
    int maxVal = INT_MIN;
    long sum = 0;

    for (uint8_t i = 0; i < _samples; i++)
    {
        int value = analogRead(_pin);

        if (value < minVal)
            minVal = value;

        if (value > maxVal)
            maxVal = value;

        sum += value;

        delayMicroseconds(SAMPLE_DELAY_US);
    }

    sum -= minVal;
    sum -= maxVal;

    return sum / (_samples - 2);
}

// =============================================================
// RAW TO VOLTAGE
// =============================================================
//
// Converts raw ADC value to input voltage.
//
// Divider compensation is applied here.
//
// =============================================================

float AnalogMeasure::rawToVoltage(float raw) const
{
    if (_adcMax <= 0)
        return NAN;

    return (raw / (float)_adcMax) * _vRef * _dividerRatio;
}