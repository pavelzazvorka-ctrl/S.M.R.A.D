#include <Arduino.h>

// =============================================================
// EXPLORIR CO2 SENSOR DRIVER
// =============================================================
//
// Lightweight UART driver for ExplorIR-M CO2 sensors.
//
// Features:
// -------------------------------------------------------------
// - internal retry handling
// - UART timeout protection
// - scale factor handling
// - simple Reading API
// - diagnostic raw response storage
//
// Design goals:
// -------------------------------------------------------------
// - simple integration
// - deterministic behavior
// - readable implementation
// - no dynamic allocation outside String internals
//
// Example:
// -------------------------------------------------------------
//
// HardwareSerial co2Uart(2);
//
// ExplorIR_CO2 co2(
//     co2Uart,
//     16,     // RX
//     17      // TX
// );
//
// if (co2.begin())
// {
//     auto r = co2.read();
//
//     if (r.valid)
//     {
//         Serial.println(r.ppm);
//     }
// }
//
// =============================================================

class ExplorIR_CO2
{
public:

    // =========================================================
    // SENSOR READING
    // =========================================================
    //
    // valid:
    //   true if parsing and validation succeeded
    //
    // ppm:
    //   CO2 concentration in parts per million
    //
    // percent:
    //   CO2 concentration in %
    //
    // raw:
    //   raw UART response from sensor
    //   useful for diagnostics
    //
    // =========================================================

    struct Reading
    {
        bool valid;
        float ppm;
        float percent;
        String raw;
    };

    // =========================================================
    // CONSTRUCTOR
    // =========================================================
    //
    // serial:
    //   UART interface used for communication
    //
    // rxPin / txPin:
    //   ESP32 UART pin mapping
    //
    // baud:
    //   sensor UART speed
    //
    // =========================================================

    ExplorIR_CO2(
        HardwareSerial& serial,
        int rxPin,
        int txPin,
        uint32_t baud = 9600
    );

    // =========================================================
    // INITIALIZATION
    // =========================================================
    //
    // Initializes UART and reads sensor scale factor.
    //
    // Returns:
    //   true  -> sensor initialized successfully
    //   false -> sensor unavailable or invalid response
    //
    // =========================================================

    bool begin();

    // =========================================================
    // READ SENSOR
    // =========================================================
    //
    // Reads CO2 value from sensor.
    //
    // Internal retry logic is handled automatically.
    //
    // Returns:
    //   Reading structure
    //
    // =========================================================

    Reading read();

    // =========================================================
    // SCALE FACTOR
    // =========================================================
    //
    // Returns internal sensor scale factor.
    //
    // Final ppm value:
    //
    //   ppm = rawValue * scaleFactor
    //
    // =========================================================

    int scaleFactor() const;

private:

    // UART interface
    HardwareSerial& _serial;

    // UART pin mapping
    int _rx;
    int _tx;

    // UART baud rate
    uint32_t _baud;

    // Sensor scaling factor
    int _scaleFactor = 1;

    // =========================================================
    // INTERNAL HELPERS
    // =========================================================

    // Clear pending UART input
    void flush();

    // Send command and receive one response line
    String command(const char* cmd, uint32_t timeoutMs);

    // Parse scale factor response
    int parseScaleFactor(const String &response);

    // Parse CO2 response
    int parseCo2RawValue(const String &response);
};