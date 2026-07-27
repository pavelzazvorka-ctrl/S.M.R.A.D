#include <Arduino.h>
#include "ExplorIR_CO2.h"

// =============================================================
// EXPLORIR CO2 SENSOR DRIVER
// =============================================================
//
// Driver for ExplorIR-M CO2 sensor family.
//
// Communication:
// - UART
// - command/response protocol
// - CR/LF terminated commands
//
// Public API:
// - begin()
// - read()
// - scaleFactor()
//
// Design goals:
// - keep the driver small
// - hide retry logic inside the class
// - return a simple Reading structure
// - never block forever
// - tolerate temporary invalid UART responses
//
// =============================================================

static const uint8_t  BEGIN_RETRIES       = 5;
static const uint8_t  READ_RETRIES        = 5;

static const uint32_t SENSOR_BOOT_DELAY_MS = 1500;
static const uint32_t BEGIN_TIMEOUT_MS     = 500;
static const uint32_t READ_TIMEOUT_MS      = 500;

static const uint32_t BEGIN_RETRY_DELAY_MS = 100;
static const uint32_t READ_RETRY_DELAY_MS  = 80;

static const size_t RESPONSE_RESERVE_SIZE  = 32;

// =============================================================
// CONSTRUCTOR
// =============================================================
//
// The class does not own the HardwareSerial object.
// It receives a reference to an existing UART instance.
//
// Example:
//
//   HardwareSerial co2Uart(2);
//   ExplorIR_CO2 co2(co2Uart, 16, 17);
//
// =============================================================

ExplorIR_CO2::ExplorIR_CO2(
    HardwareSerial& serial,
    int rxPin,
    int txPin,
    uint32_t baud)
    :
    _serial(serial),
    _rx(rxPin),
    _tx(txPin),
    _baud(baud)
{
}

// =============================================================
// BEGIN
// =============================================================
//
// Initializes UART and queries sensor scale factor.
//
// The ExplorIR sensor reports CO2 as a raw value.
// The final ppm value is:
//
//   ppm = rawValue * scaleFactor
//
// begin() returns false if:
// - sensor does not respond
// - response format is invalid
// - scale factor is zero or invalid
//
// Retry is done internally so caller does not need to implement it.
//
// =============================================================

bool ExplorIR_CO2::begin()
{
    _serial.begin(_baud, SERIAL_8N1, _rx, _tx);

    // Sensor needs time after power-up before it replies reliably.
    delay(SENSOR_BOOT_DELAY_MS);

    flush();

    for (uint8_t attempt = 0; attempt < BEGIN_RETRIES; attempt++)
    {
        String response = command(".\r\n", BEGIN_TIMEOUT_MS);
        response.trim();

        int scale = parseScaleFactor(response);

        if (scale > 0)
        {
            _scaleFactor = scale;
            return true;
        }

        delay(BEGIN_RETRY_DELAY_MS);
    }

    return false;
}

// =============================================================
// READ
// =============================================================
//
// Reads CO2 concentration.
//
// The method performs several retries internally.
// Caller receives one Reading structure:
//
//   valid   = true/false
//   ppm     = CO2 concentration in ppm
//   percent = CO2 concentration in percent
//   raw     = last raw response from sensor
//
// If all retries fail:
// - valid is false
// - ppm and percent are NAN
// - raw contains the last received response
//
// =============================================================

ExplorIR_CO2::Reading ExplorIR_CO2::read()
{
    Reading out;

    out.valid = false;
    out.ppm = NAN;
    out.percent = NAN;
    out.raw = "";

    for (uint8_t attempt = 0; attempt < READ_RETRIES; attempt++)
    {
        String response = command("Z\r\n", READ_TIMEOUT_MS);
        response.trim();

        out.raw = response;

        int rawValue = parseCo2RawValue(response);

        if (rawValue >= 0 && _scaleFactor > 0)
        {
            out.ppm = rawValue * _scaleFactor;
            out.percent = out.ppm / 10000.0f;
            out.valid = true;
            return out;
        }

        delay(READ_RETRY_DELAY_MS);
    }

    return out;
}

// =============================================================
// SCALE FACTOR
// =============================================================

int ExplorIR_CO2::scaleFactor() const
{
    return _scaleFactor;
}

// =============================================================
// FLUSH
// =============================================================
//
// Clears pending UART bytes before sending a new command.
//
// This prevents old or partial responses from being interpreted
// as a response to the current command.
//
// =============================================================

void ExplorIR_CO2::flush()
{
    while (_serial.available())
    {
        _serial.read();
    }
}

// =============================================================
// COMMAND
// =============================================================
//
// Sends command and waits for one LF-terminated response line.
//
// Important:
// - never waits forever
// - strips CR characters
// - returns partial response on timeout
//
// Returning partial response is useful for diagnostics.
//
// =============================================================

String ExplorIR_CO2::command(const char* cmd, uint32_t timeoutMs)
{
    flush();

    _serial.print(cmd);
    _serial.flush();

    String response;
    response.reserve(RESPONSE_RESERVE_SIZE);

    uint32_t start = millis();

    while (millis() - start < timeoutMs)
    {
        while (_serial.available())
        {
            char c = _serial.read();

            if (c == '\r')
                continue;

            response += c;

            if (c == '\n')
                return response;
        }

        delay(2);
    }

    return response;
}

// =============================================================
// PARSE SCALE FACTOR
// =============================================================
//
// Expected response begins with:
//
//   .
//
// Example depends on firmware version, but original implementation
// expected scale factor at substring(2).
//
// This parser keeps compatibility with that behavior but isolates
// the parsing in one place.
//
// =============================================================

int ExplorIR_CO2::parseScaleFactor(const String &response)
{
    if (!response.startsWith("."))
        return -1;

    if (response.length() < 3)
        return -1;

    int scale = response.substring(2).toInt();

    if (scale <= 0)
        return -1;

    return scale;
}

// =============================================================
// PARSE CO2 RAW VALUE
// =============================================================
//
// Expected response begins with:
//
//   Z
//
// Original implementation expected raw value at substring(2).
//
// Returns:
// - raw value >= 0 on success
// - -1 on invalid response
//
// =============================================================

int ExplorIR_CO2::parseCo2RawValue(const String &response)
{
    if (!response.startsWith("Z"))
        return -1;

    if (response.length() < 3)
        return -1;

    int rawValue = response.substring(2).toInt();

    if (rawValue < 0)
        return -1;

    return rawValue;
}