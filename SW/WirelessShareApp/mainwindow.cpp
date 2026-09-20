#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "serialbridge.h"
#include "transfermanager.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSerialPortInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QUrl>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_bridge(new SerialBridge(this))
    , m_transferManager(new TransferManager(m_bridge, this))
    , m_trayIcon(nullptr)
    , m_reconnectTimer(new QTimer(this))
    , m_statusTimer(new QTimer(this))
{
    ui->setupUi(this);
    createTrayIcon();
    loadSettings();
    refreshPorts();

    connect(ui->refreshButton, &QPushButton::clicked, this, &MainWindow::refreshPorts);
    connect(ui->browseButton, &QPushButton::clicked, this, &MainWindow::chooseReceiveDirectory);
    connect(ui->connectButton, &QPushButton::clicked, this, &MainWindow::toggleConnection);
    connect(ui->autoStartCheck, &QCheckBox::toggled, this, &MainWindow::setAutoStart);
    connect(m_bridge, &SerialBridge::statusChanged, this, &MainWindow::updateDeviceStatus);
    connect(m_bridge, &SerialBridge::errorOccurred, this, &MainWindow::showError);
    connect(m_transferManager, &TransferManager::activity, this, &MainWindow::appendLog);

    m_reconnectTimer->setInterval(3000);
    connect(m_reconnectTimer, &QTimer::timeout, this, &MainWindow::reconnectIfNeeded);
    m_reconnectTimer->start();
    m_statusTimer->setInterval(2000);
    connect(m_statusTimer, &QTimer::timeout, m_bridge, &SerialBridge::requestStatus);
    m_statusTimer->start();

    if (!ui->passwordEdit->text().isEmpty() && !m_requestedPortName.isEmpty()) {
        m_connectionRequested = true;
        QTimer::singleShot(250, this, [this] { connectDevice(true); });
    }
}

MainWindow::~MainWindow()
{
    saveSettings();
    delete ui;
}

void MainWindow::loadSettings()
{
    QSettings settings;
    m_requestedPortName = settings.value(QStringLiteral("device/port")).toString();
    m_requestedVendorId = quint16(settings.value(QStringLiteral("device/vendorId"), 0).toUInt());
    m_requestedProductId = quint16(settings.value(QStringLiteral("device/productId"), 0).toUInt());
    m_hasRequestedUsbIds = m_requestedVendorId != 0 || m_requestedProductId != 0;
    ui->roleCombo->setCurrentIndex(settings.value(QStringLiteral("device/role"), 0).toInt());
    ui->passwordEdit->setText(settings.value(QStringLiteral("device/password")).toString());
    QString receiveDir = settings.value(QStringLiteral("transfer/receiveDirectory")).toString();
    if (receiveDir.isEmpty())
        receiveDir = QDir(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation))
                .filePath(QStringLiteral("WirelessShare"));
    ui->receiveDirEdit->setText(QDir::toNativeSeparators(receiveDir));
    ui->autoStartCheck->setChecked(settings.value(QStringLiteral("application/autoStart"), false).toBool());
    m_transferManager->setReceiveDirectory(receiveDir);
}

void MainWindow::saveSettings()
{
    QSettings settings;
    settings.setValue(QStringLiteral("device/port"), m_requestedPortName);
    settings.setValue(QStringLiteral("device/vendorId"), m_requestedVendorId);
    settings.setValue(QStringLiteral("device/productId"), m_requestedProductId);
    settings.setValue(QStringLiteral("device/role"), ui->roleCombo->currentIndex());
    settings.setValue(QStringLiteral("device/password"), ui->passwordEdit->text());
    settings.setValue(QStringLiteral("transfer/receiveDirectory"), ui->receiveDirEdit->text());
    settings.setValue(QStringLiteral("application/autoStart"), ui->autoStartCheck->isChecked());
}

void MainWindow::refreshPorts()
{
    const QString selected = ui->portCombo->currentData().toString();
    const QString desiredPort = !m_requestedPortName.isEmpty() ? m_requestedPortName : selected;
    ui->portCombo->clear();
    int desiredIndex = -1;
    int likelyDeviceIndex = -1;
    const QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &port : ports) {
        QString label = port.portName();
        if (!port.description().isEmpty())
            label += QStringLiteral(" — ") + port.description();
        if (port.hasVendorIdentifier() && port.hasProductIdentifier())
            label += QStringLiteral(" [%1:%2]").arg(port.vendorIdentifier(), 4, 16, QLatin1Char('0'))
                    .arg(port.productIdentifier(), 4, 16, QLatin1Char('0'));
        ui->portCombo->addItem(label, port.portName());
        const int index = ui->portCombo->count() - 1;
        if (port.portName() == desiredPort
                || (m_hasRequestedUsbIds && port.hasVendorIdentifier() && port.hasProductIdentifier()
                    && port.vendorIdentifier() == m_requestedVendorId
                    && port.productIdentifier() == m_requestedProductId))
            desiredIndex = index;
        const QString deviceText = port.description() + QLatin1Char(' ') + port.manufacturer();
        if (deviceText.contains(QStringLiteral("LOLIN"), Qt::CaseInsensitive)
                || deviceText.contains(QStringLiteral("ESP32"), Qt::CaseInsensitive)
                || deviceText.contains(QStringLiteral("USB JTAG/serial"), Qt::CaseInsensitive))
            likelyDeviceIndex = index;
    }
    if (desiredIndex >= 0)
        ui->portCombo->setCurrentIndex(desiredIndex);
    else if (m_requestedPortName.isEmpty() && likelyDeviceIndex >= 0)
        ui->portCombo->setCurrentIndex(likelyDeviceIndex);
    else if (m_requestedPortName.isEmpty() && ports.size() == 1)
        ui->portCombo->setCurrentIndex(0);
    else
        ui->portCombo->setCurrentIndex(-1);
}

void MainWindow::chooseReceiveDirectory()
{
    const QString path = QFileDialog::getExistingDirectory(this, tr("選擇接收目錄"),
                                                            ui->receiveDirEdit->text());
    if (path.isEmpty())
        return;
    ui->receiveDirEdit->setText(QDir::toNativeSeparators(path));
    m_transferManager->setReceiveDirectory(path);
    saveSettings();
}

void MainWindow::toggleConnection()
{
    if (m_connectionRequested) {
        m_connectionRequested = false;
        m_bridge->close();
        m_transferManager->setPeerConnected(false);
        ui->connectButton->setText(tr("連線"));
        ui->statusLabel->setText(tr("已中斷 USB 裝置"));
        ui->portCombo->setEnabled(true);
        ui->roleCombo->setEnabled(true);
        ui->passwordEdit->setEnabled(true);
        return;
    }
    m_requestedPortName = ui->portCombo->currentData().toString();
    m_connectionRequested = true;
    if (!connectDevice(false))
        m_connectionRequested = false;
}

bool MainWindow::connectDevice(bool quiet)
{
    if (m_bridge->isOpen())
        return true;
    if (ui->portCombo->currentIndex() < 0) {
        if (!quiet)
            QMessageBox::warning(this, tr("WirelessShare"), tr("找不到可用的序列埠。"));
        return false;
    }
    const QString password = ui->passwordEdit->text();
    if (password.toUtf8().size() < 8 || password.toUtf8().size() > 63) {
        if (!quiet)
            QMessageBox::warning(this, tr("WirelessShare"), tr("配對密碼必須是 8 至 63 個 UTF-8 bytes。"));
        return false;
    }
    m_transferManager->setReceiveDirectory(ui->receiveDirEdit->text());
    m_requestedPortName = ui->portCombo->currentData().toString();
    const QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &port : ports) {
        if (port.portName() == m_requestedPortName && port.hasVendorIdentifier()
                && port.hasProductIdentifier()) {
            m_requestedVendorId = port.vendorIdentifier();
            m_requestedProductId = port.productIdentifier();
            m_hasRequestedUsbIds = true;
            break;
        }
    }
    if (!m_bridge->open(m_requestedPortName, ui->roleCombo->currentIndex() == 0,
                        password))
        return false;
    ui->connectButton->setText(tr("中斷"));
    ui->statusLabel->setText(tr("USB 已連接，正在等待裝置回應…"));
    ui->portCombo->setEnabled(false);
    ui->roleCombo->setEnabled(false);
    ui->passwordEdit->setEnabled(false);
    saveSettings();
    appendLog(tr("已開啟 %1").arg(m_bridge->portName()));
    return true;
}

void MainWindow::updateDeviceStatus(const QString &text, bool peerConnected)
{
    if (ui->statusLabel->text() != text)
        appendLog(text);
    ui->statusLabel->setText(text);
    m_transferManager->setPeerConnected(peerConnected);
    ui->statusLabel->setStyleSheet(peerConnected ? QStringLiteral("color: #147a36;") : QString());
}

void MainWindow::showError(const QString &text)
{
    appendLog(tr("錯誤：%1").arg(text));
    if (!m_bridge->isOpen()) {
        ui->connectButton->setText(tr("連線"));
        ui->portCombo->setEnabled(true);
        ui->roleCombo->setEnabled(true);
        ui->passwordEdit->setEnabled(true);
        m_transferManager->setPeerConnected(false);
    }
}

void MainWindow::appendLog(const QString &text)
{
    ui->logEdit->appendPlainText(QStringLiteral("[%1] %2")
                                 .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")), text));
}

void MainWindow::setAutoStart(bool enabled)
{
#ifdef Q_OS_WIN
    QSettings startup(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
                      QSettings::NativeFormat);
    if (enabled)
        startup.setValue(QStringLiteral("WirelessShare"),
                         QStringLiteral("\"") + QDir::toNativeSeparators(QCoreApplication::applicationFilePath())
                         + QStringLiteral("\" --background"));
    else
        startup.remove(QStringLiteral("WirelessShare"));
#else
    Q_UNUSED(enabled)
#endif
    saveSettings();
}

void MainWindow::reconnectIfNeeded()
{
    if (!m_connectionRequested || m_bridge->isOpen())
        return;
    refreshPorts();
    connectDevice(true);
}

void MainWindow::createTrayIcon()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;
    QMenu *menu = new QMenu(this);
    QAction *openAction = menu->addAction(tr("開啟 WirelessShare"));
    QAction *receiveAction = menu->addAction(tr("開啟接收目錄"));
    menu->addSeparator();
    QAction *quitAction = menu->addAction(tr("結束"));
    connect(openAction, &QAction::triggered, this, [this] { showNormal(); raise(); activateWindow(); });
    connect(receiveAction, &QAction::triggered, this, [this] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(ui->receiveDirEdit->text()));
    });
    connect(quitAction, &QAction::triggered, this, [this] {
        m_quitting = true;
        qApp->quit();
    });
    m_trayIcon = new QSystemTrayIcon(style()->standardIcon(QStyle::SP_DriveNetIcon), this);
    m_trayIcon->setToolTip(tr("WirelessShare"));
    m_trayIcon->setContextMenu(menu);
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
            showNormal(); raise(); activateWindow();
        }
    });
    m_trayIcon->show();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!m_quitting && m_trayIcon) {
        hide();
        event->ignore();
        m_trayIcon->showMessage(tr("WirelessShare"), tr("程式仍在背景監聽剪貼簿。"),
                                QSystemTrayIcon::Information, 2000);
        return;
    }
    saveSettings();
    QMainWindow::closeEvent(event);
}
