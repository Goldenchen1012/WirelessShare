QT += core
QT -= gui
CONFIG += console c++11
CONFIG -= app_bundle
TEMPLATE = app

INCLUDEPATH += ../WirelessShareApp

SOURCES += \
    protocoltest.cpp \
    ../WirelessShareApp/protocol.cpp

HEADERS += ../WirelessShareApp/protocol.h
