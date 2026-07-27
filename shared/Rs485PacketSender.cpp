#include "Rs485PacketSender.h"

Rs485PacketSender::Rs485PacketSender(
    HardwareSerial &serial,
    int rxPin,
    int txPin,
    uint32_t baud
)
    :
    _serial(serial),
    _rxPin(rxPin),
    _txPin(txPin),
    _baud(baud)
{
}

void Rs485PacketSender::begin()
{
    _serial.begin(_baud, SERIAL_8N1, _rxPin, _txPin);

    Log.printf(
        "RS485 sender started RX=%d TX=%d baud=%lu packetSize=%u crcOffset=%u",
        _rxPin,
        _txPin,
        _baud,
        (unsigned)sizeof(SmradPacket),
        (unsigned)smradPacketCrcOffset()
    );
}

bool Rs485PacketSender::send(SmradPacket &packet)
{
    if (!smradPacketBasicValid(packet))
    {
        Log.printf("rs485 tx invalid packet header");
        return false;
    }

    packet.uptimeMs = millis();

    packet.crc = 0;

    packet.crc = crc16ccitt(
        reinterpret_cast<const uint8_t*>(&packet),
        smradPacketCrcOffset()
    );

    logPacket(packet);

    size_t written = _serial.write(
        reinterpret_cast<const uint8_t*>(&packet),
        sizeof(SmradPacket)
    );

    _serial.flush();

    if (written != sizeof(SmradPacket))
    {
        Log.printf(
            "rs485 tx failed written=%u expected=%u",
            (unsigned)written,
            (unsigned)sizeof(SmradPacket)
        );
        return false;
    }

    Log.printf("rs485 tx ok seq=%lu crc=0x%04X", packet.sequence, packet.crc);
    return true;
}

void Rs485PacketSender::logPacket(const SmradPacket &packet)
{
    Log.printf(
        "rs485 tx packet seq=%lu uptime=%lu fields=%u mask=0x%08lX crc=0x%04X",
        packet.sequence,
        packet.uptimeMs,
        packet.fieldCount,
        packet.validMask,
        packet.crc
    );

    for (uint8_t i = 0; i < packet.fieldCount && i < SMRAD_MAX_FIELDS; i+=8)
    {
        Log.printf(
            "rs485 tx f%02u valid=%u values %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f",
            i + 1,
            smradPacketFieldValid(packet, i) ? 1 : 0,
            packet.values[i],packet.values[i+1],packet.values[i+2],packet.values[i+3],
            packet.values[i+4],packet.values[i+5],packet.values[i+6],packet.values[i+7]
        );
    }
}