#ifndef SERIALBRIDGE_H
#define SERIALBRIDGE_H

#include <QObject>
#include <QQueue>
#include <QSerialPort>

class QTimer;

class SerialBridge : public QObject
{
    Q_OBJECT

public:
    explicit SerialBridge(QObject *parent = nullptr);

    bool open(const QString &portName, bool accessPointRole, const QString &password);
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
    void resetDataFlow();
    void processStatus(const QByteArray &payload);

    QSerialPort m_port;
    QTimer *m_configTimer;
    QTimer *m_dataRetryTimer;
    QByteArray m_receiveBuffer;
    QByteArray m_configPayload;
    QQueue<QByteArray> m_dataQueue;
    QByteArray m_inFlightData;
    quint32 m_inFlightSequence = 0;
    quint32 m_nextDataSequence = 1;
    int m_dataRetryCount = 0;
    quint8 m_expectedRole = 0;
    bool m_closing = false;
};

#endif // SERIALBRIDGE_H
