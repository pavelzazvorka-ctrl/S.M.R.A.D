# Subsurface Multi-gas Respiration \& Anomaly Detector

![smrad](/images/smrad.png)

SMRAD je autonomní vícekanálový environmentální a plynový logger založený na ESP32-S3 a FreeRTOS. Firmware je navržený pro dlouhodobý bezobslužný provoz.

![smrad](/images/gas PoC.png)

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

Systém používá dvojici ESP32 S3 chipu propojenych linkou RS485.
Je rozdělen na "horní" (Logger) a "dolní" (Probe) stanici.
Probe je zodpovědný za měření hodnot a jejich serializaci na RS485.
Logger data přeposílá přes wifi a 5G modem do MQTT a zároveň je zapisuje na SD kartu.

Logger PINS
+ LED\_PIN        38
+ SD\_CS\_PIN     10
+ SD\_MOSI\_PIN   11
+ SD\_SCK\_PIN    12
+ SD\_MISO\_PIN   13
+ SDA\_PIN        16
+ SCL\_PIN        17
+ RS485_RX_PIN    15
+ RS485_TX_PIN    18
+ POWER\_VIN\_PIN A2
+ POWER\_BAT\_PIN A3

Probe PINS
+ SDA\_PIN         8
+ SCL\_PIN         9
+ LED\_PIN         38
+ CO2\_RX          16
+ CO2\_TX          17
+ MQ4\_PIN         A0
+ MQ8\_PIN         A1
+ POWER\_VIN\_PIN  A2
+ RS485_RX_PIN   4
+ RS485_TX_PIN   5

## SW

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
| 19  | Napeti zdroje | Logger A2		      |  raw		   | 0	  | 4096	 | napeti zdroje logger BAT                             | 
| 20	| Temperature	  | RTC   			      |  °C		     | -40	| 85	   | RTC.readTemperature                                  | 

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