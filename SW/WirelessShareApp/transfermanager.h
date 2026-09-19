#ifndef TRANSFERMANAGER_H
#define TRANSFERMANAGER_H

#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QFile>
#include <QList>
#include <QObject>
#include <QStringList>

class QMimeData;
class QTimer;
class SerialBridge;

class TransferManager : public QObject
{
    Q_OBJECT

public:
    explicit TransferManager(SerialBridge *bridge, QObject *parent = nullptr);
    ~TransferManager();

    void setReceiveDirectory(const QString &path);
    void setPeerConnected(bool connected);

signals:
    void activity(const QString &text);

private slots:
    void clipboardChanged();
    void pumpSend();
    void receiveRecord(const QByteArray &record);

private:
    enum ContentKind : quint8 { Text = 1, Image = 2, Files = 3 };
    enum SendPhase { SendBegin, SendNextItem, SendContent, SendEnd, SendWaitAck };

    struct SendItem {
        QString sourcePath;
        QString relativePath;
        bool directory = false;
        quint64 size = 0;
        QByteArray memoryData;
    };

    struct SendState {
        explicit SendState(ContentKind contentKind);
        quint64 id = 0;
        ContentKind kind;
        QList<SendItem> items;
        SendPhase phase = SendBegin;
        int itemIndex = -1;
        quint64 offset = 0;
        QFile file;
        QCryptographicHash hash;
        QElapsedTimer ackTimer;
        int retries = 0;
        QString label;
    };

    struct ReceiveState {
        explicit ReceiveState(ContentKind contentKind);
        quint64 id = 0;
        ContentKind kind;
        quint32 expectedItems = 0;
        quint32 nextItem = 0;
        quint32 currentItem = 0;
        quint64 currentSize = 0;
        quint64 currentOffset = 0;
        QFile file;
        QString currentFinalPath;
        QByteArray memoryData;
        QCryptographicHash hash;
        QString stagingRoot;
        QStringList rootNames;
    };

    SendState *makeTransfer(const QMimeData *mimeData);
    SendState *makeFileTransfer(const QList<QUrl> &urls);
    bool addFileTree(SendState *state, const QString &rootPath, const QString &rootName);
    void resetSend(SendState *state);
    void finishSend();
    void abortReceive(const QString &reason);
    bool sendRecord(const QByteArray &record);
    bool processBegin(const QByteArray &record);
    bool processItem(const QByteArray &record);
    bool processChunk(const QByteArray &record);
    bool processEnd(const QByteArray &record);
    void processAck(const QByteArray &record);
    void sendAck(quint64 id, bool success);
    QString safeDestination(const QString &relativePath) const;
    QString uniqueDestination(const QString &name) const;
    static quint64 newTransferId();

    SerialBridge *m_bridge;
    QTimer *m_sendTimer;
    QList<SendState *> m_sendQueue;
    ReceiveState *m_receive = nullptr;
    QString m_receiveDirectory;
    bool m_peerConnected = false;
    bool m_suppressNextClipboard = false;
};

#endif // TRANSFERMANAGER_H
