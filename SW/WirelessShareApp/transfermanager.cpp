#include "transfermanager.h"

#include "protocol.h"
#include "serialbridge.h"

#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QImage>
#include <QMimeData>
#include <QRandomGenerator>
#include <QSet>
#include <QTimer>
#include <QUrl>

namespace {
constexpr quint64 MaxFileSize = 1024ULL * 1024ULL * 1024ULL;
constexpr quint64 MaxClipboardSize = 64ULL * 1024ULL * 1024ULL;
constexpr int DataChunkSize = 2048;
constexpr qint64 SerialHighWaterMark = 32 * 1024;

enum RecordType : quint8 {
    TransferBeginRecord = 1,
    ItemRecord = 2,
    ChunkRecord = 3,
    TransferEndRecord = 4,
    AcknowledgementRecord = 5
};

QString uniqueName(const QString &wanted, QSet<QString> &used)
{
    if (!used.contains(wanted)) {
        used.insert(wanted);
        return wanted;
    }
    const QFileInfo info(wanted);
    const QString suffix = info.completeSuffix().isEmpty() ? QString() : QStringLiteral(".") + info.completeSuffix();
    const QString base = suffix.isEmpty() ? wanted : wanted.left(wanted.size() - suffix.size());
    for (int number = 2; ; ++number) {
        const QString candidate = QStringLiteral("%1 (%2)%3").arg(base).arg(number).arg(suffix);
        if (!used.contains(candidate)) {
            used.insert(candidate);
            return candidate;
        }
    }
}
}

TransferManager::SendState::SendState(ContentKind contentKind)
    : kind(contentKind), hash(QCryptographicHash::Sha256)
{
}

TransferManager::ReceiveState::ReceiveState(ContentKind contentKind)
    : kind(contentKind), hash(QCryptographicHash::Sha256)
{
}

TransferManager::TransferManager(SerialBridge *bridge, QObject *parent)
    : QObject(parent), m_bridge(bridge), m_sendTimer(new QTimer(this))
{
    m_sendTimer->setInterval(5);
    connect(m_sendTimer, &QTimer::timeout, this, &TransferManager::pumpSend);
    connect(m_bridge, &SerialBridge::bytesWritten, this, &TransferManager::pumpSend);
    connect(m_bridge, &SerialBridge::dataReceived, this, &TransferManager::receiveRecord);
    connect(QApplication::clipboard(), &QClipboard::dataChanged, this, &TransferManager::clipboardChanged);
    m_sendTimer->start();
}

TransferManager::~TransferManager()
{
    qDeleteAll(m_sendQueue);
    if (m_receive) {
        if (m_receive->file.isOpen())
            m_receive->file.close();
        if (!m_receive->stagingRoot.isEmpty())
            QDir(m_receive->stagingRoot).removeRecursively();
        delete m_receive;
    }
}

void TransferManager::setReceiveDirectory(const QString &path)
{
    m_receiveDirectory = QDir::cleanPath(path);
    QDir().mkpath(m_receiveDirectory);
}

void TransferManager::setPeerConnected(bool connected)
{
    if (m_peerConnected == connected)
        return;
    m_peerConnected = connected;
    if (!connected) {
        if (!m_sendQueue.isEmpty())
            resetSend(m_sendQueue.first());
        abortReceive(tr("連線中斷，未完成的接收已取消"));
    }
}

quint64 TransferManager::newTransferId()
{
    return (quint64(QRandomGenerator::global()->generate()) << 32)
            | QRandomGenerator::global()->generate();
}

void TransferManager::clipboardChanged()
{
    if (m_suppressNextClipboard) {
        m_suppressNextClipboard = false;
        return;
    }

    SendState *state = makeTransfer(QApplication::clipboard()->mimeData());
    if (!state)
        return;
    m_sendQueue.append(state);
    emit activity(tr("已排入傳送：%1").arg(state->label));
    pumpSend();
}

TransferManager::SendState *TransferManager::makeTransfer(const QMimeData *mimeData)
{
    if (mimeData->hasUrls()) {
        QList<QUrl> localUrls;
        for (const QUrl &url : mimeData->urls()) {
            if (url.isLocalFile())
                localUrls.append(url);
        }
        if (!localUrls.isEmpty())
            return makeFileTransfer(localUrls);
    }

    if (mimeData->hasImage()) {
        QImage image = qvariant_cast<QImage>(mimeData->imageData());
        if (image.isNull())
            return nullptr;
        QByteArray png;
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        if (!image.save(&buffer, "PNG") || png.size() > static_cast<qint64>(MaxClipboardSize))
            return nullptr;
        SendState *state = new SendState(Image);
        state->id = newTransferId();
        state->label = tr("圖片 (%1 KiB)").arg((png.size() + 1023) / 1024);
        SendItem item;
        item.relativePath = QStringLiteral("clipboard.png");
        item.size = png.size();
        item.memoryData = png;
        state->items.append(item);
        return state;
    }

    if (mimeData->hasText()) {
        const QByteArray text = mimeData->text().toUtf8();
        if (text.isEmpty() || text.size() > static_cast<qint64>(MaxClipboardSize))
            return nullptr;
        SendState *state = new SendState(Text);
        state->id = newTransferId();
        state->label = tr("文字 (%1 bytes)").arg(text.size());
        SendItem item;
        item.relativePath = QStringLiteral("clipboard.txt");
        item.size = text.size();
        item.memoryData = text;
        state->items.append(item);
        return state;
    }
    return nullptr;
}

TransferManager::SendState *TransferManager::makeFileTransfer(const QList<QUrl> &urls)
{
    SendState *state = new SendState(Files);
    state->id = newTransferId();
    QSet<QString> rootNames;
    for (const QUrl &url : urls) {
        const QFileInfo info(url.toLocalFile());
        if (!info.exists() || info.isSymLink())
            continue;
        const QString rootName = uniqueName(info.fileName(), rootNames);
        if (!addFileTree(state, info.absoluteFilePath(), rootName)) {
            emit activity(tr("檔案超過 1 GiB 或無法讀取：%1").arg(info.absoluteFilePath()));
            delete state;
            return nullptr;
        }
    }
    if (state->items.isEmpty()) {
        delete state;
        return nullptr;
    }
    state->label = tr("%1 個檔案/資料夾項目").arg(state->items.size());
    return state;
}

bool TransferManager::addFileTree(SendState *state, const QString &rootPath, const QString &rootName)
{
    const QFileInfo rootInfo(rootPath);
    if (rootInfo.isFile()) {
        if (rootInfo.size() < 0 || quint64(rootInfo.size()) > MaxFileSize || !rootInfo.isReadable())
            return false;
        SendItem item;
        item.sourcePath = rootInfo.absoluteFilePath();
        item.relativePath = rootName;
        item.size = rootInfo.size();
        state->items.append(item);
        return true;
    }

    SendItem root;
    root.sourcePath = rootInfo.absoluteFilePath();
    root.relativePath = rootName;
    root.directory = true;
    state->items.append(root);

    QDir base(rootInfo.absoluteFilePath());
    QDirIterator it(rootInfo.absoluteFilePath(), QDir::AllEntries | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QFileInfo info = it.fileInfo();
        if (info.isSymLink())
            continue;
        SendItem item;
        item.sourcePath = info.absoluteFilePath();
        item.relativePath = rootName + QStringLiteral("/") + base.relativeFilePath(info.absoluteFilePath());
        item.relativePath = QDir::fromNativeSeparators(item.relativePath);
        item.directory = info.isDir();
        if (!item.directory) {
            if (!info.isFile() || !info.isReadable() || info.size() < 0 || quint64(info.size()) > MaxFileSize)
                return false;
            item.size = info.size();
        }
        state->items.append(item);
    }
    return true;
}

void TransferManager::resetSend(SendState *state)
{
    state->file.close();
    state->phase = SendBegin;
    state->itemIndex = -1;
    state->offset = 0;
    state->hash.reset();
    state->ackTimer.invalidate();
}

bool TransferManager::sendRecord(const QByteArray &record)
{
    return record.size() <= static_cast<int>(Protocol::MaxPayloadSize) && m_bridge->sendData(record);
}

void TransferManager::pumpSend()
{
    if (!m_peerConnected || !m_bridge->isOpen() || m_sendQueue.isEmpty())
        return;

    SendState *state = m_sendQueue.first();
    if (state->phase == SendWaitAck) {
        if (state->ackTimer.isValid() && state->ackTimer.elapsed() > 10000) {
            if (++state->retries > 3) {
                emit activity(tr("對方未確認，傳送失敗：%1").arg(state->label));
                finishSend();
            } else {
                emit activity(tr("未收到確認，重新傳送 (%1/3)：%2").arg(state->retries).arg(state->label));
                resetSend(state);
            }
        }
        return;
    }
    for (int sent = 0; sent < 8 && m_bridge->bytesToWrite() < SerialHighWaterMark; ++sent) {
        QByteArray record;
        if (state->phase == SendBegin) {
            record.append(char(TransferBeginRecord));
            Protocol::appendU64(record, state->id);
            record.append(char(state->kind));
            Protocol::appendU32(record, state->items.size());
            if (!sendRecord(record))
                return;
            state->phase = SendNextItem;
        } else if (state->phase == SendNextItem) {
            ++state->itemIndex;
            if (state->itemIndex >= state->items.size()) {
                state->phase = SendEnd;
                continue;
            }
            const SendItem &item = state->items.at(state->itemIndex);
            const QByteArray path = item.relativePath.toUtf8();
            if (path.size() > 1024) {
                emit activity(tr("路徑過長，取消傳送：%1").arg(item.relativePath));
                finishSend();
                return;
            }
            record.append(char(ItemRecord));
            Protocol::appendU64(record, state->id);
            Protocol::appendU32(record, state->itemIndex);
            record.append(item.directory ? char(1) : char(0));
            Protocol::appendU64(record, item.size);
            Protocol::appendU16(record, path.size());
            record.append(path);
            if (!sendRecord(record)) {
                --state->itemIndex;
                return;
            }
            state->offset = 0;
            if (item.directory || item.size == 0) {
                state->phase = SendNextItem;
            } else {
                if (!item.sourcePath.isEmpty()) {
                    state->file.setFileName(item.sourcePath);
                    if (!state->file.open(QIODevice::ReadOnly)) {
                        emit activity(tr("無法讀取，取消傳送：%1").arg(item.sourcePath));
                        finishSend();
                        return;
                    }
                }
                state->phase = SendContent;
            }
        } else if (state->phase == SendContent) {
            const SendItem &item = state->items.at(state->itemIndex);
            QByteArray chunk;
            if (item.sourcePath.isEmpty())
                chunk = item.memoryData.mid(state->offset, DataChunkSize);
            else
                chunk = state->file.read(DataChunkSize);
            if (chunk.isEmpty()) {
                emit activity(tr("讀取中斷，取消傳送：%1").arg(item.relativePath));
                finishSend();
                return;
            }
            record.append(char(ChunkRecord));
            Protocol::appendU64(record, state->id);
            Protocol::appendU32(record, state->itemIndex);
            Protocol::appendU64(record, state->offset);
            record.append(chunk);
            if (!sendRecord(record)) {
                if (!item.sourcePath.isEmpty())
                    state->file.seek(state->offset);
                return;
            }
            state->hash.addData(chunk);
            state->offset += chunk.size();
            if (state->offset == item.size) {
                state->file.close();
                state->phase = SendNextItem;
            } else if (state->offset > item.size) {
                emit activity(tr("檔案大小於傳送中改變，已取消：%1").arg(item.relativePath));
                finishSend();
                return;
            }
        } else if (state->phase == SendEnd) {
            record.append(char(TransferEndRecord));
            Protocol::appendU64(record, state->id);
            record.append(state->hash.result());
            if (!sendRecord(record))
                return;
            state->phase = SendWaitAck;
            state->ackTimer.start();
            emit activity(tr("資料已送出，等待對方驗證：%1").arg(state->label));
            return;
        }
    }
}

void TransferManager::finishSend()
{
    if (m_sendQueue.isEmpty())
        return;
    SendState *state = m_sendQueue.takeFirst();
    state->file.close();
    delete state;
}

void TransferManager::receiveRecord(const QByteArray &record)
{
    if (record.isEmpty())
        return;
    if (static_cast<quint8>(record.at(0)) == AcknowledgementRecord) {
        processAck(record);
        return;
    }
    bool ok = false;
    switch (static_cast<quint8>(record.at(0))) {
    case TransferBeginRecord: ok = processBegin(record); break;
    case ItemRecord: ok = processItem(record); break;
    case ChunkRecord: ok = processChunk(record); break;
    case TransferEndRecord: ok = processEnd(record); break;
    default: return;
    }
    if (!ok) {
        if (m_receive)
            sendAck(m_receive->id, false);
        abortReceive(tr("收到無效或順序錯誤的資料，已取消接收"));
    }
}

bool TransferManager::processBegin(const QByteArray &record)
{
    int offset = 1;
    quint64 id = 0;
    quint32 itemCount = 0;
    if (!Protocol::readU64(record, offset, id) || record.size() - offset < 1)
        return false;
    const quint8 kindValue = static_cast<quint8>(record.at(offset++));
    if (!Protocol::readU32(record, offset, itemCount) || offset != record.size()
            || kindValue < Text || kindValue > Files || itemCount == 0 || itemCount > 100000
            || (kindValue != Files && itemCount != 1))
        return false;

    abortReceive(QString());
    m_receive = new ReceiveState(static_cast<ContentKind>(kindValue));
    m_receive->id = id;
    m_receive->expectedItems = itemCount;
    if (m_receive->kind == Files) {
        m_receive->stagingRoot = QDir(m_receiveDirectory).filePath(
                    QStringLiteral(".WirelessShare-%1.part").arg(id, 16, 16, QLatin1Char('0')));
        if (!QDir().mkpath(m_receive->stagingRoot))
            return false;
    }
    emit activity(tr("開始接收 %1 個項目").arg(itemCount));
    return true;
}

QString TransferManager::safeDestination(const QString &relativePath) const
{
    if (!m_receive || m_receive->kind != Files)
        return QString();
    const QString normalized = QDir::fromNativeSeparators(relativePath);
    if (normalized.isEmpty() || QDir::isAbsolutePath(normalized) || normalized.contains(QLatin1Char(':')))
        return QString();
    const QStringList parts = normalized.split(QLatin1Char('/'), QString::SkipEmptyParts);
    if (parts.isEmpty())
        return QString();
    for (const QString &part : parts) {
        if (part == QStringLiteral(".") || part == QStringLiteral(".."))
            return QString();
    }
    return QDir(m_receive->stagingRoot).filePath(parts.join(QLatin1Char('/')));
}

bool TransferManager::processItem(const QByteArray &record)
{
    if (!m_receive)
        return false;
    if (m_receive->nextItem > 0 && m_receive->currentOffset != m_receive->currentSize)
        return false;
    int offset = 1;
    quint64 id = 0;
    quint32 index = 0;
    quint64 size = 0;
    quint16 pathLength = 0;
    if (!Protocol::readU64(record, offset, id) || !Protocol::readU32(record, offset, index)
            || record.size() - offset < 1)
        return false;
    const bool directory = record.at(offset++) != 0;
    if (!Protocol::readU64(record, offset, size) || !Protocol::readU16(record, offset, pathLength)
            || record.size() - offset != pathLength || id != m_receive->id
            || index != m_receive->nextItem || index >= m_receive->expectedItems
            || size > (m_receive->kind == Files ? MaxFileSize : MaxClipboardSize)
            || (directory && size != 0))
        return false;

    const QString relativePath = QString::fromUtf8(record.constData() + offset, pathLength);
    m_receive->currentItem = index;
    m_receive->currentSize = size;
    m_receive->currentOffset = 0;
    ++m_receive->nextItem;

    if (m_receive->kind != Files)
        return index == 0 && !directory;

    const QString destination = safeDestination(relativePath);
    if (destination.isEmpty())
        return false;
    const QString rootName = QDir::fromNativeSeparators(relativePath).section(QLatin1Char('/'), 0, 0);
    if (!m_receive->rootNames.contains(rootName))
        m_receive->rootNames.append(rootName);
    if (directory)
        return QDir().mkpath(destination);
    if (!QDir().mkpath(QFileInfo(destination).absolutePath()))
        return false;
    m_receive->currentFinalPath = destination;
    m_receive->file.setFileName(destination + QStringLiteral(".part"));
    if (!m_receive->file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    if (size == 0) {
        m_receive->file.close();
        return QFile::rename(m_receive->file.fileName(), destination);
    }
    return true;
}

bool TransferManager::processChunk(const QByteArray &record)
{
    if (!m_receive)
        return false;
    int offset = 1;
    quint64 id = 0;
    quint32 index = 0;
    quint64 fileOffset = 0;
    if (!Protocol::readU64(record, offset, id) || !Protocol::readU32(record, offset, index)
            || !Protocol::readU64(record, offset, fileOffset) || id != m_receive->id
            || index != m_receive->currentItem || fileOffset != m_receive->currentOffset)
        return false;
    const QByteArray chunk = record.mid(offset);
    if (chunk.isEmpty() || m_receive->currentOffset + quint64(chunk.size()) > m_receive->currentSize)
        return false;

    if (m_receive->kind == Files) {
        if (!m_receive->file.isOpen() || m_receive->file.write(chunk) != chunk.size())
            return false;
    } else {
        m_receive->memoryData.append(chunk);
    }
    m_receive->hash.addData(chunk);
    m_receive->currentOffset += chunk.size();
    if (m_receive->currentOffset == m_receive->currentSize && m_receive->kind == Files) {
        const QString partPath = m_receive->file.fileName();
        m_receive->file.close();
        if (!QFile::rename(partPath, m_receive->currentFinalPath))
            return false;
    }
    return true;
}

QString TransferManager::uniqueDestination(const QString &name) const
{
    const QString wanted = QDir(m_receiveDirectory).filePath(name);
    if (!QFileInfo::exists(wanted))
        return wanted;
    const QFileInfo info(name);
    const QString suffix = info.completeSuffix().isEmpty() ? QString() : QStringLiteral(".") + info.completeSuffix();
    const QString base = suffix.isEmpty() ? name : name.left(name.size() - suffix.size());
    for (int number = 2; ; ++number) {
        const QString candidate = QDir(m_receiveDirectory).filePath(
                    QStringLiteral("%1 (%2)%3").arg(base).arg(number).arg(suffix));
        if (!QFileInfo::exists(candidate))
            return candidate;
    }
}

bool TransferManager::processEnd(const QByteArray &record)
{
    if (!m_receive || record.size() != 1 + 8 + 32)
        return false;
    int offset = 1;
    quint64 id = 0;
    if (!Protocol::readU64(record, offset, id) || id != m_receive->id
            || m_receive->nextItem != m_receive->expectedItems
            || m_receive->currentOffset != m_receive->currentSize
            || record.mid(offset) != m_receive->hash.result())
        return false;

    if (m_receive->kind == Text) {
        m_suppressNextClipboard = true;
        QApplication::clipboard()->setText(QString::fromUtf8(m_receive->memoryData));
    } else if (m_receive->kind == Image) {
        const QImage image = QImage::fromData(m_receive->memoryData, "PNG");
        if (image.isNull())
            return false;
        m_suppressNextClipboard = true;
        QApplication::clipboard()->setImage(image);
    } else {
        QList<QUrl> urls;
        for (const QString &rootName : m_receive->rootNames) {
            const QString source = QDir(m_receive->stagingRoot).filePath(rootName);
            const QString destination = uniqueDestination(rootName);
            if (!QDir().rename(source, destination))
                return false;
            urls.append(QUrl::fromLocalFile(destination));
        }
        QDir(m_receive->stagingRoot).removeRecursively();
        QMimeData *mimeData = new QMimeData;
        mimeData->setUrls(urls);
        m_suppressNextClipboard = true;
        QApplication::clipboard()->setMimeData(mimeData);
    }

    const quint64 completedId = m_receive->id;
    emit activity(tr("接收完成，共 %1 個項目").arg(m_receive->expectedItems));
    delete m_receive;
    m_receive = nullptr;
    sendAck(completedId, true);
    return true;
}

void TransferManager::sendAck(quint64 id, bool success)
{
    QByteArray record;
    record.append(char(AcknowledgementRecord));
    Protocol::appendU64(record, id);
    record.append(success ? char(1) : char(0));
    sendRecord(record);
}

void TransferManager::processAck(const QByteArray &record)
{
    if (record.size() != 10 || m_sendQueue.isEmpty())
        return;
    int offset = 1;
    quint64 id = 0;
    if (!Protocol::readU64(record, offset, id))
        return;
    SendState *state = m_sendQueue.first();
    if (state->id != id || state->phase != SendWaitAck)
        return;
    if (record.at(offset) != 0) {
        emit activity(tr("對方已驗證，傳送完成：%1").arg(state->label));
        finishSend();
        pumpSend();
    } else if (++state->retries <= 3) {
        emit activity(tr("對方驗證失敗，重新傳送 (%1/3)：%2").arg(state->retries).arg(state->label));
        resetSend(state);
    } else {
        emit activity(tr("對方驗證失敗，傳送已取消：%1").arg(state->label));
        finishSend();
    }
}

void TransferManager::abortReceive(const QString &reason)
{
    if (!m_receive)
        return;
    m_receive->file.close();
    if (!m_receive->stagingRoot.isEmpty())
        QDir(m_receive->stagingRoot).removeRecursively();
    delete m_receive;
    m_receive = nullptr;
    if (!reason.isEmpty())
        emit activity(reason);
}
