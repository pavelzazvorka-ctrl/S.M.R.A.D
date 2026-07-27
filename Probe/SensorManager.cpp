#include "SensorManager.h"

// =============================================================
// GLOBAL SENSOR MANAGER INSTANCE
// =============================================================
//
// Centralized sensor subsystem used by the firmware.
//
// All physical sensor access should go through:
//
//   Sensors
//
// This prevents:
// - concurrent I2C access
// - UART collisions
// - duplicated sensor logic
// - timing inconsistencies
//
// =============================================================

SensorManager Sensors;

bool SensorManager::initPowerTelemetry()
{
    _powerVin.begin();
    _powerVin.setSamples(21);
    _powerVin.setEmaAlpha(0.15f);
    _powerVin.setVoltageDivider(
        POWER_VIN_RTOP,
        POWER_VIN_RBOTTOM
    );

    _powerBat.begin();
    _powerBat.setSamples(21);
    _powerBat.setEmaAlpha(0.15f);
    _powerBat.setVoltageDivider(
        POWER_BAT_RTOP,
        POWER_BAT_RBOTTOM
    );
    Log.printf("* Power telemetry OK");
    return true;
}

void SensorManager::begin()
{
    Log.printf("Sensors init...");

    // Initialize last-known-good cache.
    // Used to stabilize telemetry when temporary invalid readings
    // occur.
    for (uint8_t i = 0; i < FIELD_COUNT; i++)
    {
        _lastGood[i] = NAN;
        _hasLastGood[i] = false;
    }

    // Initialize individual sensor groups.
    _status.bme     = initBme();
    _status.o2      = initGas();
    _status.co2     = initCo2();
    _status.fs3000  = initFS3000();
    _status.power   = initPowerTelemetry();

    initAnalogSensors();
    Log.printf("Sensors init done");
}

// =============================================================
// SENSOR STATUS ACCESS
// =============================================================

ProbeStatus& SensorManager::status()
{
    return _status;
}

// =============================================================
// LAST VALID CO2 TIMESTAMP
// =============================================================

uint32_t SensorManager::lastValidCo2Ms() const
{
    return _lastValidCo2Ms;
}

// =============================================================
// INITIALIZE BME280
// =============================================================

bool SensorManager::initBme()
{
    bool ok = _bme.begin(0x76, &Wire);
    if (!ok)
        ok = _bme.begin(0x77, &Wire);

    Log.printf(ok ? "* BME280 OK" : "* BME280 FAIL");
    return ok;
}

// =============================================================
// INITIALIZE FS3000
// =============================================================

bool SensorManager::initFS3000()
{
    const uint32_t timeoutMs = 5000UL;
    uint32_t startMs = millis();

    while (millis() - startMs < timeoutMs)
    {
        if (_fs.begin())
        {
            _fs.setRange(AIRFLOW_RANGE_15_MPS);
            Log.printf("* FS3000 OK");
            return true;
        }
        delay(250);
    }
    Log.printf("* FS3000 FAIL");
    return false;
}

// =============================================================
// INITIALIZE DFRobot GAS SENSOR
// =============================================================

bool SensorManager::initGas()
{
    const uint32_t timeoutMs = 5000UL;
    uint32_t startMs = millis();

    while (millis() - startMs < timeoutMs)
    {
        if (_o2.begin())
        {
            _o2.changeAcquireMode(_o2.PASSIVITY);
            _o2.setTempCompensation(_o2.OFF);

            Log.printf("* O2 OK");
            return true;
        }

        delay(250);
    }

    Log.printf("* O2 FAIL");

    return false;
}

// =============================================================
// INITIALIZE EXPLORIR CO2
// =============================================================

bool SensorManager::initCo2()
{
    const uint32_t timeoutMs = 7000UL;
    uint32_t startMs = millis();

    while (millis() - startMs < timeoutMs)
    {
        if (_co2.begin())
        {
            Log.printf(
                "* ExplorIR CO2 OK, scale factor: %d",
                _co2.scaleFactor()
            );

            return true;
        }

        delay(500);
    }

    Log.printf("* ExplorIR CO2 FAIL");

    return false;
}

// =============================================================
// INITIALIZE Analog
// =============================================================

void SensorManager::initAnalogSensors()
{
    // MQ4
    _mq4.begin();
    _mq4.setSamples(21);
    _mq4.setEmaAlpha(0.2f);
    _mq4.setVoltageDivider(10000, 20000);
    _status.mq4 = true;

    // Methan
    _h2s.begin();
    _h2s.setSamples(21);
    _h2s.setEmaAlpha(0.2f);
    _h2s.setVoltageDivider(10000, 20000);
    _status.h2s = true;

    Log.printf("* Analog MQ4/H2S OK");
}

// =============================================================
// READ ALL SENSORS
// =============================================================

void SensorManager::read(SensorPacket &packet)
{
    clearPacket(packet);

    // ========================================================================================
    // CHANNEL 1 
    // ========================================================================================
    
    // No.	Parametr	    Zdroj			Jednotka	 Min	Max	   Pozn.
    // ---------------------------------------------------------------------------------------------------------------------
    // 0	Temperature	    BME280			    °C		  -40	85	   bme.readTemperature
    // 1	Pressure	    BME280			    kPa		   30	110	   bme.readPressure / 1000
    // 2	Rel. humidity	BME280			    %		    0	100	   bme.readHumidity
    // 3	Airflow vel.	FS3000-1015		    count		0	4000   raw airflow (později kal. rovnice 1)
    // 4	Oxygen		    DFRobot SEN0465		% vol		0	21	   o2.readGasConcentrationPPM / 10000
    // 5	CO2		        ExplorIR-M-E-100	% vol		0	100	   co2Reading.percent
    // 6	Methane		    MQ-4			    % vol		0	4095   MQ4 raw_filtered (kalibrace na % vol dodatečně)
    // 7	H2S		        DFRobot SEN0568		ppm vol		0	4095   h2s raw_filtered (kalibrace na ppm vol. dodatečně)

    if (_status.bme)
    {
        float temp         = _bme.readTemperature();
        float pressureKpa  = _bme.readPressure() / 1000.0f;
        float humidity     = _bme.readHumidity();

        setField(packet, 0, temp,        isGoodNumber(temp));
        setField(packet, 1, pressureKpa, isGoodNumber(pressureKpa));
        setField(packet, 2, humidity,    isGoodNumber(humidity));
    }

    if (_status.fs3000)
    {
        float airflow = _fs.readMetersPerSecond();
        setField(packet, 3, airflow, isGoodNumber(airflow));
    }

    if (_status.o2)
    {
        float o2Ppm = _o2.readGasConcentrationPPM();
        setField(packet, 4, o2Ppm,  isGoodNumber(o2Ppm));
    }

    if (_status.co2)
    {
        ExplorIR_CO2::Reading co2Reading;   
        co2Reading.valid = false;
        
        co2Reading = _co2.read();
        if (co2Reading.valid)
        {
            _lastValidCo2Ms = millis(); 
            setField(packet, 5, co2Reading.ppm, co2Reading.valid && isGoodNumber(co2Reading.ppm));
        }
    }

    if (_status.mq4)
    {
        AnalogMeasure::Reading mq4Reading;
        mq4Reading.valid = false;
        mq4Reading = _mq4.read();

        if (mq4Reading.valid)
        {
            setField(packet, 6, mq4Reading.filteredRaw,mq4Reading.valid && isGoodNumber(mq4Reading.filteredRaw));
        }
    }

    if (_status.h2s)
    {
        AnalogMeasure::Reading h2sReading;
        h2sReading.valid = false;
        h2sReading = _h2s.read();

        if (h2sReading.valid)
        {
            setField(packet, 6, h2sReading.filteredRaw,h2sReading.valid && isGoodNumber(h2sReading.filteredRaw));
        }
    }
    // ========================================================================================
    // CHANNEL 1 
    // ========================================================================================

    // reserved for calibrated data

    // ========================================================================================
    // CHANNEL 3
    // ========================================================================================
    // No.	Parametr	Zdroj			Jednotka	Min	Max	Pozn.
    // ----------------------------------------------------------------------------------------
    // 16	Čas. značka	ESP (RTC)						Unix time (float)
    // 17	Probe Uin	ESP Ain	V	V		0	4095	Ain
    // 18	U_check		ESP Ain	V	V		0	5	    U na pom. zdroji 3,3V (přítomnost 230V)
    // 21	U bat		ESP Ain	V	V		0	20	    U na baterii (zbytek kapacity), U dělič
    // 22	Teplota		RTC			°C		

    if (_status.power)
    {
        AnalogMeasure::Reading vinReading;
        vinReading.valid = false;
        vinReading = _powerVin.read();

        if (vinReading.valid)
        {
            setField(packet, 16, vinReading.filteredRaw,vinReading.valid && isGoodNumber(vinReading.filteredRaw));
        }
    }
    Log.printf("Packet #%lu read", packet.sequence);
}

// =============================================================
// CLEAR PACKET
// =============================================================

void SensorManager::clearPacket(SensorPacket &packet)
{
    for (uint8_t i = 0; i < FIELD_COUNT; i++)
    {
        packet.f[i] = NAN;
        packet.valid[i] = false;
    }

    packet.sequence = ++_packetSequence;
    packet.timestampMs = millis();
}

// =============================================================
// SET FIELD
// =============================================================

void SensorManager::setField(
    SensorPacket &packet,
    uint8_t index,
    float value,
    bool valid
)
{
    if (index >= FIELD_COUNT)
        return;

    packet.f[index] =
        keepLastGoodValue(index, value, valid);

    packet.valid[index] =
        valid || _hasLastGood[index];
}

// =============================================================
// LAST GOOD VALUE STABILIZATION
// =============================================================

float SensorManager::keepLastGoodValue(
    uint8_t index,
    float value,
    bool valid
)
{
    if (index >= FIELD_COUNT)
        return NAN;

    if (valid && isGoodNumber(value))
    {
        _lastGood[index] = value;
        _hasLastGood[index] = true;

        return value;
    }

    if (_hasLastGood[index])
        return _lastGood[index];

    return NAN;
}

void SensorManager::logSelfTest()
{
    Log.printf("==== SENSOR SELF-TEST =====");
    Log.printf("BME280:   %s", _status.bme ? "OK" : "FAIL");
    Log.printf("O2:       %s", _status.o2 ? "OK" : "FAIL");
    Log.printf("CO2:      %s", _status.co2 ? "OK" : "FAIL");
    Log.printf("FS3000:   %s", _status.fs3000 ? "OK" : "FAIL");
    Log.printf("MQ4:      %s", _status.mq4 ? "OK" : "FAIL");
    Log.printf("H2S:      %s", _status.h2s ? "OK" : "FAIL");
    Log.printf("POWER:    %s", _status.power ? "OK" : "FAIL");
    Log.printf("============================");
}