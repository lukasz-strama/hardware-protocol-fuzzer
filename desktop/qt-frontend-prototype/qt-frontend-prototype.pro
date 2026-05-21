QT += widgets

CONFIG += c++17

TARGET = HardwareProtocolFuzzerDesktop
TEMPLATE = app

SOURCES += \
    src/backend_bridge.cpp \
    src/main.cpp \
    src/main_window.cpp \
    ../../csv_logger.c \
    ../../diag.c \
    ../../frame.c \
    ../../session.c \
    ../../transport.c

HEADERS += \
    src/backend_bridge.h \
    src/main_window.h

INCLUDEPATH += \
    src \
    ../..

RESOURCES += \
    resources.qrc
