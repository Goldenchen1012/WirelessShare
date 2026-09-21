#include "serialbridge.h"

#include "protocol.h"

#include <QTimer>

SerialBridge::SerialBridge(QObject *parent)
    : QObject(parent), m_configTimer(new QTimer(this)), m_dataRetryTimer(new QTimer(this))
{
    connect(&m_port, &QSerialPort::readyRead, this, &SerialBridge::readAvailable);
    connect(&m_port, &QSerialPort::bytesWritten, this, &SerialBridge::bytesWritten);
    connect(&m_port, QOverload<QSerialPort::SerialPortError>::of(&QSerialPort::errorOccurred),
            this, &SerialBridge::serialError);
    m_configTimer->setInterval(1000);
    connect(m_configTimer, &QTimer::timeout, this, &SerialBridge::sendConfiguration);
    m_dataRetryTimer->setSingleShot(true);
    connect(m_dataRetryTimer, &QTimer::timeout, this, &SerialBridge::retryDataFrame);
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

    m_expectedRole = accessPointRole ? 1 : 2;
    m_configPayload.clear();
    m_configPayload.append(char(m_expectedRole));
    const QByteArray passwordUtf8 = password.toUtf8();
    m_configPayload.append(char(passwordUtf8.size()));
    m_configPayload.append(passwordUtf8);
    if (!writeFrame(Protocol::Configure, m_configPayload)) {
        close();
        emit errorOccurred(tr("無法傳送裝置設定"));
        return false;
    }
    m_configTimer->start();
    requestStatus();
    return true;
}

void SerialBridge::close()
{
    m_configTimer->stop();
    m_closing = true;
    if (m_port.isOpen()) {
        m_port.setDataTerminalReady(false);
        m_port.close();
    }
    m_closing = false;
    m_receiveBuffer.clear();
    resetDataFlow();
    m_nextDataSequence = 1;
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
    if (!m_inFlightData.isEmpty() || !m_dataQueue.isEmpty())
        return m_port.bytesToWrite() + Protocol::MaxPayloadSize;
    return m_port.bytesToWrite();
}

bool SerialBridge::sendData(const QByteArray &payload)
{
    if (!m_port.isOpen() || payload.isEmpty()
            || payload.size() + 4 > static_cast<int>(Protocol::MaxPayloadSize))
        return false;
    m_dataQueue.enqueue(payload);
    pumpDataQueue();
    return true;
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

bool SerialBridge::writeInFlightData()
{
    QByteArray payload;
    Protocol::appendU32(payload, m_inFlightSequence);
    payload.append(m_inFlightData);
    return writeFrame(Protocol::Data, payload);
}

void SerialBridge::pumpDataQueue()
{
    if (!m_port.isOpen() || !m_inFlightData.isEmpty() || m_dataQueue.isEmpty())
        return;
    m_inFlightData = m_dataQueue.dequeue();
    m_inFlightSequence = m_nextDataSequence++;
    m_dataRetryCount = 0;
    writeInFlightData();
    m_dataRetryTimer->start(1000);
}

void SerialBridge::retryDataFrame()
{
    if (!m_port.isOpen() || m_inFlightData.isEmpty())
        return;
    if (++m_dataRetryCount > 5) {
        resetDataFlow();
        emit errorOccurred(tr("本機裝置未確認資料幀，已中止本次傳輸"));
        emit dataFlowFailed();
        return;
    }
    writeInFlightData();
    m_dataRetryTimer->start(1000);
}

void SerialBridge::processDataResult(const QByteArray &payload, bool accepted)
{
    if (!accepted && payload.isEmpty() && !m_inFlightData.isEmpty()) {
        m_dataRetryTimer->start(250);
        return;
    }
    int offset = 0;
    quint32 sequence = 0;
    if (payload.size() != 4 || !Protocol::readU32(payload, offset, sequence)
            || m_inFlightData.isEmpty() || sequence != m_inFlightSequence)
        return;
    if (!accepted) {
        m_dataRetryTimer->start(250);
        return;
    }
    m_dataRetryTimer->stop();
    m_inFlightData.clear();
    m_dataRetryCount = 0;
    pumpDataQueue();
    emit dataFrameAcknowledged();
}

void SerialBridge::resetDataFlow()
{
    m_dataRetryTimer->stop();
    m_dataQueue.clear();
    m_inFlightData.clear();
    m_dataRetryCount = 0;
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
        else if (frame.type == Protocol::DataAck)
            processDataResult(frame.payload, true);
        else if (frame.type == Protocol::DataNack)
            processDataResult(frame.payload, false);
    }
}

void SerialBridge::processStatus(const QByteArray &payload)
{
    if (payload.size() < 3)
        return;
    const quint8 state = static_cast<quint8>(payload.at(0));
    const quint8 role = static_cast<quint8>(payload.at(1));
    const qint8 rssi = static_cast<qint8>(payload.at(2));
    const QString firmwareDetail = QString::fromUtf8(payload.mid(3));
    const int diagnosticStart = firmwareDetail.indexOf(QLatin1Char(';'));
    const QString diagnostics = diagnosticStart >= 0 ? firmwareDetail.mid(diagnosticStart) : QString();
    if (role == m_expectedRole && state != 0)
        m_configTimer->stop();
    if (state != 3)
        resetDataFlow();
    const QString roleText = role == 1 ? QStringLiteral("A/AP")
                                       : role == 2 ? QStringLiteral("B/Station") : tr("未設定");
    QString detail = firmwareDetail;
    if (state == 0)
        detail = tr("尚未設定，正在重送設定");
    else if (state == 1)
        detail = tr("Wi-Fi 已建立，等待 B 裝置連線");
    else if (state == 2)
        detail = tr("正在連接 A 裝置");
    else if (state == 3)
        detail = tr("已與對方連線");
    else if (state == 4)
        detail = tr("Wi-Fi 已連接，正在建立資料通道");
    detail += diagnostics;
    QString text = tr("裝置 %1：%2").arg(roleText, detail);
    if (state == 3 && role == 2)
        text += tr("，RSSI %1 dBm").arg(rssi);
    emit statusChanged(text, state == 3);
}

void SerialBridge::sendConfiguration()
{
    if (m_port.isOpen() && !m_configPayload.isEmpty()) {
        writeFrame(Protocol::Configure, m_configPayload);
        requestStatus();
    }
}

void SerialBridge::serialError(QSerialPort::SerialPortError error)
{
    if (m_closing || error == QSerialPort::NoError || error == QSerialPort::NotOpenError)
        return;
    const QString message = m_port.errorString();
    if (error == QSerialPort::ResourceError || error == QSerialPort::DeviceNotFoundError
            || error == QSerialPort::PermissionError)
        close();
    emit errorOccurred(message);
}
