# Subsurface Multi-gas Respiration \& Anomaly Detector

![smrad](/images/smrad.png)

( AI translation just to give an overview to non CZ based visitors )

SMRAD is an autonomous multi-channel environmental and gas logger based on the ESP32-S3 and FreeRTOS. The firmware is designed for long-term unattended operation.

### Main goals:

+ long-term stability
+ resilience against Wi-Fi and MQTT outages
+ resilience against invalid sensor readings
+ separation of sensor and network logic
+ watchdog recovery
+ local SD logging
+ simple system extensibility

### Design decisions — the firmware deliberately prioritizes:

+ readability over micro-optimization
+ simple debugging
+ separation of subsystems
+ easy maintenance

Reason: The system is intended for long-term operation and future expansion. Predictable system behavior is more important than maximum performance.

## HW configuration

![smrad](/images/Overview.png)
![smrad](/images/RS485cz.png)

The system consists of two stations connected via RS485:

- **Probe** – lower measurement unit. Reads physical sensors, builds a `SensorPacket`, converts it to `SmradPacket` and sends it over RS485.
- **Logger** – upper unit. Receives and validates the RS485 packet, applies calibrations, adds local service data, stores measurements on the SD card, and publishes them via MQTT/ThingSpeak.
- **Monitor** – simple diagnostic firmware for checking RS485 communication without the full Logger logic.
- **shared** – shared data structures and implementations used by multiple firmware projects.

The Logger connects to the network via **Wi-Fi**. In a particular installation, Internet connectivity may be provided by an external 5G modem/router; the firmware itself does not contain a cellular/5G modem driver.
The Probe normally measures and sends every `10 s`. The Logger continuously services Wi-Fi and MQTT, but the publishing cycle is normally `20 s`; the newest available packet is always taken from the single-item `packetQueue`.

## HW configuration

### Logger pins

- `LED_PIN 38`
- `SD_CS_PIN 10`
- `SD_MOSI_PIN 11`
- `SD_SCK_PIN 12`
- `SD_MISO_PIN 13`
- `SDA_PIN 16`
- `SCL_PIN 17`
- `RS485_RX_PIN 15`
- `RS485_TX_PIN 18`
- `POWER_VIN_PIN A2`
- `POWER_BAT_PIN A3`

### Probe pins

- `SDA_PIN 8`
- `SCL_PIN 9`
- `LED_PIN 38`
- `CO2_RX 16`
- `CO2_TX 17`
- `MQ4_PIN A0`
- `H2S_PIN A1`
- `POWER_VIN_PIN A2`
- RS485: RX `4`, TX `5`, `115200 baud`

The Probe RS485 pins are currently defined directly in `Probe.ino`; the other main SW/HW constants are in `Probe/AppConfig.h`.

## Runtime

### Logger LED
The LED is updated every 500 ms.

| State | Color |
| --- | --- |
| last MQTT publish OK | 🟢 green |
| everything OK | ⚪ white |
| Wi-Fi error | 🔵 blue |
| MQTT error | 🟣 magenta |
| SD error | 🟡 yellow |
| general error | 🔴 red |

### Probe LED

The Probe normally flashes white; after sending an RS485 packet, it signals activity in green. At startup, a sensor self-test is performed: the number of white flashes identifies the sensor, followed by green/red to indicate its status.

| Number of flashes | Sensor |
| ---: | --- |
| 1 | BME280 |
| 2 | CO2 |
| 3 | FS3000 |
| 4 | H2S |
| 5 | MQ4 |
| 6 | O2 |

## Firmware

The firmware uses a FreeRTOS task architecture.
Data flow: sensorTask → Queue → networkTask → MQTT / ThingSpeak
At the same time: sensorTask → sdQueue → sdTask → SD card
Each task has its own heartbeat monitored by the watchdog layer.

### FreeRTOS tasks

+ sensorTask Reads sensors, validates data, and creates a SensorPacket.
+ networkTask Handles Wi-Fi, MQTT reconnection, and publishing.
+ heartbeatTask Controls the RGB LED.
+ watchdogTask Monitors task heartbeats and performs recovery.
+ timeTask Synchronizes time via NTP at startup and every hour.
+ sdTask handles CSV logging and event logging to the SD card.

The watchdog layer uses multi-level recovery logic.

Levels:
1. reconnect Wi-Fi
2. reconnect MQTT
3. restart ESP32

Each task periodically records a heartbeat timestamp. watchdogTask checks:
+ task timeouts
+ a prolonged Wi-Fi outage
+ a prolonged MQTT outage
+ a prolonged absence of a successful publish
+ absence of valid CO2 data

## Project overview

S.M.R.A.D. consists of three separate firmware projects:

- **Probe** – lower measurement unit. Reads physical sensors, builds a data packet and sends it over RS485.
- **Logger** – upper unit. Receives data from the Probe, adds its own data, applies calibration, stores data on the SD card, and sends it via MQTT.
- **Monitor** – simple service firmware intended primarily for RS485 communication diagnostics.

Code shared by multiple firmware projects is stored in the **shared** folder.

## Recommended code-reading order

For a new developer, the quickest approach is:

1. `Probe/Probe.ino` – main Probe flow.
2. `Probe/SensorManager.cpp` – sensor reading and mapping to fields 0–23.
3. `shared/SensorPacket.h` – shared data structure and named field indices.
4. `shared/SmradPacket.h` – binary RS485 protocol format.
5. `shared/Rs485PacketSender.cpp` – sending from the Probe.
6. `Logger/Logger.ino` – hlavní FreeRTOS tasky Loggeru.
7. `shared/Rs485PacketReceiver.cpp` – reception, synchronization, header validation, and CRC.
8. `Logger/ConfigManager.cpp` – runtime configuration and calibration from the SD card.
9. `Logger/MqttPublisher.cpp` – MQTT/ThingSpeak publishing.
10. `Logger/SdLogger.cpp` – `data.csv` a `events.log`.
11. `Logger/WatchdogManager.cpp` – monitoring of tasks and subsystems.

## Directory structure

### `Probe/`

- `Probe.ino` – initialization, main measurement loop, RS485 transmission, and watchdog.
- `SensorManager.cpp/.h` – sensor initialization and reading, value validation, mapping into `SensorPacket`.
- `ExplorIR_CO2.cpp/.h` – communication with the ExplorIR CO2 sensor.
- `ProbeStatus.h` – availability of individual sensors.
- `AppConfig.h` – piny a compile-time settings Probe.

### `Logger/`

- `Logger.ino` – main application and FreeRTOS tasks.
- `BaseSensorManager.cpp/.h` – local Logger service measurements and RS485 heartbeat.
- `ConfigManager.cpp/.h` – loading `config.json` a `calibration.json`.
- `Calibration.cpp/.h` – mathematical calibration functions.
- `WifiManager.cpp/.h` – Wi-Fi state machine a recovery.
- `MqttPublisher.cpp/.h` – MQTT state machine a publishing do ThingSpeak.
- `SdLogger.cpp/.h` – CSV/event logging a recovery SD karty.
- `TimeSync.cpp/.h` – time synchronization via NTP.
- `LedStatus.cpp/.h` – RGB status LED.
- `WatchdogManager.cpp/.h` – task, Wi-Fi, MQTT, publish a RS485 dohled.
- `AppConfig.h` – piny a compile-time default Logger settings.
- `mqtt_secrets.h` – default MQTT credentials; see the security note below.

### `shared/`

- `SensorPacket.h` – 24 hodnot, jejich validita a společné názvy indexů.
- `SmradPacket.h` – RS485 packet format a valid mask.
- `Rs485PacketSender.*` – odesílání RS485.
- `Rs485PacketReceiver.*` – příjem a kontrola paketů.
- `Crc16.h` – CRC16.
- `AnalogMeasure.*` – ADC měření, průměrování a EMA.
- `DebugLog.*` – společný debug/monitor logging.

Soubory stejného jména v `Logger/`, `Probe/` a `Monitor/` mohou být pouze **forwardery** na implementaci v `shared/`. Při změně společné logiky nejprve ověřte, zda neupravujete pouze forwarder.

### `SD/`

Příklad provozního obsahu SD karty:

- `config.json` – runtime konfigurace Loggeru,
- `calibration.json` – kalibrační pravidla,
- `data.csv` – ukázková data,
- `events.log` – ukázkový systémový log.

## Data flow

The basic path of one measurement is:

          physical sensors
               ↓
        Probe/SensorManager
               ↓
          SensorPacket
               ↓
           SmradPacket
               ↓
             RS485
              :::
             RS485
               ↓
     Logger/Rs485PacketReceiver
               ↓
          SensorPacket
               ↓
           calibration
               ↓
     local Logger data
               ↓
       ┌───────┴────────┐
       ↓                ↓
    SD card          MQTT
    data.csv        ThingSpeak

The Probe is the only part of the system that directly reads the main measurement sensors. The Logger primarily works with the already received packet and adds data that is local to the Logger.

## Application constants in AppConfig.h

enabling/disabling debug information on the serial port / value monitor
+ SERIAL_DEBUG 1
+ SERIAL_MONITOR 1

default Wi-Fi configuration
+ WIFI_SSID       "STRONG_FA12_2.4GHz"
+ WIFI_PASS       "6S7THuRG77"

default MQTT configuration
+ MQTT\_SERVER     "mqtt3.thingspeak.com"
+ MQTT\_PORT       1883
+ CHANNEL\_ID\_1    "3112132"
+ CHANNEL\_ID\_2    "3431221"
+ CHANNEL\_ID\_3    "3431223"

measured-channel definitions
+ FIELD\_COUNT         24
+ FIELDS\_PER\_CHANNEL  8
+ MAX\_CHANNELS        3

sensor reading interval
+ SENSOR\_PERIOD\_MS    20000UL

MQTT data publishing interval
+ MQTT\_SPACING\_MS     1200UL

Retry intervals
+ WIFI\_RETRY\_MS       15000UL
+ MQTT\_RETRY\_MS       8000UL

NTP intervals
+ TIME\_SYNC\_INTERVAL\_MS   3600000UL
+ TIME\_SYNC\_TIMEOUT\_MS    10000UL

Watchdog timing
+ WATCHDOG\_PERIOD\_MS             5000UL
+ DIAGNOSTIC\_LOG\_PERIOD\_MS       60000UL
+ SENSOR\_TASK\_TIMEOUT\_MS         90000UL
+ NETWORK\_TASK\_TIMEOUT\_MS        45000UL
+ LED\_TASK\_TIMEOUT\_MS            15000UL
+ TIME\_TASK\_TIMEOUT\_MS           60000UL
+ SD\_TASK\_TIMEOUT\_MS             60000UL

Wi-Fi recovery policy
+ WIFI\_OFFLINE\_RECOVERY\_MS       180000UL
+ vWIFI\_OFFLINE\_RESTART\_MS        900000UL

MQTT recovery policy
+ MQTT\_OFFLINE\_RECOVERY\_MS       180000UL
+ MQTT\_OFFLINE\_RESTART\_MS        1200000UL

Restart timeouts
+ NO\_PUBLISH\_RESTART\_MS          1800000UL
+ RS485\_NO\_VALID\_WARN\_MS          600000UL

### Time handling

The Logger has two time sources: the DS3231 RTC module and external synchronization via NTP services.
The system works as follows: shortly after boot, the time is read from the RTC module (if it was previously written),
and when NTP servers are reachable, the Logger obtains the exact time from them and synchronizes the RTC module.
By the second Logger startup at the latest, accurate time is available immediately when the system starts.

The firmware uses NTP synchronization through:
+ pool.ntp.org
+ time.google.com
+ time.cloudflare.com

## SD configuration

The configuration on the SD card overrides the default values
in AppConfig.json. If the card is missing, the values compiled into AppConfig.h are used.

    config.json
    {
      "device": {
      "name": "SMRAD-03 STRONG",
      "location": "Dan MQTT / modem",
      "hardwareRevision": 1
    },
    "wifi": {
      "ssid": "STRONG\_FA12\_2.4GHz",
      "password": "6S7THuRG77"
    },
    "mqtt": {
      "server": "mqtt3.thingspeak.com",
      "port": 1883,
      "clientId": "Jx0uMyYRDCokEC8UGyEdBRQ",
      "username": "Jx0uMyYRDCokEC8UGyEdBRQ",
      "password": "vV98a7nxij3Ucm1fsxS8j2Ni",
      "channel1": "3112132",
      "channel2": "3431221",
      "channel3": "3431223"
    },
    "timing": {
      "sensorPeriodMs": 20000,
      "mqttSpacingMs": 1200,
      "timeSyncIntervalMs": 3600000
    }

Events are written to events.log
      20886,2026-07-19 17:49:59,sd recovery ok
      6002,NO\_TIME,boot
      5918,NO\_TIME,boot
      6013,NO\_TIME,boot
      5936,NO\_TIME,boot

Data are written to data.csv
      millis,iso\_time,sequence,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,f16,f17,f18,f19,f20,f21,f22,f23,f24
      10538,NO\_TIME,295,20.000,29.388,28.150,979.895,281.477,39.500,1800.000,0.180,1.316,988.000,1.194,1089.046,0.015,0.000,0.000,0.000,0.383,303.000,0.366,317.119,1.887,1.198,0.000,0.000
      20639,2026-05-27 14:10:26,296,20.000,29.388,28.140,979.890,281.522,39.441,1700.000,0.170,1.322,1113.000,1.345,1093.836,0.015,0.000,0.000,0.000,0.379,298.000,0.360,313.295,1.886,1.165,0.000,0.000
      30740,2026-05-27

## Measured values

Measured values are sent to MQTT. By default, there are 3 channels with 8 float values each.
Values are written to the SD card and sent to MQTT.

The key function is void SensorManager::read(SensorPacket \&packet)
in SensorManager.cpp. This function controls reading data from the sensors
and writing it to the appropriate channel fields

The code typically looks like this:

    if (_status.gas)
    {
      float gasPpm = _gas.readGasConcentrationPPM();
      float gasTemp = _gas.readTempC();
      setField(packet, 0, gasPpm,  isGoodNumber(gasPpm));
      setField(packet, 1, gasTemp, isGoodNumber(gasTemp));
    }
    if (_status.bme)
    {
      float temp         = _bme.readTemperature();
      float přessureHpa  = _bme.readPressure() / 100.0f;
      float altitude     = _bme.readAltitude(SEALEVELPRESSURE\\\_HPA);
      float humidity     = _bme.readHumidity();

      setField(packet, 2, temp,        isGoodNumber(temp));
      setField(packet, 3, přessureHpa, isGoodNumber(přessureHpa));
      setField(packet, 4, altitude,    isGoodNumber(altitude));
      setField(packet, 5, humidity,    isGoodNumber(humidity));
    }

First, it is determined whether the sensor provided valid data, and then
the data is written to the individual channel positions. There are 3 channels with 8 fields each,
for a total of up to 24 values (0–23).

Both Probe and Logger use `setField()` with last-known-good stabilization. If a current reading temporarily fails and a previous valid value exists, the last known good value may be reused.

The current implementation **does not impose an age limit on last-good values**. This behavior is intentionally retained; it must be taken into account when interpreting the data. If it becomes necessary in the future to distinguish a long-term failure of a specific sensor, a timestamp and maximum cache age would be suitable extensions.

Value order

#### CHANNEL 1

| No.	| Parametr      | Source             | Unit   | Min  | Max    | Notes                                                |   
| --- | --------------| ----------------- | ---------- | ---- | ------ | ---------------------------------------------------- | 
| 0	  | Temperature	  | BME280			      |  °C		     | -40	| 85	   | bme.readTemperature                                  | 
| 1	  | Pressure	    | BME280			      |  kPa		   |  30	| 110	   | bme.readPressure / 1000                              | 
| 2	  | Rel. humidity	| BME280			      |  %		     |   0	| 100	   | bme.readHumidity                                     | 
| 3	  | Airflow vel.	| FS3000-1015		    | m/s		     |   0	| 15     | raw airflow (později kal. rovnice 1)                 | 
| 4	  | Oxygen		    | DFRobot SEN0465		| % vol		   |   0	| 21	   | o2.readGasConcentrationPPM / 10000                   | 
| 5	  | CO2		        | ExplorIR-M-E-100	| % vol		   |   0	| 100	   | co2Reading.percent                                   | 
| 6	  | Methane		    | MQ-4			        | % vol		   |   0	| 4095   | MQ4 raw\_filtered (calibration na % vol dodatečně)     | 
| 7	  | H2S		        | DFRobot SEN0568		| ppm vol	   |   0	| 4095   | h2s raw\_filtered (calibration na ppm vol. dodatečně)  | 

#### CHANNEL 2

Channel 2 is intentionally left empty by the Probe. It is assumed that the Logger will have
calibration data available and that corrected values will be written to the same positions in channel 2.

#### CHANNEL 3

Channel 3 is considered a service channel; except for attribute 17, all fields are filled by the Logger.

| No.	| Parametr      | Source             | Unit   | Min  | Max    | Notes                                                |   
| --- | --------------| ----------------- | ---------- | ---- | ------ | ---------------------------------------------------- | 
| 16	| Timestamp	  | RTC	    		      |  min		   | 0   	| 	     | float (24 bit) minut od zacatku UNIX epochy          | 
| 17  | Supply voltage | Probe A2		      |  raw		   | 0	  | 4096	 | napeti zdroje probe  BAT                             | 
| 18  | Supply voltage | Logger A2		      |  raw		   | 0	  | 4096	 | napeti zdroje logger AC                              | 
| 19  | Supply voltage | Logger A3		      |  raw		   | 0	  | 4096	 | napeti zdroje logger BAT                             | 
| 20	| Temperature	  | RTC   			      |  °C		     | -40	| 85	   | RTC.readTemperature                                  | 

## How to add a new sensor

When adding a new sensor, distinguish between the physical sensor and the telemetry field.

### 1. Sensor initialization

Initialization of the main measurement sensors belongs in `Probe/SensorManager`.

State inicializace uložte do `ProbeStatus`, aby bylo možné rozlišit mezi:

- a sensor that is not present,
- a sensor that could not be initialized,
- a sensor that is available but whose current measurement is invalid.

### 2. Reading a value

Write the measured value in `SensorManager::read()` using:

    setField(packet, index, value, valid);

Each quantity must have a uniquely assigned index from 0–23.

Before using a new index, check the “Measured values” table in this README.

### 3. Value validity

A value must be marked as valid only if the sensor provided a usable measurement.

The mere existence of a numeric value is not the same as a valid measurement.

### 4. Transmission over RS485

In the Probe, `SensorPacket` is converted into `SmradPacket`. `SmradPacket` contains the values and a bit mask indicating their validity.

The packet is protected by CRC16.

When changing the RS485 protocol format, consider increasing `SMRAD_PACKET_VERSION`, because Probe, Logger, and Monitor must use a compatible packet structure.

### 5. Logger

Fields 0–7 are primarily data from the Probe.

Fields 8–15 are intended for calibrated values.

Fields 16–23 are system service and local values.

Before changing this allocation, check:

- `SensorManager.cpp`
- `BaseSensorManager.cpp`
- `Calibration.cpp`
- `MqttPublisher.cpp`
- `SdLogger.cpp`
- the measured-values table in the README

### 6. MQTT / ThingSpeak

Every eight items in `SensorPacket` form one ThingSpeak channel:

    0–7   → channel1 / field1–field8
    8–15  → channel2 / field1–field8
    16–23 → channel3 / field1–field8

Changing a quantity's index can therefore change not only the RS485 packet, but also the target MQTT channel and ThingSpeak field.

Po každé změně mapování aktualizujte the measured-values table in the README.

![smrad](/images/Testing.png)

## Developer quick start

1. Upload the Probe firmware from Probe/Probe.ino.
2. Upload the Logger firmware from Logger/Logger.ino.
3. For RS485 diagnostics, Monitor/Monitor.ino can be used instead of the Logger.
4. Prepare an SD card with /config.json and /calibration.json.
5. Configure the Wi-Fi and MQTT credentials.
6. Open the Serial Monitor at 115200 baud.
7. After startup, verify the Probe self-test and then verify RS485 packet reception on the Logger.

## Required Arduino libraries

ArduinoJson
PubSubClient
Adafruit BME280
Adafruit Unified Sensor
Adafruit NeoPixel
SparkFun FS3000
Rtc by Makuna / RtcDS3231
WiFi
SD/SPI

## Code organization and structure

The code is organized into the following folders:

+ Logger - upper station
+ Monitor - test firmware for monitoring data on the serial line
+ Probe - lower station
+ Shared - shared code

The applications are written in C++ for ESP32-S3 RTOS. They are divided into tasks, and
each task is monitored by the watchdog. If a task does not report within the defined time,
the entire application is restarted.

## Logger/Probe environment settings

    board ESP32S3 Dev module
    Upload 921600
    USB Mode. Hardware CDC and JTAG
    USB CDC on boot: Enabled
    USB Firmware MSC on boot: Disabled
    USB DFU on boot: Disabled
    CPU frequency 240 MHz
    Flash mode QIO 80 MHz
    Partition scheme Default 4MB with spifs
    PSRAM: OPI PSRAM
    Flash Size 4M (32M)

## Calibration

Calibration is stored in the calibration.json file and may look like this:

    {
      "calibration":\[
      { "in":0, "out": 8, "type":1, "a":1 },
      { "in":1, "out": 9, "type":1, "a":1 },
      { "in":2, "out":10, "type":1, "a":1 },
      { "in":3, "out":11, "type":1, "a":1 },
      { "in":4, "out":12, "type":1, "a":1 },
      { "in":5, "out":13, "type":1, "a":1 },
      { "in":6, "out":14, "type":1, "a":1 },
      { "in":7, "out":15, "type":1, "a":1 }
      ]
    }

Calibration converts a measured sensor value (x) into the resulting physical value (y) using a selected mathematical function.

Each channel may have one or more calibrations assigned to it, defined by the CalibrationConfig structure.
Calibration ale jde dal CIn je vstupni kanal cOut je vystupni, takze priklad calibration je vlastne prekopirovani
the results 1->8, 2->9, etc.

### Struktura calibration

| Item	| Description         |   
| ------- | --------------|
|  cIn	  | Input channel identifier. | 
|  cOut	  | Output channel identifier.| 
|  cType	| Type of calibration function used.  | 
|  lowerLimit	|  Lower limit platnosti calibration. | 
|  upperLimit	| Upper limit platnosti calibration.  | 
|  a–g	| Parameters kalibrační rovnice. Meaning závisí na typu calibration. | 

#### Range limits
A valid range of input values can be defined before the calculation.
lowerLimit <= x <= upperLimit
If lowerLimit and upperLimit differ and the input value lies outside this interval, the function returns 0.0
Pokud jsou obě hodnoty stejné (lowerLimit == upperLimit), kontrola rozsahu se neprovádí a calibration je platná pro všechny vstupy.

### Typy calibration

#### 1\. Polynomial (CAL\_POLYNOMIAL)

Uses a polynomial of up to the sixth order.
y = g * a·x * b·x² * c·x³ * d·x⁴ * e·x⁵ * f·x⁶

Parameters

| Parametr | Meaning       |
| -------- | ------------ |
| a	| coefficient of x  | 
| b	| coefficient of x² | 
| c	| coefficient of x³ | 
| d	| coefficient of x⁴ | 
| e	| coefficient of x⁵ | 
| f	| coefficient of x⁶ | 
| g	| absolute offset (constant)| 

This type is suitable, for example, for converting sensor voltage into gas concentration or another nonlinear quantity.

#### 2\. Exponential (CAL\_EXPONENTIAL)

Uses an exponential function.
y = a · e^(b·x) + c·x + d

##### Parameters

| Parametr | Meaning       |
| -------- | ------------ |
| a	| amplitude of the exponential part | 
| b	| exponent            | 
| c	| linear correction    |    
| d	| offset              | 

This model is suitable, for example, for sensors with an exponential response.

#### 3\. Power (CAL\_POWER)

Uses a power function.
y = a · (b + x)^c + d

##### Parameters

| Parametr | Meaning       |
| -------- | ------------ |
| a	| multiplication coefficient | 
| b	| input offset        | 
| c	| exponent            | 
| d	| offset              | 

Constraint: b + x > 0 must hold

#### 4\. Logarithmic (CAL\_LOG)

Uses the natural logarithm.
y = a · ln(b·x + c) + d

##### Parameters

| Parametr | Meaning       |
| -------- | ------------ |
| a | multiplication coefficient |
| b	| input multiplier     |
| c	| logarithm-argument offset |
| d	  offset |

Constraint: b·x + c > 0 must hold 

#### Neznámý typ calibration

Pokud cType obsahuje neznámou hodnotu, calibration se neprovede a funkce vrátí původní vstupní hodnotu: y = x

## RS232 debug output

### Logger debug output
```
----------------------------------------------------------------------
Subsurface Multi-gas Respiration and Anomaly Detector 1.00 Base-logger
----------------------------------------------------------------------
RTC time is: 2026-09-08 09:48:25
RS485 receiver started RX=15 TX=18 baud=115200 packetSize=116 crcOffset=114
Boot #1
Reset reason: POWERON (1)
* SD Reader OK
CFG Config loaded from: /config.json
CFG Calibration loaded: 8 valid of 8 record(s)
Device: SMRAD-PZ develop
Location: PZ MQTT / TamNET
HW rev: 2
WiFi SSID: TamNET_Guest
MQTT server: mqtt3.thingspeak.com:1883
Channels: 3389737 / 3394344 / 3394347
Timing: sensor=20000 mqttSpacing=1200 timeSync=3600000
Cal[0]: In=0 Out=8 Type=1
Cal[1]: In=1 Out=9 Type=1
Cal[2]: In=2 Out=10 Type=1
Cal[3]: In=3 Out=11 Type=1
Cal[4]: In=4 Out=12 Type=1
Cal[5]: In=5 Out=13 Type=1
Cal[6]: In=6 Out=14 Type=1
Cal[7]: In=7 Out=15 Type=1
SNS Sensors init...
* Power telemetry OK
SNS Base sensors init done
WD Watchdog task started
SD task started
WiFi connecting...
MQTT connecting...
MQTT OK
rs485 packet ok seq=188 fields=24 mask=0x000200C7
CAL Calibration found: in=0 -> out=8 type=1 27.190 -> 27.190
CAL Calibration found: in=1 -> out=9 type=1 97.501 -> 97.501
CAL Calibration found: in=2 -> out=10 type=1 38.852 -> 38.852
CAL Calibration found: in=3 -> out=11 type=1 nan -> nan
CAL Calibration found: in=4 -> out=12 type=1 nan -> nan
CAL Calibration found: in=5 -> out=13 type=1 nan -> nan
CAL Calibration found: in=6 -> out=14 type=1 258.905 -> 258.905
CAL Calibration found: in=7 -> out=15 type=1 208.035 -> 208.035
27.190001;97.500656;38.851562;nan;nan;nan;258.905334;208.035416;27.190001;97.500656;38.851562;nan;nan;nan;258.905334;208.035416;nan;242.699295;nan;nan;nan;nan;nan;nan
SNS Packet #188 add local values
RTC temp = 26.00
SNS 26.00 -> 20 
NTP sync OK: 1788860915 2026-09-08 09:48:35
SYS time: 2026-09-08 09:48:35
RTC time set to: 2026-09-08 09:48:35
rs485 packet ok seq=189 fields=24 mask=0x000200C7
CAL Calibration found: in=0 -> out=8 type=1 27.260 -> 27.260
CAL Calibration found: in=1 -> out=9 type=1 97.498 -> 97.498
CAL Calibration found: in=2 -> out=10 type=1 39.421 -> 39.421
CAL Calibration found: in=3 -> out=11 type=1 nan -> nan
CAL Calibration found: in=4 -> out=12 type=1 nan -> nan
CAL Calibration found: in=5 -> out=13 type=1 nan -> nan
CAL Calibration found: in=6 -> out=14 type=1 262.924 -> 262.924
CAL Calibration found: in=7 -> out=15 type=1 203.228 -> 203.228
27.260000;97.498047;39.420898;nan;nan;nan;262.924255;203.228333;27.260000;97.498047;39.420898;nan;nan;nan;262.924255;203.228333;nan;242.594406;nan;nan;nan;nan;nan;nan
SNS Packet #189 add local values
RTC temp = 26.00
SNS 26.00 -> 20 
MQTT Tx topic 1: field1=27.260&field2=97.498&field3=39.421&field4=&field5=&field6=&field7=262.924&field8=203.228
MQTT Tx topic 2: field1=27.260&field2=97.498&field3=39.421&field4=&field5=&field6=&field7=262.924&field8=203.228
MQTT Tx topic 3: field1=29814348.000&field2=242.594&field3=2.488&field4=2.461&field5=26.000&field6=&field7=&field8=
```

### Logger monitor output

Pro zjednoduseni calibration je mozne prekompilovat Logger firmware v nastaveni

#### AppConfig.h:
```
#define SERIAL_DEBUG 0
#define SERIAL_MONITOR 1
```

The RS232 output (USB-C cable) will then contain only the measured data

```
27.879999;97.399109;37.876953;nan;nan;nan;238.301010;194.063965;27.879999;97.399109;37.876953;nan;nan;nan;238.301010;194.063965;nan;222.627319;nan;nan;nan;nan;nan;nan
27.940001;97.396515;36.976562;nan;nan;nan;235.640808;198.251175;27.940001;97.396515;36.976562;nan;nan;nan;235.640808;198.251175;nan;221.933228;nan;nan;nan;nan;nan;nan
27.959999;97.396950;36.849609;nan;nan;nan;235.312653;201.400940;27.959999;97.396950;36.849609;nan;nan;nan;235.312653;201.400940;nan;221.193253;nan;nan;nan;nan;nan;nan
27.930000;97.397820;36.789062;nan;nan;nan;235.650116;203.920761;27.930000;97.397820;36.789062;nan;nan;nan;235.650116;203.920761;nan;219.964264;nan;nan;nan;nan;nan;nan
27.920000;97.398560;36.612305;nan;nan;nan;234.520096;206.936615;27.920000;97.398560;36.612305;nan;nan;nan;234.520096;206.936615;nan;218.919632;nan;nan;nan;nan;nan;nan
27.920000;97.397873;36.541016;nan;nan;nan;232.416077;209.149307;27.920000;97.397873;36.541016;nan;nan;nan;232.416077;209.149307;nan;219.231689;nan;nan;nan;nan;nan;nan
27.959999;97.400169;36.438477;nan;nan;nan;232.732864;209.319443;27.959999;97.400169;36.438477;nan;nan;nan;232.732864;209.319443;nan;221.146942;nan;nan;nan;nan;nan;nan
```

### Probe debug output
```
------------------------------------------------------------------
Subsurface Multi-gas Respiration and Anomaly Detector 1.00 - PROBE
------------------------------------------------------------------
Sensors init...
* BME280 OK
* O2 FAIL
* ExplorIR CO2 FAIL
* FS3000 FAIL
* Power telemetry OK
* Analog MQ4/H2S OK
Sensors init done
==== SENSOR SELF-TEST =====
BME280:   OK
O2:       FAIL
CO2:      FAIL
FS3000:   FAIL
MQ4:      OK
H2S:      OK
POWER:    OK
============================
RS485 sender started RX=4 TX=5 baud=115200 packetSize=116 crcOffset=114
Probe ready ...
Probe WDT task added
Packet #1 read
rs485 tx packet seq=1 uptime=32822 fields=24 mask=0x000200C7 crc=0x5675
rs485 tx f01 valid=1 values 27.080 97.492 37.985 nan nan nan 281.000 215.000
rs485 tx f09 valid=0 values nan nan nan nan nan nan nan nan
rs485 tx f17 valid=0 values nan 351.000 nan nan nan nan nan nan
rs485 tx ok seq=1 crc=0x5675
RS485 sent packet #1
Packet #2 read
rs485 tx packet seq=2 uptime=43061 fields=24 mask=0x000200C7 crc=0xE7B8
rs485 tx f01 valid=1 values 27.060 97.491 38.009 nan nan nan 281.800 211.000
rs485 tx f09 valid=0 values nan nan nan nan nan nan nan nan
rs485 tx f17 valid=0 values nan 334.650 nan nan nan nan nan nan
rs485 tx ok seq=2 crc=0xE7B8
RS485 sent packet #2
```
