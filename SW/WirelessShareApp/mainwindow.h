#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class QCloseEvent;
class QSystemTrayIcon;
class QTimer;
class SerialBridge;
class TransferManager;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
protected:
    void closeEvent(QCloseEvent *event) override;
private slots:
    void refreshPorts();
    void chooseReceiveDirectory();
    void toggleConnection();
    void updateDeviceStatus(const QString &text, bool peerConnected);
    void showError(const QString &text);
    void appendLog(const QString &text);
    void setAutoStart(bool enabled);
    void reconnectIfNeeded();
private:
    void loadSettings();
    void saveSettings();
    bool connectDevice(bool quiet = false);
    void createTrayIcon();
    Ui::MainWindow *ui;
    SerialBridge *m_bridge;
    TransferManager *m_transferManager;
    QSystemTrayIcon *m_trayIcon;
    QTimer *m_reconnectTimer;
    QTimer *m_statusTimer;
    QString m_requestedPortName;
    quint16 m_requestedVendorId = 0;
    quint16 m_requestedProductId = 0;
    bool m_hasRequestedUsbIds = false;
    bool m_connectionRequested = false;
    bool m_quitting = false;
};
#endif // MAINWINDOW_H
