#include "serialbridge.h"

#include "protocol.h"

SerialBridge::SerialBridge(QObject *parent)
    : QObject(parent)
{
    connect(&m_port, &QSerialPort::readyRead, this, &SerialBridge::readAvailable);
    connect(&m_port, &QSerialPort::bytesWritten, this, &SerialBridge::bytesWritten);
    connect(&m_port, QOverload<QSerialPort::SerialPortError>::of(&QSerialPort::errorOccurred),
            this, &SerialBridge::serialError);
}

bool SerialBridge::open(const QString &portName, bool accessPointRole, const QString &password)
{
    close();
    m_port.setPortName(portName);
    m_port.setBaudRate(921600);
    m_port.setDataBits(QSerialPort::Data8);
    m_port.setParity(QSerialPort::NoParity);
    m_port.setStopBits(QSerialPort::OneStop);
    m_port.setFlowControl(QSerialPort::NoFlowControl);
    if (!m_port.open(QIODevice::ReadWrite)) {
        emit errorOccurred(tr("無法開啟 %1：%2").arg(portName, m_port.errorString()));
        return false;
    }
    m_port.setDataTerminalReady(true);

    QByteArray config;
    config.append(accessPointRole ? char(1) : char(2));
    const QByteArray passwordUtf8 = password.toUtf8();
    config.append(char(passwordUtf8.size()));
    config.append(passwordUtf8);
    if (!writeFrame(Protocol::Configure, config)) {
        close();
        emit errorOccurred(tr("無法傳送裝置設定"));
        return false;
    }
    requestStatus();
    return true;
}

void SerialBridge::close()
{
    if (m_port.isOpen())
        m_port.close();
    m_receiveBuffer.clear();
}

bool SerialBridge::isOpen() const
{
    return m_port.isOpen();
}

QString SerialBridge::portName() const
{
    return m_port.portName();
}

qint64 SerialBridge::bytesToWrite() const
{
    return m_port.bytesToWrite();
}

bool SerialBridge::sendData(const QByteArray &payload)
{
    return writeFrame(Protocol::Data, payload);
}

void SerialBridge::requestStatus()
{
    writeFrame(Protocol::GetStatus, QByteArray());
}

bool SerialBridge::writeFrame(quint8 type, const QByteArray &payload)
{
    if (!m_port.isOpen())
        return false;
    const QByteArray frame = Protocol::encodeFrame(type, payload);
    return !frame.isEmpty() && m_port.write(frame) == frame.size();
}

void SerialBridge::readAvailable()
{
    m_receiveBuffer.append(m_port.readAll());
    while (true) {
        Protocol::Frame frame;
        QString error;
        if (!Protocol::takeFrame(m_receiveBuffer, frame, &error)) {
            if (!error.isEmpty()) {
                emit errorOccurred(error);
                continue;
            }
            break;
        }
        if (frame.type == Protocol::Data)
            emit dataReceived(frame.payload);
        else if (frame.type == Protocol::Status)
            processStatus(frame.payload);
    }
}

void SerialBridge::processStatus(const QByteArray &payload)
{
    if (payload.size() < 3)
        return;
    const quint8 state = static_cast<quint8>(payload.at(0));
    const quint8 role = static_cast<quint8>(payload.at(1));
    const qint8 rssi = static_cast<qint8>(payload.at(2));
    const QString detail = QString::fromUtf8(payload.mid(3));
    QString text = tr("裝置 %1：%2").arg(role == 1 ? QStringLiteral("A/AP") : QStringLiteral("B/Station"), detail);
    if (state == 3 && role == 2)
        text += tr("，RSSI %1 dBm").arg(rssi);
    emit statusChanged(text, state == 3);
}

void SerialBridge::serialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError || error == QSerialPort::NotOpenError)
        return;
    const QString message = m_port.errorString();
    if (error == QSerialPort::ResourceError || error == QSerialPort::DeviceNotFoundError
            || error == QSerialPort::PermissionError)
        close();
    emit errorOccurred(message);
}
