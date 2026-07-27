#include "SdLogger.h"

// =============================================================
// GLOBAL SD LOGGER INSTANCE
// =============================================================
//
// Single shared SD logger used by the firmware.
//
// Responsibilities:
// -------------------------------------------------------------
// - initialize SD card
// - write measurement CSV
// - write event log
// - provide simple recovery helper
//
// All measurement writes should be performed from sdTask.
//
// =============================================================

SdLogger SdLog;

// =============================================================
// BEGIN
// =============================================================
//
// Initializes SPI bus and SD card.
//
// SD card wiring is defined in AppConfig.h:
//
//   SD_CS_PIN
//   SD_MOSI_PIN
//   SD_SCK_PIN
//   SD_MISO_PIN
//   SD_SPI_SPEED
//
// Design note:
// -------------------------------------------------------------
// SD_SPI_SPEED is intentionally conservative.
//
// 4 MHz is usually fast enough for CSV logging and more reliable
// than aggressive high-speed SPI wiring on breadboards or long wires.
//
// Return value:
// -------------------------------------------------------------
// true  -> SD card initialized successfully
// false -> SD card unavailable
//
// =============================================================

bool SdLogger::begin()
{
    SPI.begin(
        SD_SCK_PIN,
        SD_MISO_PIN,
        SD_MOSI_PIN,
        SD_CS_PIN
    );

    if (!SD.begin(SD_CS_PIN, SPI, SD_SPI_SPEED))
    {
        _available = false;
        Log.printf("* SD Reader failed");
        return false;
    }

    _available = true;
    Log.printf("* SD Reader OK");

    return true;
}

// =============================================================
// AVAILABLE
// =============================================================
//
// Returns current SD availability state.
//
// This is a software flag, not a full hardware re-test.
//
// If a write fails:
// - _available may be set false by caller
// - recoverIfNeeded() can attempt reinitialization
//
// =============================================================

bool SdLogger::available() const
{
    return _available;
}

// =============================================================
// RECOVER IF NEEDED
// =============================================================
//
// Attempts SD recovery only if logger is currently unavailable.
//
// This is intentionally simple:
//
// 1. try begin()
// 2. ensure CSV header exists
// 3. log recovery event
//
// This is called from sdTask after a write failure.
//
// =============================================================

void SdLogger::recoverIfNeeded()
{
    if (_available)
        return;

    if (begin())
    {
        ensureDataHeader();
        logEvent("sd recovery ok");
    }
}

// =============================================================
// APPEND LINE
// =============================================================
//
// Appends one text line to a file.
//
// Used by:
// - event log
//
// Current implementation opens and closes the file every time.
//
// Why open/write/close each time:
// -------------------------------------------------------------
// This is slower than keeping file open,
// but much safer for unattended embedded logging.
//
// Benefits:
// - lower risk of filesystem corruption after reset
// - data are flushed on every write
// - watchdog reset is less likely to lose buffered content
//
// =============================================================

bool SdLogger::appendLine(const char* path, const char* line)
{
    if (!_available)
        return false;

    if (!path || !line)
        return false;

    File file = SD.open(path, FILE_APPEND);

    if (!file)
    {
        Log.printf("SD open failed: %s", path);
        _available = false;
        return false;
    }

    file.println(line);
    file.close();

    return true;
}

// =============================================================
// LOG EVENT
// =============================================================
//
// Appends one event record to events.log.
//
// Format:
//
//   millis,iso_time,event_text
//
// Example:
//
//   12345,2026-05-26 14:00:00,wifi recovery
//
// Used for:
// - boot events
// - watchdog events
// - WiFi recovery
// - MQTT recovery
// - SD recovery
//
// =============================================================

bool SdLogger::logEvent(const char* eventText)
{
    if (!_available)
        return false;

    if (!eventText)
        return false;

    char timeBuffer[32];
    Clock.isoTime(timeBuffer, sizeof(timeBuffer));

    char line[192];

    int written = snprintf(
        line,
        sizeof(line),
        "%lu,%s,%s",
        millis(),
        timeBuffer,
        eventText
    );

    if (written < 0 || written >= (int)sizeof(line))
        return false;

    return appendLine(_eventFile, line);
}

// =============================================================
// ENSURE DATA CSV HEADER
// =============================================================
//
// Creates CSV header if data file does not exist yet.
//
// File:
//
//   /data.csv
//
// Header format:
//
//   millis,iso_time,sequence,f1,f2,...,f24
//
// This makes SD logs directly importable into:
// - Excel
// - LibreOffice
// - Python / pandas
// - database tools
//
// =============================================================

void SdLogger::ensureDataHeader()
{
    if (!_available)
        return;

    if (SD.exists(_dataFile))
        return;

    File file = SD.open(_dataFile, FILE_WRITE);

    if (!file)
    {
        Log.printf("SD header open failed");
        _available = false;
        return;
    }

    file.print("millis,iso_time,sequence");

    for (int i = 0; i < FIELD_COUNT; i++)
    {
        file.printf(",f%d", i + 1);
    }

    file.println();
    file.close();
}

// =============================================================
// LOG PACKET CSV
// =============================================================
//
// Writes one SensorPacket as a CSV row.
//
// File:
//
//   /data.csv
//
// Row format:
//
//   millis,iso_time,sequence,f1,f2,...,fN
//
// Invalid values:
// -------------------------------------------------------------
// Invalid or missing values are written as empty CSV fields.
//
// Example:
//
//   12345,2026-05-26 14:00:00,42,12.3,,1010.5
//
// Empty fields are intentional.
// They preserve field positions while clearly indicating missing data.
//
// Performance / Safety:
// -------------------------------------------------------------
// The file is opened and closed for every packet.
//
// This is intentional for robustness.
//
// If power is lost or watchdog reset occurs:
// - most recently closed rows remain valid
// - FAT corruption risk is lower
//
// =============================================================

bool SdLogger::logPacketCsv(const SensorPacket &packet)
{
    if (!_available)
        return false;

    File file = SD.open(_dataFile, FILE_APPEND);

    if (!file)
    {
        Log.printf("SD data open failed");
        _available = false;
        return false;
    }

    char timeBuffer[32];
    Clock.isoTime(timeBuffer, sizeof(timeBuffer));

    file.printf(
        "%lu,%s,%lu",
        packet.timestampMs,
        timeBuffer,
        packet.sequence
    );

    for (int i = 0; i < FIELD_COUNT; i++)
    {
        file.print(",");

        if (packet.valid[i] && isGoodNumber(packet.f[i]))
        {
            file.print(packet.f[i], 3);
        }
    }

    file.println();
    file.close();

    return true;
}