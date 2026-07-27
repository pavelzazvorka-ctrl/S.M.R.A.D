#pragma once

#include <Arduino.h>

#include "SmradPacket.h"
#include "Crc16.h"
#include "DebugLog.h"

class Rs485PacketReceiver
{
public:
    Rs485PacketReceiver(
        HardwareSerial &serial,
        int rxPin,
        int txPin,
        uint32_t baud = 9600
    );

    void begin();

    bool receive(
        SmradPacket &packet,
        uint32_t timeoutMs = 100
    );

    uint32_t packetsOk() const;
    uint32_t crcErrors() const;
    uint32_t timeoutErrors() const;
    uint32_t magicErrors() const;
    uint32_t headerErrors() const;

private:
    enum ParserState
    {
        WAIT_MAGIC1,
        WAIT_MAGIC2,
        WAIT_MAGIC3,
        WAIT_MAGIC4,
        READ_BODY
    };

    HardwareSerial &_serial;

    int _rxPin;
    int _txPin;
    uint32_t _baud;

    ParserState _state = WAIT_MAGIC1;

    SmradPacket _work;
    size_t _pos = 0;

    uint32_t _packetsOk = 0;
    uint32_t _crcErrors = 0;
    uint32_t _timeoutErrors = 0;
    uint32_t _magicErrors = 0;
    uint32_t _headerErrors = 0;

    void resetParser();
};