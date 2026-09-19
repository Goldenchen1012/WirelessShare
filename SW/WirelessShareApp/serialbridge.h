#ifndef SERIALBRIDGE_H
#define SERIALBRIDGE_H

#include <QObject>
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

private slots:
    void readAvailable();
    void serialError(QSerialPort::SerialPortError error);

private:
    bool writeFrame(quint8 type, const QByteArray &payload);
    void processStatus(const QByteArray &payload);

    QSerialPort m_port;
    QByteArray m_receiveBuffer;
};

#endif // SERIALBRIDGE_H
