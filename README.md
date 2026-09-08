# Subsurface Multi-gas Respiration & Anomaly Detector (SMRAD)

![SMRAD](/images/smrad.png)

SMRAD je autonomní vícekanálový environmentální a plynový logger založený na dvojici ESP32-S3. Firmware je navržen pro dlouhodobý bezobslužný provoz, jednoduchou diagnostiku a postupné rozšiřování bez zbytečně složité architektury.

Hlavní cíle projektu jsou dlouhodobá stabilita, odolnost proti výpadkům Wi-Fi/MQTT a krátkodobě nevalidním měřením, oddělení měřicí a síťové logiky, watchdog recovery, lokální SD logging a čitelnost kódu před mikrooptimalizací.

## Jak systém funguje

![Přehled systému](/images/Overview.png)
![RS485 zapojení](/images/RS485cz.png)

Systém tvoří dvě stanice propojené RS485:

- **Probe** – spodní měřicí jednotka. Čte fyzické senzory, sestaví `SensorPacket`, převede jej na `SmradPacket` a odešle po RS485.
- **Logger** – horní jednotka. Přijme a ověří RS485 paket, aplikuje kalibrace, doplní lokální servisní údaje, uloží měření na SD kartu a publikuje je přes MQTT/ThingSpeak.
- **Monitor** – jednoduchý diagnostický firmware pro kontrolu RS485 komunikace bez celé logiky Loggeru.
- **shared** – společné datové struktury a implementace používané více firmware projekty.

Logger komunikuje do sítě přes **Wi-Fi**. V konkrétní instalaci může internetové připojení poskytovat externí 5G modem/router; firmware samotný neobsahuje cellular/5G modem driver.

### Datový tok

```text
Probe:
Senzory -> SensorManager -> SensorPacket -> SmradPacket -> RS485

Logger:
RS485 -> sensorTask -> SensorPacket
                    |-> packetQueue -> networkTask -> MQTT / ThingSpeak
                    `-> sdQueue     -> sdTask      -> SD karta
```

Probe standardně měří a odesílá po `10 s`. Logger obsluhuje Wi-Fi a MQTT průběžně, ale publikační cyklus je standardně `20 s`; z jednoprkové `packetQueue` se vždy vezme nejnovější dostupný paket.

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

RS485 piny Probe jsou zatím definovány přímo v `Probe.ino`; ostatní hlavní HW konstanty jsou v `Probe/AppConfig.h`.

## Význam LED

### Logger

LED se aktualizuje každých 500 ms.

| Stav | Barva |
| --- | --- |
| poslední MQTT publish OK | 🟢 zelená |
| vše OK | ⚪ bílá |
| Wi-Fi error | 🔵 modrá |
| MQTT error | 🟣 magenta |
| SD error | 🟡 žlutá |
| obecná chyba | 🔴 červená |

### Probe

Probe standardně bliká bíle; po odeslání RS485 paketu signalizuje aktivitu zeleně. Při startu proběhne self-test senzorů: počet bílých bliknutí určuje senzor, následná zelená/červená jeho stav.

Probe standardně bliká bíle; po odeslání RS485 paketu signalizuje aktivitu zeleně. Při startu proběhne self-test senzorů: počet bílých bliknutí určuje senzor, následná zelená/červená jeho stav.

| Počet bliknutí | Senzor |
| ---: | --- |
| 1 | BME280 |
| 2 | CO2 |
| 3 | FS3000 |
| 4 | H2S |
| 5 | MQ4 |
| 6 | O2 |

## Jak se v projektu zorientovat

S.M.R.A.D. se skládá ze tří samostatných firmware projektů:

- **Probe** – spodní měřicí jednotka. Čte fyzické senzory, sestaví datový paket a odešle jej po RS485.
- **Logger** – horní jednotka. Přijímá data z Probe, doplňuje vlastní údaje, aplikuje kalibrace, ukládá data na SD kartu a odesílá je přes MQTT.
- **Monitor** – jednoduchý servisní firmware určený především pro diagnostiku RS485 komunikace.

Část kódu společná pro více firmware je uložena ve složce **shared**.

### Doporučené pořadí pro čtení zdrojového kódu

Pokud projekt vidíte poprvé, doporučujeme postupovat v tomto pořadí:

1. `Probe/Probe.ino` – hlavní program měřicí jednotky.
2. `Probe/SensorManager.cpp` – zde je definováno, jak se čtou senzory a do kterých polí se ukládají naměřené hodnoty.
3. `shared/SensorPacket.h` – interní reprezentace naměřených dat.
4. `shared/SmradPacket.h` – formát paketu přenášeného po RS485.
5. `shared/Rs485PacketSender.cpp` – odesílání paketu z Probe.
6. `Logger/Logger.ino` – hlavní tok programu Loggeru a FreeRTOS tasky.
7. `shared/Rs485PacketReceiver.cpp` – příjem a kontrola RS485 paketů.
8. `Logger/ConfigManager.cpp` – načtení konfigurace a kalibrace z SD karty.
9. `Logger/MqttPublisher.cpp` – převod dat do ThingSpeak/MQTT kanálů.
10. `Logger/SdLogger.cpp` – zápis měření a událostí na SD kartu.
11. `Logger/WatchdogManager.cpp` – diagnostika a recovery mechanismy.

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

## Struktura adresářů

### `Probe/`

Firmware spodní měřicí jednotky.

- `Probe.ino` – inicializace Probe, hlavní měřicí smyčka, RS485 odesílání a watchdog.
- `SensorManager.cpp/.h` – inicializace a čtení jednotlivých senzorů a mapování hodnot do `SensorPacket`.
- `ExplorIR_CO2.cpp/.h` – komunikace s CO2 senzorem ExplorIR přes UART.
- `ProbeStatus.h` – stav dostupnosti jednotlivých senzorů.
- `AppConfig.h` – piny, periody měření a další compile-time nastavení Probe.

### `Logger/`

Firmware horní jednotky.

- `Logger.ino` – hlavní aplikace a vytvoření FreeRTOS tasků.
- `BaseSensorManager.cpp/.h` – lokální měření Loggeru a doplňování servisních polí.
- `ConfigManager.cpp/.h` – načítání `config.json` a `calibration.json`.
- `Calibration.cpp/.h` – matematické kalibrační funkce.
- `WifiManager.cpp/.h` – připojení a recovery Wi-Fi.
- `MqttPublisher.cpp/.h` – MQTT připojení a publikování jednotlivých kanálů.
- `SdLogger.cpp/.h` – `data.csv`, `events.log` a recovery SD karty.
- `TimeSync.cpp/.h` – NTP synchronizace systémového času.
- `LedStatus.cpp/.h` – stavová RGB LED.
- `WatchdogManager.cpp/.h` – dohled nad tasky, konektivitou a stavem systému.
- `AppConfig.h` – compile-time výchozí nastavení Loggeru.

### `shared/`

Kód používaný více firmware projekty.

- `SensorPacket.h` – interní datová struktura obsahující až 24 hodnot a jejich validity.
- `SmradPacket.h` – binární formát RS485 protokolu.
- `Rs485PacketSender.*` – odesílání RS485 paketů.
- `Rs485PacketReceiver.*` – příjem paketů, kontrola hlavičky a CRC.
- `Crc16.h` – CRC16 kontrola paketů.
- `AnalogMeasure.*` – pomocná třída pro ADC měření, průměrování a EMA filtraci.
- `DebugLog.*` – společné debug/monitor výpisy.

Soubory stejného jména ve složkách `Logger`, `Probe` a `Monitor` jsou v řadě případů pouze **forwardery**. Skutečná implementace je ve `shared/`. Při opravě společného kódu proto nejprve ověřte, zda neupravujete pouze forwarder.

### `Monitor/`

Minimální diagnostický firmware pro příjem a zobrazení RS485 paketů bez celé logiky Loggeru.

Je vhodný zejména při hledání problémů mezi Probe a Loggerem.

### `SD/`

Příklady obsahu SD karty:

- `config.json` – runtime konfigurace Loggeru,
- `calibration.json` – kalibrační pravidla,
- `data.csv` – ukázka naměřených dat,
- `events.log` – ukázka systémového logu.

Tyto soubory slouží jako příklady provozní struktury SD karty a neměly by obsahovat produkční hesla nebo jiné tajné přístupové údaje.

## SW

Probe i Logger používají `setField()` s last-known-good stabilizací. Pokud aktuální čtení krátkodobě selže a existuje předchozí dobrá hodnota, může být poslední dobrá hodnota znovu použita.

Současná implementace **nemá časový limit stáří last-good hodnoty**. Je to vědomě ponechané současné chování; při interpretaci dat je potřeba s tím počítat. Pokud bude v budoucnu požadováno rozlišit dlouhodobý výpadek konkrétního senzoru, vhodným rozšířením je timestamp a maximální stáří cache.

Kanal 3 povazujeme za servisni ..krome attr 17 doplni vse Logger

| No.	| Parametr      | Zdroj             | Jednotka   | Min  | Max    | Pozn.                                                |   
| --- | --------------| ----------------- | ---------- | ---- | ------ | ---------------------------------------------------- | 
| 16	| Čas. značka	  | RTC	    	      |  min		   | 0   	| 	     | float (24 bit) minut od zacatku UNIX epochy          | 
| 17  | Napeti zdroje | Probe A2	      |  raw		   | 0	  | 4096	 | napeti zdroje probe  BAT                             | 
| 18  | Napeti zdroje | Logger A2	      |  raw		   | 0	  | 4096	 | napeti zdroje logger AC                              | 
| 19  | Napeti zdroje | Logger A3	      |  raw		   | 0	  | 4096	 | napeti zdroje logger BAT                             | 
| 20	| Temperature	  | RTC   		      |  °C		     | -40	| 85	   | RTC.readTemperature                                  | 

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

![smrad](/images/Testing.png)

## FreeRTOS tasky Loggeru


## FreeRTOS tasky Loggeru

- `sensorTask` – přijímá validní RS485 pakety, převádí je na `SensorPacket`, aplikuje kalibraci, doplňuje lokální hodnoty a předává paket do front.
- `networkTask` – často obsluhuje Wi-Fi/MQTT state machines; data publikuje samostatným pomalejším cyklem.
- `heartbeatTask` – stavová RGB LED.
- `watchdogTask` – kontrola heartbeatů a provozních stavů.
- `timeTask` – NTP synchronizace a RTC.
- `sdTask` – CSV logging na SD kartu.

**Task heartbeat znamená pouze to, že task běží.** Dostupnost RS485 dat se sleduje samostatně přes timestamp posledního validního paketu. Výpadek Probe proto není zaměněn za zamrznutí `sensorTask`.

Watchdog sleduje zejména:

- timeout jednotlivých FreeRTOS tasků,
- dlouhý výpadek Wi-Fi,
- dlouhý výpadek MQTT,
- dlouhou absenci úspěšného publish,
- dlouhou absenci validního RS485 paketu.

## Aplikační konstanty

Compile-time výchozí hodnoty jsou v `AppConfig.h`; Logger je může přepsat pomocí `/config.json` na SD kartě.

### Logger – důležité časování

```text
SENSOR_PERIOD_MS            20000 ms   publish cyklus Loggeru
MQTT_SPACING_MS              1200 ms   prodleva mezi ThingSpeak kanály
WIFI_RETRY_MS               15000 ms
MQTT_RETRY_MS                8000 ms
TIME_SYNC_INTERVAL_MS     3600000 ms
WATCHDOG_PERIOD_MS            5000 ms
RS485_NO_VALID_WARN_MS      600000 ms
```

`MQTT_SPACING_MS` **není četnost měření ani celého publish cyklu**. Je to pouze rozestup mezi publikací jednotlivých ThingSpeak kanálů jednoho paketu.

### Probe – důležité časování

```text
SENSOR_PERIOD_MS             10000 ms
PROBE_SENSOR_WATCHDOG_MS    120000 ms
PROBE_SEND_WATCHDOG_MS      120000 ms
```

## Práce s časem

Logger používá RTC DS3231 a NTP:

1. při startu se pokusí načíst validní čas z RTC,
2. po připojení Wi-Fi provede NTP synchronizaci,
3. úspěšně získaný čas zapíše zpět do RTC,
4. NTP synchronizaci pravidelně opakuje.

Použité NTP servery jsou definovány v implementaci `TimeSync`.

## SD konfigurace

`/config.json` přepisuje compile-time výchozí hodnoty z `Logger/AppConfig.h`. Pokud `config.json` chybí nebo je nevalidní, Logger pokračuje s výchozí konfigurací.

`/calibration.json` se načítá **nezávisle**. Pokud chybí nebo je nevalidní, platný `config.json` zůstává aktivní a Logger pokračuje bez kalibrací.

Příklad struktury bez skutečných hesel:

```json
{
  "device": {
    "name": "SMRAD-03",
    "location": "example",
    "hardwareRevision": 1
  },
  "wifi": {
    "ssid": "YOUR_WIFI",
    "password": "YOUR_WIFI_PASSWORD"
  },
  "mqtt": {
    "server": "mqtt3.thingspeak.com",
    "port": 1883,
    "clientId": "YOUR_CLIENT_ID",
    "username": "YOUR_USERNAME",
    "password": "YOUR_MQTT_PASSWORD",
    "channel1": "CHANNEL_1",
    "channel2": "CHANNEL_2",
    "channel3": "CHANNEL_3"
  },
  "timing": {
    "sensorPeriodMs": 20000,
    "mqttSpacingMs": 1200,
    "timeSyncIntervalMs": 3600000
  }
}
```

Události se zapisují do `events.log`, měření do `data.csv`.

## Měřené hodnoty a mapování polí

`SensorPacket` obsahuje 24 float hodnot a samostatné pole `valid[24]`. Pojmenované indexy jsou definované v `shared/SensorPacket.h`; při přidání nebo přesunu veličiny používejte tyto názvy místo číselných „magic numbers“.

Mapování do ThingSpeak:

```text
0..7   -> channel1 / field1..field8
8..15  -> channel2 / field1..field8
16..23 -> channel3 / field1..field8
```

SD logger i MQTT publisher respektují `packet.valid[]`. Numerická hodnota označená jako nevalidní se nepovažuje za platné měření.

### Channel 1 – raw / primární hodnoty z Probe

| Index | Symbol | Parametr | Zdroj | Jednotka v paketu | Poznámka |
| ---: | --- | --- | --- | --- | --- |
| 0 | `FIELD_TEMPERATURE` | teplota | BME280 | °C | `readTemperature()` |
| 1 | `FIELD_PRESSURE` | tlak | BME280 | kPa | `readPressure()/1000` |
| 2 | `FIELD_HUMIDITY` | relativní vlhkost | BME280 | % | `readHumidity()` |
| 3 | `FIELD_AIRFLOW` | rychlost proudění | FS3000-1015 | m/s | `readMetersPerSecond()` |
| 4 | `FIELD_O2_RAW` | O2 | DFRobot SEN0465 | ppm | raw hodnota z `readGasConcentrationPPM()` |
| 5 | `FIELD_CO2_RAW` | CO2 | ExplorIR-M-E-100 | ppm | `co2Reading.ppm` |
| 6 | `FIELD_METHANE_RAW` | methane / MQ4 | MQ-4 | raw ADC | filtrovaná raw hodnota |
| 7 | `FIELD_H2S_RAW` | H2S | analog vstup | raw ADC | filtrovaná raw hodnota |

Pokud je požadováno `% vol`, převod raw/ppm hodnoty je vhodné provést kalibrací do Channel 2. Například čistý převod ppm na `% vol` používá vztah `% = ppm / 10000`.

### Channel 2 – kalibrované hodnoty

Pole 8–15 jsou standardně vyhrazena pro výsledky kalibrace polí 0–7. Formát umožňuje výstup i do jiného platného indexu 0–23, ale pro čitelnost a kompatibilitu projektu doporučujeme zachovat běžné mapování `0->8`, `1->9`, …, `7->15`.

### Channel 3 – servisní data

| Index | Symbol | Parametr | Zdroj | Jednotka | Poznámka |
| ---: | --- | --- | --- | --- | --- |
| 16 | `FIELD_TIMESTAMP_MIN` | časová značka | Logger | min | Unix epoch v minutách uložený jako float |
| 17 | `FIELD_PROBE_POWER` | napájení Probe | Probe A2 | raw ADC | přichází z Probe |
| 18 | `FIELD_LOGGER_AC` | Logger pomocné napětí | Logger A2 | V | po aplikaci děliče |
| 19 | `FIELD_LOGGER_BAT` | Logger baterie | Logger A3 | V | po aplikaci děliče |
| 20 | `FIELD_RTC_TEMPERATURE` | teplota RTC | DS3231 | °C | doplňuje Logger |
| 21–23 | — | rezervováno | — | — | zatím nepoužito |

## Validita a „last good value“

Probe i Logger používají `setField()` s last-known-good stabilizací. Pokud aktuální čtení krátkodobě selže a existuje předchozí dobrá hodnota, může být poslední dobrá hodnota znovu použita.

Probe i Logger používají `setField()` s last-known-good stabilizací. Pokud aktuální čtení krátkodobě selže a existuje předchozí dobrá hodnota, může být poslední dobrá hodnota znovu použita.

Současná implementace **nemá časový limit stáří last-good hodnoty**. Je to vědomě ponechané současné chování; při interpretaci dat je potřeba s tím počítat. Pokud bude v budoucnu požadováno rozlišit dlouhodobý výpadek konkrétního senzoru, vhodným rozšířením je timestamp a maximální stáří cache.

Kanal 3 povazujeme za servisni ..krome attr 17 doplni vse Logger

| No.	| Parametr      | Zdroj             | Jednotka   | Min  | Max    | Pozn.                                                |   
| --- | --------------| ----------------- | ---------- | ---- | ------ | ---------------------------------------------------- | 
| 16	| Čas. značka	  | RTC	    	      |  min		   | 0   	| 	     | float (24 bit) minut od zacatku UNIX epochy          | 
| 17  | Napeti zdroje | Probe A2	      |  raw		   | 0	  | 4096	 | napeti zdroje probe  BAT                             | 
| 18  | Napeti zdroje | Logger A2	      |  raw		   | 0	  | 4096	 | napeti zdroje logger AC                              | 
| 19  | Napeti zdroje | Logger A3	      |  raw		   | 0	  | 4096	 | napeti zdroje logger BAT                             | 
| 20	| Temperature	  | RTC   		      |  °C		     | -40	| 85	   | RTC.readTemperature                                  | 

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

![smrad](/images/Testing.png)

## Kalibrace


## Kalibrace

Kalibrace je definována v `/calibration.json`. Firmware podporuje maximálně **8 kalibračních záznamů**. Každý vstupní index 0–7 může mít v současné implementaci jednu nalezenou kalibraci (`getCalibration()` vrací první odpovídající záznam).

Příklad:

```json
{
  "calibration": [
    { "in": 0, "out": 8,  "type": 1, "a": 1 },
    { "in": 1, "out": 9,  "type": 1, "a": 1 },
    { "in": 2, "out": 10, "type": 1, "a": 1 },
    { "in": 3, "out": 11, "type": 1, "a": 1 },
    { "in": 4, "out": 12, "type": 1, "a": 1 },
    { "in": 5, "out": 13, "type": 1, "a": 1 },
    { "in": 6, "out": 14, "type": 1, "a": 1 },
    { "in": 7, "out": 15, "type": 1, "a": 1 }
  ]
}
```

Při načítání se kontroluje:

- `in` v rozsahu 0–7,
- `out` v rozsahu 0–23,
- `type` v rozsahu 1–4,
- konečnost všech limitů a koeficientů,
- případně `lowerLimit <= upperLimit`.

Chybný záznam je zalogován a přeskočen; nevyřadí ostatní správné kalibrace.

### Rozsah platnosti

Pokud `lowerLimit != upperLimit`, platí kalibrace pouze pro:

```text
lowerLimit <= x <= upperLimit
```

Mimo rozsah současná implementace vrací `0.0`. Pokud jsou oba limity stejné, kontrola rozsahu se nepoužije.

### Typ 1 – Polynomial

Používá polynom až šestého řádu.
y = g + a·x + b·x² + c·x³ + d·x⁴ + e·x⁵ + f·x⁶

```text
y = g + a*x + b*x^2 + c*x^3 + d*x^4 + e*x^5 + f*x^6
```

### Typ 2 – Exponential

```text
y = a * exp(b*x) + c*x + d
```

### Typ 3 – Power

```text
y = a * (b+x)^c + d
```

Musí platit `b + x > 0`, jinak výsledek není validní (`NAN`).

### Typ 4 – Logarithmic

```text
y = a * ln(b*x+c) + d
```

Musí platit `b*x + c > 0`, jinak výsledek není validní (`NAN`).

Výstup kalibrace je označen jako validní pouze tehdy, když byl validní vstup **a zároveň** je výsledkem konečné číslo.

## Jak přidat nový senzor

1. Inicializaci hlavních měřicích senzorů přidejte do `Probe/SensorManager`.
2. Stav senzoru uložte do `ProbeStatus`.
3. V `SensorManager::read()` zapište hodnotu pomocí `setField(packet, FIELD_..., value, valid)`.
4. Pokud přidáváte nové pole, doplňte pojmenovaný index v `shared/SensorPacket.h` a tuto tabulku v README.
5. Nezměňte bez rozmyslu `SmradPacket` – změna wire formátu může vyžadovat zvýšení verze protokolu a současnou aktualizaci Probe, Loggeru i Monitoru.
6. Ověřte MQTT mapování a SD CSV výstup.

## RS485 diagnostika

Receiver je stavový a self-synchronizing. Kontroluje magic bytes, hlavičku a CRC16.

Diagnostické čítače:

- `packetsOk` – validní přijaté pakety,
- `crcErrors` – paket s chybným CRC,
- `headerErrors` – neplatná hlavička,
- `magicErrors` – bajty mimo očekávanou synchronizaci,
- `timeoutErrors` – **pouze rozpracovaný paket, který nebyl dokončen v timeoutu**. Běžné čekání mezi periodickými pakety se za chybu nepočítá.

## Nastavení vývojového prostředí

Používané nastavení ESP32-S3:

```text
Board: ESP32S3 Dev Module
Upload: 921600
USB Mode: Hardware CDC and JTAG
USB CDC on boot: Enabled
USB Firmware MSC on boot: Disabled
USB DFU on boot: Disabled
CPU frequency: 240 MHz
Flash mode: QIO 80 MHz
Partition scheme: Default 4MB with SPIFFS
PSRAM: OPI PSRAM
Flash Size: 4M (32M)
```

![Testing](/images/Testing.png)

Hlavní externí knihovny použité projektem:

- ArduinoJson,
- PubSubClient,
- Adafruit BME280 / Adafruit Unified Sensor,
- Adafruit NeoPixel,
- SparkFun FS3000,
- DFRobot MultiGasSensor,
- Makuna RTC (`RtcDS3231`).

Pro reprodukovatelný production build je vhodné do budoucna zapsat také konkrétně ověřené verze knihoven a ESP32 Arduino core.

## Bezpečnost konfigurace

Produkční Wi-Fi a MQTT credentials **nepatří do veřejného Git repository ani do README**. Tento projekt historicky obsahoval reálné credentials v konfiguračních souborech / zdrojácích a Git historii.

Před zveřejněním repository doporučujeme:

- rotovat již commitnutá hesla/API údaje,
- používat `mqtt_secrets.example.h` a lokální necommitovaný `mqtt_secrets.h`,
- používat anonymizovaný `config.example.json`,
- doplnit odpovídající `.gitignore`,
- nezapomenout, že odstranění secretu z aktuálního souboru jej samo o sobě neodstraní ze starších commitů.
