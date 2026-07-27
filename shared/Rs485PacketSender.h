#pragma once

#include <Arduino.h>

#include "SmradPacket.h"
#include "Crc16.h"
#include "DebugLog.h"

class Rs485PacketSender
{
public:
    Rs485PacketSender(
        HardwareSerial &serial,
        int rxPin,
        int txPin,
        uint32_t baud = 9600
    );

    void begin();

    bool send(SmradPacket &packet);

private:
    HardwareSerial &_serial;

    int _rxPin;
    int _txPin;
    uint32_t _baud;

    void logPacket(const SmradPacket &packet);
};