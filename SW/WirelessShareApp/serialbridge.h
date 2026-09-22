#ifndef SERIALBRIDGE_H
#define SERIALBRIDGE_H

#include <QObject>
#include <QElapsedTimer>
#include <QQueue>
#include <QSerialPort>

class QTimer;

class SerialBridge : public QObject
{
    Q_OBJECT

public:
    explicit SerialBridge(QObject *parent = nullptr);

    bool open(const QString &portName, bool accessPointRole, const QString &password,
              bool directSerial = false);
    void close();
    bool isOpen() const;
    QString portName() const;
    qint64 bytesToWrite() const;
    bool sendData(const QByteArray &payload);
    void requestStatus();

signals:
    void dataReceived(const QByteArray &payload);
    void statusChanged(const QString &text, bool peerConnected);
    void errorOccurred(const QString &text);
    void bytesWritten(qint64 bytes);
    void dataFrameAcknowledged();
    void dataFlowFailed();

private slots:
    void readAvailable();
    void serialError(QSerialPort::SerialPortError error);
    void sendConfiguration();
    void retryDataFrame();

private:
    bool writeFrame(quint8 type, const QByteArray &payload);
    bool writeInFlightData();
    void pumpDataQueue();
    void processDataResult(const QByteArray &payload, bool accepted);
    void processDirectData(const QByteArray &payload);
    void processDirectHello(const QByteArray &payload, bool acknowledgement);
    void updateDirectStatus(bool connected, const QString &detail);
    void resetDataFlow();
    void processStatus(const QByteArray &payload);

    QSerialPort m_port;
    QTimer *m_configTimer;
    QTimer *m_dataRetryTimer;
    QByteArray m_receiveBuffer;
    QByteArray m_configPayload;
    QByteArray m_directHelloPayload;
    QByteArray m_directPeerSession;
    QQueue<QByteArray> m_dataQueue;
    QByteArray m_inFlightData;
    quint32 m_inFlightSequence = 0;
    quint32 m_nextDataSequence = 1;
    int m_dataRetryCount = 0;
    quint32 m_lastDirectReceiveSequence = 0;
    bool m_hasLastDirectReceiveSequence = false;
    bool m_directSerial = false;
    bool m_directPeerConnected = false;
    QElapsedTimer m_directPeerTimer;
    quint8 m_expectedRole = 0;
    bool m_closing = false;
};

#endif // SERIALBRIDGE_H
