#include <Arduino.h>

#include "SmradPacket.h"
#include "Rs485PacketReceiver.h"

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>

#include "AppConfig.h"
#include "SensorPacket.h"

#include "DebugLog.h"
#include "LedStatus.h"
#include "WifiManager.h"
#include "TimeSync.h"
#include "BaseSensorManager.h"
#include "MqttPublisher.h"
#include "SdLogger.h"
#include "WatchdogManager.h"
#include "ConfigManager.h"
#include "Calibration.h"

#include <sys/time.h>
#include <RtcDS3231.h> // Rtc by Makuna

// =============================================================
// GLOBALS
// =============================================================

WiFiClient wifiClient;
MqttPublisher Mqtt(wifiClient);

QueueHandle_t packetQueue = nullptr;
QueueHandle_t sdQueue = nullptr;

TaskHandle_t sensorTaskHandle = nullptr;
TaskHandle_t networkTaskHandle = nullptr;
TaskHandle_t heartbeatTaskHandle = nullptr;
TaskHandle_t watchdogTaskHandle = nullptr;
TaskHandle_t timeTaskHandle = nullptr;
TaskHandle_t sdTaskHandle = nullptr;

TaskHealth timeHealth    = { "timeTask",      0, TIME_TASK_TIMEOUT_MS    };
TaskHealth sensorHealth  = { "sensorTask",    0, SENSOR_TASK_TIMEOUT_MS  };
TaskHealth networkHealth = { "networkTask",   0, NETWORK_TASK_TIMEOUT_MS };
TaskHealth ledHealth     = { "heartbeatTask", 0, LED_TASK_TIMEOUT_MS     };
TaskHealth sdHealth      = { "sdTask",        0, SD_TASK_TIMEOUT_MS      };

HardwareSerial rs485Uart(1);
Rs485PacketReceiver rs485(rs485Uart, RS485_RX_PIN, RS485_TX_PIN, RS485_BAUD);

RtcDS3231<TwoWire> rtc(Wire); 
bool rtcLoaded = false;

void convertToSensorPacket(const SmradPacket &in, SensorPacket &out)
{
    for (uint8_t i = 0; i < FIELD_COUNT; i++)
    {
        out.f[i] = NAN;
        out.valid[i] = false;
    }
    
    out.sequence = in.sequence;
    out.timestampMs = millis();
    
    uint8_t count = min((uint8_t)FIELD_COUNT, in.fieldCount);

    for (uint8_t i = 0; i < count; i++)
    {
        out.f[i] = in.values[i];
        out.valid[i] = smradPacketFieldValid(in, i);
    }
        
    // Calibration
    for (uint8_t i = 0; i < count; i++)
    {
        // calibration
        const CalibrationConfig* cal = Config.getCalibration(i);
        if (cal)
        {
            float value = calibrate(in.values[i],cal);

            Log.printf(
                "CAL Calibration found: in=%lu -> out=%lu type=%lu %.3f -> %.3f",
                cal->cIn,
                cal->cOut,
                cal->cType, in.values[i], value); 
            out.f[cal->cOut] = value;                      
        }        
    }
}

// =============================================================
// SENSOR TASK
// =============================================================

void sensorTask(void *pv)
{
    (void)pv;

    SmradPacket in;

    for (;;)
    {
        SensorPacket packet;
        smradPacketInit(in);

        if (rs485.receive(in, 250))
        {
            convertToSensorPacket(in, packet);

            // Calibration - monitor CSV
            Log.monitor(
                //"DATA packet #%lu ok=%lu crc=%lu timeout=%lu, values: "
                "%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f",
                //in.sequence,
                //rs485.packetsOk(),
                //rs485.crcErrors(),
                //rs485.timeoutErrors(),
                packet.f[0],
                packet.f[1],
                packet.f[2],
                packet.f[3],
                packet.f[4],
                packet.f[5],
                packet.f[6],
                packet.f[7],
                packet.f[8],
                packet.f[9],
                packet.f[10],
                packet.f[11],
                packet.f[12],
                packet.f[13],
                packet.f[14],
                packet.f[15],
                packet.f[16],
                packet.f[17],
                packet.f[18],
                packet.f[19],
                packet.f[20],
                packet.f[21],
                packet.f[22],
                packet.f[23]
            );

            Sensors.addLocalData(packet); // extra data
            
            RtcTemperature temp = rtc.GetTemperature();
            float t = temp.AsFloatDegC();
            Log.printf("RTC temp = %.2f", t);
            Sensors.addLocalSlotData(packet,20,t); // extra data

            if (packetQueue)
                xQueueOverwrite(packetQueue, &packet);

            if (sdQueue)
                xQueueSend(sdQueue, &packet, 0);

            Watchdog.markAlive(sensorHealth);

        }        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// =============================================================
// NETWORK TASK
// =============================================================

void networkTask(void *pv)
{
    (void)pv;

    SensorPacket packet;

    for (;;)
    {
        Wifi.tick();
        Mqtt.tick();

        if (Mqtt.connected())
        {
            if (packetQueue && xQueueReceive(packetQueue, &packet, pdMS_TO_TICKS(100)))
            {
                if (Mqtt.publishPacket(packet))
                    StatusLed.showSentBlink();
            }
        }

        Watchdog.markAlive(networkHealth);
        vTaskDelay(pdMS_TO_TICKS(Config.get().timing.sensorPeriodMs));
    }
}

// =============================================================
// HEARTBEAT TASK
// =============================================================

void heartbeatTask(void *pv)
{
    (void)pv;

    for (;;)
    {
        StatusLed.update(Wifi.connected(), Mqtt.connected());

        Watchdog.markAlive(ledHealth);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// =============================================================
// TIME TASK
// =============================================================

bool rtcInit() {
    rtc.Begin(); // Inicializace knihovny (Pozor, velké 'B')

    if (!rtc.GetIsRunning()) {
        Log.printf("RTC was not actively running, starting now.");
        rtc.SetIsRunning(true);
    }
    
    // rtc.lostPower() z Adafruit se zde jmenuje takto:
    if (!rtc.IsDateTimeValid()) {
        Log.printf("RTC lost power or time is invalid!");
    }
    return loadTimeFromRtc();
}

void printSystemTime()
{
    time_t now;
    time(&now);

    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    Log.printf("SYS time: %04d-%02d-%02d %02d:%02d:%02d",
                  timeinfo.tm_year + 1900,
                  timeinfo.tm_mon + 1,
                  timeinfo.tm_mday,
                  timeinfo.tm_hour,
                  timeinfo.tm_min,
                  timeinfo.tm_sec);
}

bool loadTimeFromRtc() {
    RtcDateTime dt = rtc.GetDateTime();
    
    if (!dt.IsValid()) {
        Log.printf("RTC failure / not set");
        return false;
    }

    // Výpis do logu
    Log.printf("RTC time is: %04d-%02d-%02d %02d:%02d:%02d",
               dt.Year(), dt.Month(), dt.Day(), dt.Hour(), dt.Minute(), dt.Second());

    if (dt.Year() < 2024) return false;

    // Set time on ESP32 (Makuna má přímo export do Unix Epoch32)
    timeval tv;
    tv.tv_sec = dt.Epoch32Time(); 
    tv.tv_usec = 0;
    settimeofday(&tv, nullptr);
    return true;
}

bool saveTimeToRtc() {
    time_t now;
    time(&now);

    if (now < 1700000000) {
        Log.printf("RTC System time invalid");
        return false;
    }

    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    // Vytvoření objektu času pro Makuna knihovnu
    RtcDateTime dt2(
        timeinfo.tm_year + 1900,
        timeinfo.tm_mon + 1,
        timeinfo.tm_mday,
        timeinfo.tm_hour,
        timeinfo.tm_min,
        timeinfo.tm_sec
    );

    printSystemTime();
    rtc.SetDateTime(dt2); // Ekvivalent k rtc.adjust()

    // Okamžitá kontrola vyčtením
    RtcDateTime dtCheck = rtc.GetDateTime();
    Log.printf("RTC time set to: %04d-%02d-%02d %02d:%02d:%02d",
               dtCheck.Year(), dtCheck.Month(), dtCheck.Day(), dtCheck.Hour(), dtCheck.Minute(), dtCheck.Second());
    return true;
}

void timeTask(void *pv)
{
    (void)pv;

    bool firstSyncDone = false;

    for (;;)
    {
        if (Wifi.connected())
        {
            uint32_t nowMs = millis();
            bool needSync = false;

            if (!firstSyncDone)
            {
                needSync = true;
            }
            else if (nowMs - Clock.lastSyncMs() > Config.get().timing.timeSyncIntervalMs)
            {
                needSync = true;
            }

            if (needSync)
            {
                if (Clock.sync())
                {
                    firstSyncDone = true;
                    saveTimeToRtc();
                }
            }
        }

        Watchdog.markAlive(timeHealth);
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

// =============================================================
// SD TASK
// =============================================================

void sdTask(void *pv)
{
    (void)pv;

    SensorPacket packet;

    Log.printf("SD task started");

    for (;;)
    {
        if (sdQueue && xQueueReceive(sdQueue, &packet, pdMS_TO_TICKS(1000)))
        {
            if (!SdLog.logPacketCsv(packet))
            {
                SdLog.recoverIfNeeded();

                if (SdLog.available())
                    SdLog.logPacketCsv(packet);
            }
        }

        Watchdog.markAlive(sdHealth);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// =============================================================
// WATCHDOG TASK
// =============================================================

void watchdogTask(void *pv)
{
    (void)pv;

    Log.printf("WD Watchdog task started");

    TaskHealth* healthList[] =
    {
        &timeHealth,
        &sensorHealth,
        &networkHealth,
        &ledHealth,
        &sdHealth
    };

    for (;;)
    {
        Watchdog.checkTasks(healthList, sizeof(healthList) / sizeof(healthList[0]));
        Watchdog.checkConnectivity(Wifi, Mqtt);
        Watchdog.checkSensors(Sensors);

        Watchdog.logDiagnostics(
            sensorTaskHandle,
            networkTaskHandle,
            heartbeatTaskHandle,
            timeTaskHandle,
            sdTaskHandle,
            watchdogTaskHandle,
            Wifi,
            Mqtt,
            SdLog
        );

        vTaskDelay(pdMS_TO_TICKS(WATCHDOG_PERIOD_MS));
    }
}

// =============================================================
// SETUP
// =============================================================

void setup()
{
    Serial.begin(115200);
    delay(500);

    Log.begin(Serial, 115200);

    Log.printf("----------------------------------------------------------------------");
    Log.printf("Subsurface Multi-gas Respiration and Anomaly Detector 0.22 Base-logger");
    Log.printf("----------------------------------------------------------------------");

    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(50000);
    
    rtcInit();
    
    rs485.begin();

    Watchdog.begin();

    StatusLed.begin();

    SdLog.begin();

    Config.begin();

    SdLog.ensureDataHeader();
    SdLog.logEvent("boot");

    Mqtt.begin();
    Sensors.begin();

    packetQueue = xQueueCreate(1, sizeof(SensorPacket));
    sdQueue = xQueueCreate(SD_QUEUE_LENGTH, sizeof(SensorPacket));

    if (!packetQueue)
    {
        Log.printf("Queue create failed");
        while (true)
            delay(1000);
    }

    if (!sdQueue)
    {
        Log.printf("SD queue create failed");
        while (true)
            delay(1000);
    }

    uint32_t nowMs = millis();

    timeHealth.lastAliveMs = nowMs;
    sensorHealth.lastAliveMs = nowMs;
    networkHealth.lastAliveMs = nowMs;
    ledHealth.lastAliveMs = nowMs;
    sdHealth.lastAliveMs = nowMs;

    xTaskCreatePinnedToCore(timeTask,      "time",     4096, NULL, 1, &timeTaskHandle,      0);
    xTaskCreatePinnedToCore(sensorTask,    "sensor",   6144, NULL, 2, &sensorTaskHandle,    1);
    xTaskCreatePinnedToCore(networkTask,   "network",  8192, NULL, 1, &networkTaskHandle,   0);
    xTaskCreatePinnedToCore(heartbeatTask, "led",      2048, NULL, 1, &heartbeatTaskHandle, 1);
    xTaskCreatePinnedToCore(sdTask,        "sd",       4096, NULL, 1, &sdTaskHandle,        1);
    xTaskCreatePinnedToCore(watchdogTask,  "watchdog", 4096, NULL, 3, &watchdogTaskHandle,  0);
}

void loop()
{
    vTaskDelay(pdMS_TO_TICKS(1000));
}


