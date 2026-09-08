// =============================================================
// RS485 PACKET RECEIVER
// =============================================================
//
// Purpose
// -------------------------------------------------------------
// Receives binary SmradPacket structures from remote SMRAD
// PROBE units over UART / RS485.
//
// The receiver is designed for noisy field conditions and
// continuous operation.
//
// Main features
// -------------------------------------------------------------
// - self-synchronizing parser
// - magic byte framing
// - CRC16 packet validation
// - survives corrupted packets
// - survives byte loss
// - survives partial packets
// - survives line noise
// - continuous stream parsing
// - detailed debug logging
//
// Synchronization model
// -------------------------------------------------------------
// The parser continuously scans incoming bytes:
//
//   WAIT_MAGIC1
//        ↓
//   WAIT_MAGIC2
//        ↓
//    READ_BODY
//
// If any corruption is detected:
//
//   CRC error
//   invalid header
//   wrong magic
//   truncated packet
//
// the parser immediately falls back to searching for the next
// valid packet start sequence.
//
// No destructive input flushing is used during runtime.
//
// Packet format
// -------------------------------------------------------------
//
//   magic1
//   magic2
//   version
//   fieldCount
//   sequence
//   uptime
//   sensor values[]
//   validMask
//   crc16
//
// Design philosophy
// -------------------------------------------------------------
// The receiver is intentionally stateful and defensive.
//
// It assumes:
// - packets may be damaged
// - bytes may be lost
// - packets may overlap
// - receiver may start in the middle of a stream
//
// The parser therefore never trusts packet alignment.
//
// Practical notes
// -------------------------------------------------------------
// RS485 itself is not required.
// The same parser can be used with:
//
// - direct UART
// - RS485
// - radio UART bridge
// - LoRa serial bridge
// - TCP serial tunnel
//
// =============================================================

#include "Rs485PacketReceiver.h"

Rs485PacketReceiver::Rs485PacketReceiver(
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

void Rs485PacketReceiver::resetParser()
{
    memset(&_work, 0, sizeof(_work));

    _state = WAIT_MAGIC1;
    _pos = 0;
}

void Rs485PacketReceiver::begin()
{
    _serial.setRxBufferSize(1024);
    _serial.begin(_baud, SERIAL_8N1, _rxPin, _txPin);

    while (_serial.available())
        _serial.read();

    resetParser();

    Log.printf(
        "RS485 receiver started RX=%d TX=%d baud=%lu packetSize=%u crcOffset=%u",
        _rxPin,
        _txPin,
        _baud,
        (unsigned)sizeof(SmradPacket),
        (unsigned)smradPacketCrcOffset()
    );
}

bool Rs485PacketReceiver::receive(SmradPacket &packet, uint32_t timeoutMs)
{
    uint8_t* raw = reinterpret_cast<uint8_t*>(&_work);

    uint32_t startMs = millis();

    while (millis() - startMs < timeoutMs)
    {
        if (!_serial.available())
        {
            delay(1);
            continue;
        }

        uint8_t b = _serial.read();

        switch (_state)
        {
            case WAIT_MAGIC1:
                if (b == SMRAD_PACKET_MAGIC_1)
                {
                    memset(&_work, 0, sizeof(_work));
                    raw = reinterpret_cast<uint8_t*>(&_work);

                    _pos = 0;
                    raw[_pos++] = b;

                    _state = WAIT_MAGIC2;
                }
                else
                {
                    _magicErrors++;
                }
                break;

            case WAIT_MAGIC2:
                if (b == SMRAD_PACKET_MAGIC_2)
                {
                    raw[_pos++] = b;
                    _state = READ_BODY;
                }
                else
                {
                    _magicErrors++;

                    if (b == SMRAD_PACKET_MAGIC_1)
                    {
                        memset(&_work, 0, sizeof(_work));
                        raw = reinterpret_cast<uint8_t*>(&_work);

                        _pos = 0;
                        raw[_pos++] = b;

                        _state = WAIT_MAGIC2;
                    }
                    else
                    {
                        resetParser();
                    }
                }
                break;

            case READ_BODY:
                if (_pos >= sizeof(SmradPacket))
                {
                    resetParser();
                    break;
                }

                raw[_pos++] = b;

                if (_pos < sizeof(SmradPacket))
                    break;

                if (!smradPacketBasicValid(_work))
                {
                    _headerErrors++;

                    Log.printf(
                        "rs485 invalid header magic=%02X%02X ver=%u fields=%u",
                        _work.magic1,
                        _work.magic2,
                        _work.version,
                        _work.fieldCount
                    );

                    resetParser();
                    break;
                }

                {
                    uint16_t expected = crc16ccitt(
                        reinterpret_cast<const uint8_t*>(&_work),
                        smradPacketCrcOffset()
                    );

                    if (expected != _work.crc)
                    {
                        _crcErrors++;

                        Log.printf(
                            "rs485 crc error expected=0x%04X got=0x%04X seq=%lu",
                            expected,
                            _work.crc,
                            _work.sequence
                        );

                        resetParser();
                        break;
                    }
                }

                memcpy(&packet, &_work, sizeof(SmradPacket));

                _packetsOk++;

                Log.printf(
                    "rs485 packet ok seq=%lu fields=%u mask=0x%08lX",
                    packet.sequence,
                    packet.fieldCount,
                    packet.validMask
                );

                resetParser();
                return true;
        }
    }

    // An idle line between periodic packets is normal. Count a timeout
    // only if a frame had already started but did not finish in time.
    if (_state != WAIT_MAGIC1)
    {
        _timeoutErrors++;
        resetParser();
    }

    return false;
}

uint32_t Rs485PacketReceiver::packetsOk() const
{
    return _packetsOk;
}

uint32_t Rs485PacketReceiver::crcErrors() const
{
    return _crcErrors;
}

uint32_t Rs485PacketReceiver::timeoutErrors() const
{
    return _timeoutErrors;
}

uint32_t Rs485PacketReceiver::magicErrors() const
{
    return _magicErrors;
}

uint32_t Rs485PacketReceiver::headerErrors() const
{
    return _headerErrors;
}