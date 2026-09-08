#include <esp_task_wdt.h>
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>

#include "SmradPacket.h"
#include "Rs485PacketSender.h"

#include "AppConfig.h"
#include "SensorPacket.h"
#include "SensorManager.h"
#include "DebugLog.h"
 
#define RS485_RX_PIN   4
#define RS485_TX_PIN   5
#define RS485_BAUD     115200

HardwareSerial rs485Uart(1);
Rs485PacketSender rs485(rs485Uart, RS485_RX_PIN, RS485_TX_PIN, RS485_BAUD);
Adafruit_NeoPixel rgb( 1, LED_PIN, NEO_GRB + NEO_KHZ800  );
uint32_t probeSequence = 0;

uint32_t lastSensorOkMs = 0;
uint32_t lastSendOkMs = 0;

void probeRestart(const char* reason)
{
    Log.printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    Log.printf("PROBE RESTART REQUESTED: %s", reason);
    Log.printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");

    rgb.setPixelColor(0, rgb.Color(0, 50, 0));
    rgb.show();

    delay(1000);
    ESP.restart();
}

void checkProbeWatchdog()
{
    uint32_t now = millis();

    if (lastSensorOkMs && now - lastSensorOkMs > PROBE_SENSOR_WATCHDOG_MS)
        probeRestart("sensor read timeout");

    if (lastSendOkMs && now - lastSendOkMs > PROBE_SEND_WATCHDOG_MS)
        probeRestart("rs485 send timeout");
}

void watchdogDelay(uint32_t delayMs)
{
    uint32_t start = millis();

    while (millis() - start < delayMs)
    {
        esp_task_wdt_reset();
        checkProbeWatchdog();

        rgb.setPixelColor(0, rgb.Color(2, 2, 2));
        rgb.show();
        delay(50);

        rgb.setPixelColor(0, 0);
        rgb.show();

        delay(950);
    }
}

void fillRs485Packet(SmradPacket &out, const SensorPacket &in)
{
    smradPacketInit(out);
    out.sequence = ++probeSequence;
    out.uptimeMs = millis();
    out.fieldCount = FIELD_COUNT;
    for (uint8_t i = 0; i < FIELD_COUNT && i < SMRAD_MAX_FIELDS; i++)
        smradPacketSetField(out, i, in.f[i], in.valid[i]);
}

void BlinkStatus(int nn, bool status)
{
    for(int i=0; i<nn; i++)
    {
        // GRB not RGB :-)
        rgb.setPixelColor(0, rgb.Color(100, 100, 100));
        rgb.show();
        delay(200);
        rgb.setPixelColor(0, rgb.Color(0, 0, 0));        
        rgb.show();
        delay(200);
    }

    if (status)
    {
        rgb.setPixelColor(0, rgb.Color(0, 255, 0));
    }
    else
    {
        rgb.setPixelColor(0, rgb.Color(255, 0, 0));
    }
    rgb.show();        
    delay(400);
    rgb.setPixelColor(0, rgb.Color(0, 0, 0));
    rgb.show();
    delay(200);
}

void setup()
{
    Serial.begin(115200);
    delay(300);
    
    Log.begin(Serial, 115200);
    Log.printf("------------------------------------------------------------------");
    Log.printf("Subsurface Multi-gas Respiration and Anomaly Detector 0.25 - PROBE");
    Log.printf("------------------------------------------------------------------");

    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(50000);

    rgb.begin();
    rgb.setPixelColor(0, rgb.Color(50, 50, 50));
    rgb.show();

    Sensors.begin();
    Sensors.logSelfTest();

    BlinkStatus(1,Sensors.status().bme);
    BlinkStatus(2,Sensors.status().co2);
    BlinkStatus(3,Sensors.status().fs3000);
    BlinkStatus(4,Sensors.status().h2s);
    BlinkStatus(5,Sensors.status().mq4);
    BlinkStatus(6,Sensors.status().o2);

    rs485.begin();
    
    Log.printf("Probe ready ...");
    rgb.setPixelColor(0, rgb.Color(20, 20, 20));
    rgb.show();

    // WDT
    esp_err_t err = esp_task_wdt_add(NULL);
    if (err == ESP_OK)
    {
        Log.printf("Probe WDT task added");
    }
    else
    {
        Log.printf("Probe WDT add failed: %d", err);
    }

    uint32_t now = millis();
    lastSensorOkMs = now;
    lastSendOkMs = now;
}

void loop()
{
    SensorPacket sensorPacket;
    SmradPacket rs485Packet;

    Sensors.read(sensorPacket);
    lastSensorOkMs = millis();

    fillRs485Packet(rs485Packet, sensorPacket);

    if (rs485.send(rs485Packet))
    {
        lastSendOkMs = millis();

        Log.printf("RS485 sent packet #%lu", rs485Packet.sequence);

        rgb.setPixelColor(0, rgb.Color(50, 0, 0));
        rgb.show();
        delay(200);
        rgb.setPixelColor(0, 0);
        rgb.show();
    }
    else
    {
        Log.printf("RS485 send failed");

        rgb.setPixelColor(0, rgb.Color(0, 50, 0));
        rgb.show();
    }

    checkProbeWatchdog();
    esp_task_wdt_reset();

    watchdogDelay(SENSOR_PERIOD_MS);

}