#include <Arduino.h>

#include "SmradPacket.h"
#include "Rs485PacketReceiver.h"

#include "AppConfig.h"
#include "SensorPacket.h"
#include "DebugLog.h"

#define RS485_RX_PIN   10
#define RS485_TX_PIN   9
#define RS485_BAUD     115200

HardwareSerial rs485Uart(1);
Rs485PacketReceiver rs485(rs485Uart, RS485_RX_PIN, RS485_TX_PIN, RS485_BAUD);

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
}

void setup()
{
    Serial.begin(115200);
    delay(300);

    Log.begin(Serial, 115200);
    Log.printf("RS485 Monitor ready ..");

    rs485.begin();
}

void loop()
{
    SmradPacket in;
    SensorPacket packet;

    smradPacketInit(in);
    if (rs485.receive(in, 500))
    {
        convertToSensorPacket(in, packet);

        Log.printf(
            "Sensor packet #%lu ok=%lu crc=%lu timeout=%lu, values: %f %f %f %f %f %f %f %f",
            in.sequence,
            rs485.packetsOk(),
            rs485.crcErrors(),
            rs485.timeoutErrors(),
            packet.f[0],
            packet.f[1],
            packet.f[2],
            packet.f[3],
            packet.f[4],
            packet.f[5],
            packet.f[6],
            packet.f[7]
        );

        // Full BASE firmware:
        // xQueueOverwrite(packetQueue, &packet);
        // xQueueSend(sdQueue, &packet, 0);
    }

    delay(10);
}
