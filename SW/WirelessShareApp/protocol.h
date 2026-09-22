#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <QByteArray>
#include <QString>
#include <QtGlobal>

namespace Protocol {

constexpr int HeaderSize = 16;
constexpr quint32 MaxPayloadSize = 4096;

enum FrameType : quint8 {
    Configure = 1,
    GetStatus = 2,
    Data = 3,
    Status = 4,
    DataAck = 5,
    DataNack = 6,
    DirectHello = 7,
    DirectHelloAck = 8
};

struct Frame
{
    quint8 type = 0;
    QByteArray payload;
};

QByteArray encodeFrame(quint8 type, const QByteArray &payload);
bool takeFrame(QByteArray &buffer, Frame &frame, QString *error = nullptr);
quint32 crc32(const QByteArray &data);

void appendU16(QByteArray &data, quint16 value);
void appendU32(QByteArray &data, quint32 value);
void appendU64(QByteArray &data, quint64 value);
bool readU16(const QByteArray &data, int &offset, quint16 &value);
bool readU32(const QByteArray &data, int &offset, quint32 &value);
bool readU64(const QByteArray &data, int &offset, quint64 &value);

} // namespace Protocol

#endif // PROTOCOL_H
