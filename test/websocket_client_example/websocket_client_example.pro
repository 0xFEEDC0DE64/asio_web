TEMPLATE = app

QT += core

CONFIG += c++latest

HEADERS += \
    websocketclient.h

SOURCES += \
    main.cpp \
    websocketclient.cpp

unix: TARGET=websocket_client_example.bin
DESTDIR=$${OUT_PWD}/..
INCLUDEPATH += $$PWD/..

include(../paths.pri)

include(../dependencies.pri)

unix: {
    LIBS += -Wl,-rpath=\\\$$ORIGIN
}
LIBS += -L$${OUT_PWD}/..
LIBS += -lasio_web

LIBS += -lssl -lcrypto
