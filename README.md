# Subsurface Multi-gas Respiration \& Anomaly Detector

![smrad](/images/smrad.png)

SMRAD je autonomní vícekanálový environmentální a plynový logger založený na ESP32-S3 a FreeRTOS. Firmware je navržený pro dlouhodobý bezobslužný provoz.

### Hlavní cíle:

+ dlouhodobá stabilita
+ odolnost proti výpadkům WiFi a MQTT
+ odolnost proti nevalidním senzorům
+ oddělení senzorové a síťové logiky
+ watchdog recovery
+ lokální SD logging
+ jednoduché rozšiřování systému

### Návrhová rozhodnutí Firmware záměrně preferuje:

+ čitelnost před mikrooptimalizací
+ jednoduchý debugging
+ oddělení subsystémů
+ jednoduchou údržbu

Důvod: Systém je určený pro dlouhodobý provoz a budoucí rozšiřování. Předvídatelné chování systému je důležitější než maximální výkon.

## HW konfigurace

![smrad](/images/Overview.png)
![smrad](/images/RS485cz.png)

Systém tvoří dvě stanice propojené RS485:

- **Probe** – spodní měřicí jednotka. Čte fyzické senzory, sestaví `SensorPacket`, převede jej na `SmradPacket` a odešle po RS485.
- **Logger** – horní jednotka. Přijme a ověří RS485 paket, aplikuje kalibrace, doplní lokální servisní údaje, uloží měření na SD kartu a publikuje je přes MQTT/ThingSpeak.
- **Monitor** – jednoduchý diagnostický firmware pro kontrolu RS485 komunikace bez celé logiky Loggeru.
- **shared** – společné datové struktury a implementace používané více firmware projekty.

Logger komunikuje do sítě přes **Wi-Fi**. V konkrétní instalaci může internetové připojení poskytovat externí 5G modem/router; firmware samotný neobsahuje cellular/5G modem driver.
Probe standardně měří a odesílá po `10 s`. Logger obsluhuje Wi-Fi a MQTT průběžně, ale publikační cyklus je standardně `20 s`; z jednoprkové `packetQueue` se vždy vezme nejnovější dostupný paket.

## HW konfigurace

### Logger piny

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

### Probe piny

- `SDA_PIN 8`
- `SCL_PIN 9`
- `LED_PIN 38`
- `CO2_RX 16`
- `CO2_TX 17`
- `MQ4_PIN A0`
- `H2S_PIN A1`
- `POWER_VIN_PIN A2`
- RS485: RX `4`, TX `5`, `115200 baud`

RS485 piny Probe jsou zatím definovány přímo v `Probe.ino`; ostatní hlavní SW/HW konstanty jsou v `Probe/AppConfig.h`.

## Runtime

### Logger LED
LED se aktualizuje každých 500 ms.

| Stav | Barva |
| --- | --- |
| poslední MQTT publish OK | 🟢 zelená |
| vše OK | ⚪ bílá |
| Wi-Fi error | 🔵 modrá |
| MQTT error | 🟣 magenta |
| SD error | 🟡 žlutá |
| obecná chyba | 🔴 červená |

### Probe LED

Probe standardně bliká bíle; po odeslání RS485 paketu signalizuje aktivitu zeleně. Při startu proběhne self-test senzorů: počet bílých bliknutí určuje senzor, následná zelená/červená jeho stav.

| Počet bliknutí | Senzor |
| ---: | --- |
| 1 | BME280 |
| 2 | CO2 |
| 3 | FS3000 |
| 4 | H2S |
| 5 | MQ4 |
| 6 | O2 |

## Firmware

Firmware používá FreeRTOS task architekturu.
Datový tok: sensorTask → Queue → networkTask → MQTT / ThingSpeak
Současně: sensorTask → sdQueue → sdTask → SD karta
Každý task má vlastní heartbeat monitorovaný watchdog vrstvou.

### FreeRTOS tasky

+ sensorTask Čte senzory, validuje data a vytváří SensorPacket.
+ networkTask Zajišťuje WiFi, MQTT reconnect a publish.
+ heartbeatTask Ovládá RGB LED.
+ watchdogTask Monitoruje heartbeat tasků a provádí recovery.
+ timeTask Synchronizace času přes NTP při startu a každou hodinu.
+ sdTask zajišťuje CSV logging a event logging na SD kartu.

Watchdog vrstva používá víceúrovňovou recovery logiku.

Úrovně:
1. reconnect WiFi
2. reconnect MQTT
3. restart ESP32

Každý task pravidelně zapisuje heartbeat timestamp. watchdogTask kontroluje:
+ timeout tasků
+ dlouhý výpadek WiFi
+ dlouhý výpadek MQTT
+ dlouhou absenci successful publish
+ absenci validních CO2 dat

## Jak se v projektu zorientovat

S.M.R.A.D. se skládá ze tří samostatných firmware projektů:

- **Probe** – spodní měřicí jednotka. Čte fyzické senzory, sestaví datový paket a odešle jej po RS485.
- **Logger** – horní jednotka. Přijímá data z Probe, doplňuje vlastní údaje, aplikuje kalibrace, ukládá data na SD kartu a odesílá je přes MQTT.
- **Monitor** – jednoduchý servisní firmware určený především pro diagnostiku RS485 komunikace.

Část kódu společná pro více firmware je uložena ve složce **shared**.

## Doporučené pořadí pro čtení kódu

Pro nového vývojáře je nejrychlejší postup:

1. `Probe/Probe.ino` – hlavní tok Probe.
2. `Probe/SensorManager.cpp` – čtení senzorů a mapování do polí 0–23.
3. `shared/SensorPacket.h` – společná datová struktura a pojmenované indexy polí.
4. `shared/SmradPacket.h` – binární formát RS485 protokolu.
5. `shared/Rs485PacketSender.cpp` – odesílání z Probe.
6. `Logger/Logger.ino` – hlavní FreeRTOS tasky Loggeru.
7. `shared/Rs485PacketReceiver.cpp` – příjem, synchronizace, validace hlavičky a CRC.
8. `Logger/ConfigManager.cpp` – runtime konfigurace a kalibrace z SD.
9. `Logger/MqttPublisher.cpp` – MQTT/ThingSpeak publikování.
10. `Logger/SdLogger.cpp` – `data.csv` a `events.log`.
11. `Logger/WatchdogManager.cpp` – dohled nad tasky a subsystémy.

## Struktura adresářů

### `Probe/`

- `Probe.ino` – inicializace, hlavní měřicí smyčka, RS485 odesílání a watchdog.
- `SensorManager.cpp/.h` – inicializace a čtení senzorů, validace hodnot, mapování do `SensorPacket`.
- `ExplorIR_CO2.cpp/.h` – komunikace s ExplorIR CO2 senzorem.
- `ProbeStatus.h` – dostupnost jednotlivých senzorů.
- `AppConfig.h` – piny a compile-time nastavení Probe.

### `Logger/`

- `Logger.ino` – hlavní aplikace a FreeRTOS tasky.
- `BaseSensorManager.cpp/.h` – lokální servisní měření Loggeru a RS485 heartbeat.
- `ConfigManager.cpp/.h` – načítání `config.json` a `calibration.json`.
- `Calibration.cpp/.h` – matematické kalibrační funkce.
- `WifiManager.cpp/.h` – Wi-Fi state machine a recovery.
- `MqttPublisher.cpp/.h` – MQTT state machine a publikování do ThingSpeak.
- `SdLogger.cpp/.h` – CSV/event logging a recovery SD karty.
- `TimeSync.cpp/.h` – synchronizace času přes NTP.
- `LedStatus.cpp/.h` – stavová RGB LED.
- `WatchdogManager.cpp/.h` – task, Wi-Fi, MQTT, publish a RS485 dohled.
- `AppConfig.h` – piny a compile-time výchozí nastavení Loggeru.
- `mqtt_secrets.h` – výchozí MQTT credentials; viz bezpečnostní poznámka níže.

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

## Datový tok

Základní cesta jednoho měření je:

          fyzické senzory
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
           kalibrace
               ↓
     lokální data Loggeru
               ↓
       ┌───────┴────────┐
       ↓                ↓
    SD karta          MQTT
    data.csv        ThingSpeak

Probe je jediná část systému, která přímo čte hlavní měřicí senzory. Logger pracuje primárně s již přijatým paketem a doplňuje data, která jsou lokální pro Logger.

## Aplikační konstanty AppConfig.h

zapínání / vypínání debug informace na seriovem portu / monitor hodnot
+ SERIAL_DEBUG 1
+ SERIAL_MONITOR 1

default konfigurace wifi
+ WIFI_SSID       "STRONG_FA12_2.4GHz"
+ WIFI_PASS       "6S7THuRG77"

default konfigurace MQTT
+ MQTT\_SERVER     "mqtt3.thingspeak.com"
+ MQTT\_PORT       1883
+ CHANNEL\_ID\_1    "3112132"
+ CHANNEL\_ID\_2    "3431221"
+ CHANNEL\_ID\_3    "3431223"

definice merenych kanalu
+ FIELD\_COUNT         24
+ FIELDS\_PER\_CHANNEL  8
+ MAX\_CHANNELS        3

četnost cteni senzorů
+ SENSOR\_PERIOD\_MS    20000UL

četnost posilani dat na MQTT
+ MQTT\_SPACING\_MS     1200UL

Retry interval
+ WIFI\_RETRY\_MS       15000UL
+ MQTT\_RETRY\_MS       8000UL

NTP intervaly
+ TIME\_SYNC\_INTERVAL\_MS   3600000UL
+ TIME\_SYNC\_TIMEOUT\_MS    10000UL

Časování watchdog
+ WATCHDOG\_PERIOD\_MS             5000UL
+ DIAGNOSTIC\_LOG\_PERIOD\_MS       60000UL
+ SENSOR\_TASK\_TIMEOUT\_MS         90000UL
+ NETWORK\_TASK\_TIMEOUT\_MS        45000UL
+ LED\_TASK\_TIMEOUT\_MS            15000UL
+ TIME\_TASK\_TIMEOUT\_MS           60000UL
+ SD\_TASK\_TIMEOUT\_MS             60000UL

wifi recovery policy
+ WIFI\_OFFLINE\_RECOVERY\_MS       180000UL
+ vWIFI\_OFFLINE\_RESTART\_MS        900000UL

mqtt recovery policy
+ MQTT\_OFFLINE\_RECOVERY\_MS       180000UL
+ MQTT\_OFFLINE\_RESTART\_MS        1200000UL

restart timeouty
+ NO\_PUBLISH\_RESTART\_MS          1800000UL
+ RS485\_NO\_VALID\_WARN\_MS          600000UL

### Práce s časem

Logger ma dualni zdroj casu - RTC modul DS3231 a externi sync přes NTP sluzby.
Cele to pracuje tak, ze kratce po bootu se nacte čas z RTC modulu ( byl-li predtim zapsan )
a kdyz ma logger v dosahu NTP servery tak z nich vycte přesny cas a tim synchronizuje RTC modul.
Nejpozdeji v druhem startu Loggeru je k dispozici přesny cas uz pri startu systemu.

Firmware používá NTP synchronizaci přes:
+ pool.ntp.org
+ time.google.com
+ time.cloudflare.com

## SD konfigurace

Konfigurace na SD karte nahrazuje defaultni hodnoty
v AppConfig.json. Kdyz karta chybi, plati hodnoty zkompilovane v AppConfig.h

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

Události jsou zapisovány do events.log
      20886,2026-07-19 17:49:59,sd recovery ok
      6002,NO\_TIME,boot
      5918,NO\_TIME,boot
      6013,NO\_TIME,boot
      5936,NO\_TIME,boot

Data jsou zapisována do data.csv
      millis,iso\_time,sequence,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,f16,f17,f18,f19,f20,f21,f22,f23,f24
      10538,NO\_TIME,295,20.000,29.388,28.150,979.895,281.477,39.500,1800.000,0.180,1.316,988.000,1.194,1089.046,0.015,0.000,0.000,0.000,0.383,303.000,0.366,317.119,1.887,1.198,0.000,0.000
      20639,2026-05-27 14:10:26,296,20.000,29.388,28.140,979.890,281.522,39.441,1700.000,0.170,1.322,1113.000,1.345,1093.836,0.015,0.000,0.000,0.000,0.379,298.000,0.360,313.295,1.886,1.165,0.000,0.000
      30740,2026-05-27

## Měřené hodnoty

Měřené hodnoty jsou posilany na MQTT. Standardně jsou k dispozici 3 kanaly po 8 float hodnotach.
Hodnoty jsou zapisovany na kartu a odesilany na MQTT

Klicova je funkce void SensorManager::read(SensorPacket \&packet)
v souboru SensorManager.cpp. Tato funkce ridi cteni dat ze senzorů
a jejich zapis na prislusna pole kanalu

Kód typicky vypada nasledovne ..

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

.. nejdříve se zjisti, jestli sensor poskytl validni data a pak
se data zapisuji na jednotliva mista kanalu .. jsou 3 kanaly po 8.
celkem az 24 (0-23) hodnot

Probe i Logger používají `setField()` s last-known-good stabilizací. Pokud aktuální čtení krátkodobě selže a existuje předchozí dobrá hodnota, může být poslední dobrá hodnota znovu použita.

Současná implementace **nemá časový limit stáří last-good hodnoty**. Je to vědomě ponechané současné chování; při interpretaci dat je potřeba s tím počítat. Pokud bude v budoucnu požadováno rozlišit dlouhodobý výpadek konkrétního senzoru, vhodným rozšířením je timestamp a maximální stáří cache.

Pořadí hodnot

#### CHANNEL 1

| No.	| Parametr      | Zdroj             | Jednotka   | Min  | Max    | Pozn.                                                |   
| --- | --------------| ----------------- | ---------- | ---- | ------ | ---------------------------------------------------- | 
| 0	  | Temperature	  | BME280			      |  °C		     | -40	| 85	   | bme.readTemperature                                  | 
| 1	  | Pressure	    | BME280			      |  kPa		   |  30	| 110	   | bme.readPressure / 1000                              | 
| 2	  | Rel. humidity	| BME280			      |  %		     |   0	| 100	   | bme.readHumidity                                     | 
| 3	  | Airflow vel.	| FS3000-1015		    | m/s		     |   0	| 15     | raw airflow (později kal. rovnice 1)                 | 
| 4	  | Oxygen		    | DFRobot SEN0465		| % vol		   |   0	| 21	   | o2.readGasConcentrationPPM / 10000                   | 
| 5	  | CO2		        | ExplorIR-M-E-100	| % vol		   |   0	| 100	   | co2Reading.percent                                   | 
| 6	  | Methane		    | MQ-4			        | % vol		   |   0	| 4095   | MQ4 raw\_filtered (kalibrace na % vol dodatečně)     | 
| 7	  | H2S		        | DFRobot SEN0568		| ppm vol	   |   0	| 4095   | h2s raw\_filtered (kalibrace na ppm vol. dodatečně)  | 

#### CHANNEL 2

Kanal 2 jde z probe umyslne prazdny. Predpoklada se, ze Logger bude mit k dispozici
kalibracni data a korigivane hodnoty budou zapsany na stejne pozice do kanalu 2.

#### CHANNEL 3

Kanal 3 povazujeme za servisni ..krome attr 17 doplni vse Logger

| No.	| Parametr      | Zdroj             | Jednotka   | Min  | Max    | Pozn.                                                |   
| --- | --------------| ----------------- | ---------- | ---- | ------ | ---------------------------------------------------- | 
| 16	| Čas. značka	  | RTC	    		      |  min		   | 0   	| 	     | float (24 bit) minut od zacatku UNIX epochy          | 
| 17  | Napeti zdroje | Probe A2		      |  raw		   | 0	  | 4096	 | napeti zdroje probe  BAT                             | 
| 18  | Napeti zdroje | Logger A2		      |  raw		   | 0	  | 4096	 | napeti zdroje logger AC                              | 
| 19  | Napeti zdroje | Logger A3		      |  raw		   | 0	  | 4096	 | napeti zdroje logger BAT                             | 
| 20	| Temperature	  | RTC   			      |  °C		     | -40	| 85	   | RTC.readTemperature                                  | 

## Jak přidat nový senzor

Při přidávání nového senzoru je potřeba rozlišovat mezi fyzickým senzorem a telemetrickým polem.

### 1. Inicializace senzoru

Inicializace hlavních měřicích senzorů patří do `Probe/SensorManager`.

Stav inicializace uložte do `ProbeStatus`, aby bylo možné rozlišit mezi:

- senzorem, který není přítomen,
- senzorem, který se nepodařilo inicializovat,
- senzorem, který je dostupný, ale aktuální měření je nevalidní.

### 2. Čtení hodnoty

Naměřenou hodnotu zapisujte v `SensorManager::read()` pomocí:

    setField(packet, index, value, valid);

Každá veličina musí mít jednoznačně přidělený index 0–23.

Před použitím nového indexu zkontrolujte tabulku „Měřené hodnoty“ v tomto README.

### 3. Validita hodnoty

Hodnota musí být označena jako validní pouze tehdy, pokud senzor poskytl použitelné měření.

Samotná existence číselné hodnoty není totéž jako validní měření.

### 4. Přenos přes RS485

`SensorPacket` je v Probe převeden na `SmradPacket`. `SmradPacket` obsahuje hodnoty a bitovou masku jejich validity.

Paket je chráněn CRC16.

Při změně formátu RS485 protokolu je nutné zvážit zvýšení `SMRAD_PACKET_VERSION`, protože Probe, Logger a Monitor musí používat kompatibilní strukturu paketu.

### 5. Logger

Pole 0–7 jsou primárně data z Probe.

Pole 8–15 jsou určena pro kalibrované hodnoty.

Pole 16–23 jsou servisní a lokální hodnoty systému.

Před změnou tohoto rozdělení zkontrolujte:

- `SensorManager.cpp`
- `BaseSensorManager.cpp`
- `Calibration.cpp`
- `MqttPublisher.cpp`
- `SdLogger.cpp`
- tabulku měřených hodnot v README

### 6. MQTT / ThingSpeak

Každých osm položek `SensorPacket` tvoří jeden ThingSpeak kanál:

    0–7   → channel1 / field1–field8
    8–15  → channel2 / field1–field8
    16–23 → channel3 / field1–field8

Změna indexu veličiny tedy může změnit nejen RS485 paket, ale také cílový MQTT kanál a ThingSpeak field.

Po každé změně mapování aktualizujte tabulku měřených hodnot v README.

![smrad](/images/Testing.png)

## Quick start pro vývojáře

1. Nahrajte Probe firmware z Probe/Probe.ino.
2. Nahrajte Logger firmware z Logger/Logger.ino.
3. Pro diagnostiku RS485 lze místo Loggeru použít Monitor/Monitor.ino.
4. Připravte SD kartu s /config.json a /calibration.json.
5. Nastavte WiFi a MQTT credentials.
6. Otevřete Serial Monitor na 115200 baud.
7. Po startu ověřte self-test Probe a následně příjem RS485 paketů na Loggeru.

## Potřebné Arduino knihovny

ArduinoJson
PubSubClient
Adafruit BME280
Adafruit Unified Sensor
Adafruit NeoPixel
SparkFun FS3000
Rtc by Makuna / RtcDS3231
WiFi
SD/SPI

## Organizace kódu a struktura

Kód ma slozky:

+ Logger - horní stanice
+ Monitor - testovaci firmware pro monitorovani dat na seriovou linku
+ Probe - spodni stanice
+ Shared - sdílený kod

Aplikace jsou napsany v CPP na ESP32S3 RTOS. Jsou rozdeleny do Tasku a
kazdy task hlida watchdog. Kdyz se task neprihlasi do definovane doby,
cela aplikace se restartuje.

## Nastavení prostředí Logger/Probe

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

## Kalibrace

Kalibrace je ulozena v calibration.json souboru a vypada napriklad takhle:

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

Kalibrace slouží k převodu naměřené hodnoty ze senzoru (x) na výslednou fyzikální hodnotu (y) pomocí zvolené matematické funkce.

Každý kanál může mít přiřazenou vlastní ( nebo i vice ) kalibraci definovanou strukturou CalibrationConfig.
Kalibrace ale jde dal CIn je vstupni kanal cOut je vystupni, takze priklad kalibrace je vlastne prekopirovani
vysledku 1->8 2->9 atd ...

### Struktura kalibrace

| Položka	| Popis         |   
| ------- | --------------|
|  cIn	  | Identifikátor vstupního kanálu. | 
|  cOut	  | Identifikátor výstupního kanálu.| 
|  cType	| Typ použité kalibrační funkce.  | 
|  lowerLimit	|  Dolní hranice platnosti kalibrace. | 
|  upperLimit	| Horní hranice platnosti kalibrace.  | 
|  a–g	| Parametry kalibrační rovnice. Význam závisí na typu kalibrace. | 

#### Omezení rozsahu
Před samotným výpočtem je možné definovat platný rozsah vstupních hodnot.
lowerLimit <= x <= upperLimit
Pokud jsou lowerLimit a upperLimit různé a vstupní hodnota leží mimo tento interval, funkce vrátí 0.0
Pokud jsou obě hodnoty stejné (lowerLimit == upperLimit), kontrola rozsahu se neprovádí a kalibrace je platná pro všechny vstupy.

### Typy kalibrace

#### 1\. Polynomial (CAL\_POLYNOMIAL)

Používá polynom až šestého řádu.
y = g * a·x * b·x² * c·x³ * d·x⁴ * e·x⁵ * f·x⁶

Parametry

| Parametr | Význam       |
| -------- | ------------ |
| a	| koeficient x  | 
| b	| koeficient x² | 
| c	| koeficient x³ | 
| d	| koeficient x⁴ | 
| e	| koeficient x⁵ | 
| f	| koeficient x⁶ | 
| g	| absolutní posun (konstanta)| 

Tento typ je vhodný například pro převod napětí senzoru na koncentraci plynu nebo jinou nelineární veličinu.

#### 2\. Exponential (CAL\_EXPONENTIAL)

Používá exponenciální funkci.
y = a · e^(b·x) + c·x + d

##### Parametry

| Parametr | Význam       |
| -------- | ------------ |
| a	| amplituda exponenciální části | 
| b	| exponent            | 
| c	| lineární korekce    |    
| d	| offset              | 

Tento model je vhodný například pro senzory s exponenciální charakteristikou.

#### 3\. Power (CAL\_POWER)

Používá mocninnou funkci.
y = a · (b + x)^c + d

##### Parametry

| Parametr | Význam       |
| -------- | ------------ |
| a	| násobící koeficient | 
| b	| posun vstupu        | 
| c	| exponent            | 
| d	| offset              | 

Omezení: Musí platit b + x > 0

#### 4\. Logarithmic (CAL\_LOG)

Používá přirozený logaritmus.
y = a · ln(b·x + c) + d

##### Parametry

| Parametr | Význam       |
| -------- | ------------ |
| a | násobící koeficient |
| b	| násobení vstupu     |
| c	| posun argumentu logaritmu |
| d	  offset |

Omezení: Musí platit b·x + c > 0 

#### Neznámý typ kalibrace

Pokud cType obsahuje neznámou hodnotu, kalibrace se neprovede a funkce vrátí původní vstupní hodnotu: y = x

## RS232 DEBUG output

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

Pro zjednoduseni kalibrace je mozne prekompilovat Logger firmware v nastaveni

#### AppConfig.h:
```
#define SERIAL_DEBUG 0
#define SERIAL_MONITOR 1
```

.. pak budou na vystupu RS232 ( UBS-C kabel ) jen namerena data

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
