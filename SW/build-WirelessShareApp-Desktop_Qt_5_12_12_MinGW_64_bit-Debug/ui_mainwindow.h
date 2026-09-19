/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 5.12.12
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralwidget;
    QVBoxLayout *verticalLayout;
    QGroupBox *deviceGroup;
    QGridLayout *deviceLayout;
    QLabel *portLabel;
    QComboBox *portCombo;
    QPushButton *refreshButton;
    QLabel *roleLabel;
    QComboBox *roleCombo;
    QLabel *passwordLabel;
    QLineEdit *passwordEdit;
    QLabel *receiveLabel;
    QLineEdit *receiveDirEdit;
    QPushButton *browseButton;
    QCheckBox *autoStartCheck;
    QPushButton *connectButton;
    QGroupBox *statusGroup;
    QVBoxLayout *statusLayout;
    QLabel *statusLabel;
    QGroupBox *logGroup;
    QVBoxLayout *logLayout;
    QPlainTextEdit *logEdit;
    QLabel *hintLabel;
    QMenuBar *menubar;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName(QString::fromUtf8("MainWindow"));
        MainWindow->resize(660, 520);
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName(QString::fromUtf8("centralwidget"));
        verticalLayout = new QVBoxLayout(centralwidget);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        deviceGroup = new QGroupBox(centralwidget);
        deviceGroup->setObjectName(QString::fromUtf8("deviceGroup"));
        deviceLayout = new QGridLayout(deviceGroup);
        deviceLayout->setObjectName(QString::fromUtf8("deviceLayout"));
        portLabel = new QLabel(deviceGroup);
        portLabel->setObjectName(QString::fromUtf8("portLabel"));

        deviceLayout->addWidget(portLabel, 0, 0, 1, 1);

        portCombo = new QComboBox(deviceGroup);
        portCombo->setObjectName(QString::fromUtf8("portCombo"));

        deviceLayout->addWidget(portCombo, 0, 1, 1, 1);

        refreshButton = new QPushButton(deviceGroup);
        refreshButton->setObjectName(QString::fromUtf8("refreshButton"));

        deviceLayout->addWidget(refreshButton, 0, 2, 1, 1);

        roleLabel = new QLabel(deviceGroup);
        roleLabel->setObjectName(QString::fromUtf8("roleLabel"));

        deviceLayout->addWidget(roleLabel, 1, 0, 1, 1);

        roleCombo = new QComboBox(deviceGroup);
        roleCombo->addItem(QString());
        roleCombo->addItem(QString());
        roleCombo->setObjectName(QString::fromUtf8("roleCombo"));

        deviceLayout->addWidget(roleCombo, 1, 1, 1, 2);

        passwordLabel = new QLabel(deviceGroup);
        passwordLabel->setObjectName(QString::fromUtf8("passwordLabel"));

        deviceLayout->addWidget(passwordLabel, 2, 0, 1, 1);

        passwordEdit = new QLineEdit(deviceGroup);
        passwordEdit->setObjectName(QString::fromUtf8("passwordEdit"));
        passwordEdit->setEchoMode(QLineEdit::Password);

        deviceLayout->addWidget(passwordEdit, 2, 1, 1, 2);

        receiveLabel = new QLabel(deviceGroup);
        receiveLabel->setObjectName(QString::fromUtf8("receiveLabel"));

        deviceLayout->addWidget(receiveLabel, 3, 0, 1, 1);

        receiveDirEdit = new QLineEdit(deviceGroup);
        receiveDirEdit->setObjectName(QString::fromUtf8("receiveDirEdit"));

        deviceLayout->addWidget(receiveDirEdit, 3, 1, 1, 1);

        browseButton = new QPushButton(deviceGroup);
        browseButton->setObjectName(QString::fromUtf8("browseButton"));

        deviceLayout->addWidget(browseButton, 3, 2, 1, 1);

        autoStartCheck = new QCheckBox(deviceGroup);
        autoStartCheck->setObjectName(QString::fromUtf8("autoStartCheck"));

        deviceLayout->addWidget(autoStartCheck, 4, 0, 1, 2);

        connectButton = new QPushButton(deviceGroup);
        connectButton->setObjectName(QString::fromUtf8("connectButton"));

        deviceLayout->addWidget(connectButton, 4, 2, 1, 1);


        verticalLayout->addWidget(deviceGroup);

        statusGroup = new QGroupBox(centralwidget);
        statusGroup->setObjectName(QString::fromUtf8("statusGroup"));
        statusLayout = new QVBoxLayout(statusGroup);
        statusLayout->setObjectName(QString::fromUtf8("statusLayout"));
        statusLabel = new QLabel(statusGroup);
        statusLabel->setObjectName(QString::fromUtf8("statusLabel"));
        statusLabel->setWordWrap(true);

        statusLayout->addWidget(statusLabel);


        verticalLayout->addWidget(statusGroup);

        logGroup = new QGroupBox(centralwidget);
        logGroup->setObjectName(QString::fromUtf8("logGroup"));
        logLayout = new QVBoxLayout(logGroup);
        logLayout->setObjectName(QString::fromUtf8("logLayout"));
        logEdit = new QPlainTextEdit(logGroup);
        logEdit->setObjectName(QString::fromUtf8("logEdit"));
        logEdit->setReadOnly(true);
        logEdit->setMaximumBlockCount(500);

        logLayout->addWidget(logEdit);


        verticalLayout->addWidget(logGroup);

        hintLabel = new QLabel(centralwidget);
        hintLabel->setObjectName(QString::fromUtf8("hintLabel"));
        hintLabel->setWordWrap(true);

        verticalLayout->addWidget(hintLabel);

        MainWindow->setCentralWidget(centralwidget);
        menubar = new QMenuBar(MainWindow);
        menubar->setObjectName(QString::fromUtf8("menubar"));
        MainWindow->setMenuBar(menubar);
        statusbar = new QStatusBar(MainWindow);
        statusbar->setObjectName(QString::fromUtf8("statusbar"));
        MainWindow->setStatusBar(statusbar);

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QApplication::translate("MainWindow", "WirelessShare", nullptr));
        deviceGroup->setTitle(QApplication::translate("MainWindow", "LOLIN S2 \350\243\235\347\275\256", nullptr));
        portLabel->setText(QApplication::translate("MainWindow", "USB CDC\357\274\232", nullptr));
        refreshButton->setText(QApplication::translate("MainWindow", "\351\207\215\346\226\260\346\225\264\347\220\206", nullptr));
        roleLabel->setText(QApplication::translate("MainWindow", "\350\243\235\347\275\256\350\247\222\350\211\262\357\274\232", nullptr));
        roleCombo->setItemText(0, QApplication::translate("MainWindow", "A\357\274\215\347\204\241\347\267\232\345\237\272\345\234\260\345\217\260 (AP)", nullptr));
        roleCombo->setItemText(1, QApplication::translate("MainWindow", "B\357\274\215\351\200\243\347\267\232\347\253\257 (Station)", nullptr));

        passwordLabel->setText(QApplication::translate("MainWindow", "\351\205\215\345\260\215\345\257\206\347\242\274\357\274\232", nullptr));
        passwordEdit->setPlaceholderText(QApplication::translate("MainWindow", "\345\205\251\345\217\260\351\233\273\350\205\246\350\253\213\350\274\270\345\205\245\347\233\270\345\220\214\345\257\206\347\242\274\357\274\2108\342\200\22363 \345\255\227\345\205\203\357\274\211", nullptr));
        receiveLabel->setText(QApplication::translate("MainWindow", "\346\216\245\346\224\266\347\233\256\351\214\204\357\274\232", nullptr));
        browseButton->setText(QApplication::translate("MainWindow", "\347\200\217\350\246\275\342\200\246", nullptr));
        autoStartCheck->setText(QApplication::translate("MainWindow", "\347\231\273\345\205\245 Windows \346\231\202\350\207\252\345\213\225\345\225\237\345\213\225", nullptr));
        connectButton->setText(QApplication::translate("MainWindow", "\351\200\243\347\267\232", nullptr));
        statusGroup->setTitle(QApplication::translate("MainWindow", "\351\200\243\347\267\232\347\213\200\346\205\213", nullptr));
        statusLabel->setText(QApplication::translate("MainWindow", "\345\260\232\346\234\252\351\200\243\346\216\245 USB \350\243\235\347\275\256", nullptr));
        logGroup->setTitle(QApplication::translate("MainWindow", "\345\202\263\350\274\270\350\250\230\351\214\204", nullptr));
        hintLabel->setText(QApplication::translate("MainWindow", "\346\217\220\347\244\272\357\274\232\350\246\226\347\252\227\351\227\234\351\226\211\345\276\214\347\250\213\345\274\217\344\273\215\346\234\203\347\225\231\345\234\250\347\263\273\347\265\261\345\214\243\343\200\202\350\244\207\350\243\275\346\226\207\345\255\227\343\200\201\345\234\226\347\211\207\343\200\201\346\252\224\346\241\210\346\210\226\350\263\207\346\226\231\345\244\276\345\215\263\345\217\257\350\207\252\345\213\225\345\202\263\351\200\201\343\200\202", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
