#include "protocol.h"

#include <QtEndian>

namespace Protocol {

static const QByteArray Magic("WSH1", 4);

quint32 crc32(const QByteArray &data)
{
    quint32 crc = 0xffffffffU;
    for (const char byte : data) {
        crc ^= static_cast<quint8>(byte);
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
    }
    return ~crc;
}

void appendU16(QByteArray &data, quint16 value)
{
    const quint16 little = qToLittleEndian(value);
    data.append(reinterpret_cast<const char *>(&little), sizeof(little));
}

void appendU32(QByteArray &data, quint32 value)
{
    const quint32 little = qToLittleEndian(value);
    data.append(reinterpret_cast<const char *>(&little), sizeof(little));
}

void appendU64(QByteArray &data, quint64 value)
{
    const quint64 little = qToLittleEndian(value);
    data.append(reinterpret_cast<const char *>(&little), sizeof(little));
}

bool readU16(const QByteArray &data, int &offset, quint16 &value)
{
    if (offset < 0 || data.size() - offset < static_cast<int>(sizeof(value)))
        return false;
    value = qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(data.constData() + offset));
    offset += sizeof(value);
    return true;
}

bool readU32(const QByteArray &data, int &offset, quint32 &value)
{
    if (offset < 0 || data.size() - offset < static_cast<int>(sizeof(value)))
        return false;
    value = qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(data.constData() + offset));
    offset += sizeof(value);
    return true;
}

bool readU64(const QByteArray &data, int &offset, quint64 &value)
{
    if (offset < 0 || data.size() - offset < static_cast<int>(sizeof(value)))
        return false;
    value = qFromLittleEndian<quint64>(reinterpret_cast<const uchar *>(data.constData() + offset));
    offset += sizeof(value);
    return true;
}

QByteArray encodeFrame(quint8 type, const QByteArray &payload)
{
    if (payload.size() > static_cast<int>(MaxPayloadSize))
        return QByteArray();

    QByteArray frame;
    frame.reserve(HeaderSize + payload.size());
    frame.append(Magic);
    frame.append(char(2));
    frame.append(char(type));
    appendU16(frame, 0);
    appendU32(frame, static_cast<quint32>(payload.size()));
    appendU32(frame, crc32(payload));
    frame.append(payload);
    return frame;
}

bool takeFrame(QByteArray &buffer, Frame &frame, QString *error)
{
    int magicAt = buffer.indexOf(Magic);
    if (magicAt < 0) {
        if (buffer.size() > Magic.size() - 1)
            buffer.remove(0, buffer.size() - (Magic.size() - 1));
        return false;
    }
    if (magicAt > 0)
        buffer.remove(0, magicAt);
    if (buffer.size() < HeaderSize)
        return false;

    if (static_cast<quint8>(buffer.at(4)) != 2) {
        buffer.remove(0, 1);
        if (error)
            *error = QStringLiteral("Unsupported transport version");
        return false;
    }

    int offset = 8;
    quint32 payloadSize = 0;
    quint32 expectedCrc = 0;
    if (!readU32(buffer, offset, payloadSize) || !readU32(buffer, offset, expectedCrc))
        return false;
    if (payloadSize > MaxPayloadSize) {
        buffer.remove(0, 1);
        if (error)
            *error = QStringLiteral("Transport payload is too large");
        return false;
    }
    if (buffer.size() < HeaderSize + static_cast<int>(payloadSize))
        return false;

    const QByteArray payload = buffer.mid(HeaderSize, payloadSize);
    if (crc32(payload) != expectedCrc) {
        buffer.remove(0, 1);
        if (error)
            *error = QStringLiteral("Transport CRC check failed");
        return false;
    }

    frame.type = static_cast<quint8>(buffer.at(5));
    frame.payload = payload;
    buffer.remove(0, HeaderSize + payloadSize);
    return true;
}

} // namespace Protocol
