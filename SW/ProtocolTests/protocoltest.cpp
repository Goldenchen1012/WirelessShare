#include "protocol.h"

#include <QCoreApplication>
#include <QDebug>

namespace {
void require(bool condition, const char *message)
{
    if (!condition)
        qFatal("Protocol test failed: %s", message);
}
}

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    Q_UNUSED(application)

    require(Protocol::crc32(QByteArrayLiteral("123456789")) == 0xcbf43926U,
            "CRC-32 reference vector");
    require(Protocol::encodeFrame(Protocol::Data, QByteArrayLiteral("abc"))
            == QByteArray::fromHex("575348310203000003000000c2412435616263"),
            "known encoded frame bytes");
    require(Protocol::encodeFrame(Protocol::Data,
                                  QByteArray(Protocol::MaxPayloadSize + 1, 'x')).isEmpty(),
            "oversized payload rejected");

    const QByteArray encoded = Protocol::encodeFrame(Protocol::Data, QByteArrayLiteral("fragmented"));
    QByteArray fragmented;
    Protocol::Frame frame;
    for (int index = 0; index < encoded.size(); ++index) {
        fragmented.append(encoded.at(index));
        const bool complete = Protocol::takeFrame(fragmented, frame);
        require(complete == (index == encoded.size() - 1), "fragmented frame boundary");
    }
    require(frame.type == Protocol::Data && frame.payload == QByteArrayLiteral("fragmented"),
            "fragmented frame content");

    QByteArray ackPayload;
    Protocol::appendU32(ackPayload, 0x78563412U);
    QByteArray ackWire = Protocol::encodeFrame(Protocol::DataAck, ackPayload);
    require(Protocol::takeFrame(ackWire, frame) && frame.type == Protocol::DataAck,
            "device ACK frame type");
    int ackOffset = 0;
    quint32 ackSequence = 0;
    require(Protocol::readU32(frame.payload, ackOffset, ackSequence)
            && ackSequence == 0x78563412U && ackOffset == frame.payload.size(),
            "device ACK sequence");

    QByteArray combined = Protocol::encodeFrame(Protocol::Status, QByteArrayLiteral("one"))
            + Protocol::encodeFrame(Protocol::Data, QByteArrayLiteral("two"));
    require(Protocol::takeFrame(combined, frame) && frame.payload == QByteArrayLiteral("one"),
            "first combined frame");
    require(Protocol::takeFrame(combined, frame) && frame.payload == QByteArrayLiteral("two"),
            "second combined frame");
    require(combined.isEmpty(), "combined input consumed");

    QByteArray damaged = Protocol::encodeFrame(Protocol::Data, QByteArrayLiteral("bad"));
    damaged[Protocol::HeaderSize] = char(damaged.at(Protocol::HeaderSize) ^ 0x01);
    damaged += Protocol::encodeFrame(Protocol::Data, QByteArrayLiteral("recovered"));
    bool sawCrcError = false;
    bool recovered = false;
    for (int attempts = 0; attempts < 8 && !recovered; ++attempts) {
        QString error;
        if (Protocol::takeFrame(damaged, frame, &error))
            recovered = frame.payload == QByteArrayLiteral("recovered");
        else if (!error.isEmpty())
            sawCrcError = true;
    }
    require(sawCrcError, "damaged frame detected");
    require(recovered, "parser resynchronized after damaged frame");

    QByteArray stressWire;
    for (quint32 sequence = 0; sequence < 5000; ++sequence) {
        QByteArray payload;
        Protocol::appendU32(payload, sequence);
        payload.append(int((sequence * 37U) % 2048U), char(sequence & 0xffU));
        stressWire += Protocol::encodeFrame(Protocol::Data, payload);
    }
    QByteArray stressInput;
    quint32 expectedSequence = 0;
    int wireOffset = 0;
    while (wireOffset < stressWire.size()) {
        const int chunkSize = qMin(1 + ((wireOffset * 17) % 3072), stressWire.size() - wireOffset);
        stressInput.append(stressWire.mid(wireOffset, chunkSize));
        wireOffset += chunkSize;
        while (Protocol::takeFrame(stressInput, frame)) {
            int payloadOffset = 0;
            quint32 sequence = 0;
            require(Protocol::readU32(frame.payload, payloadOffset, sequence), "stress sequence field");
            require(sequence == expectedSequence++, "stress frame ordering");
        }
    }
    require(expectedSequence == 5000 && stressInput.isEmpty(), "5000-frame stress stream");

    qInfo() << "All protocol tests passed";
    return 0;
}
