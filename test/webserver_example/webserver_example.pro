TEMPLATE = app

QT += core

CONFIG += c++latest

HEADERS += \
    debugresponsehandler.h \
    errorresponsehandler.h \
    examplewebserver.h \
    rootresponsehandler.h

SOURCES += \
    debugresponsehandler.cpp \
    errorresponsehandler.cpp \
    examplewebserver.cpp \
    main.cpp \
    rootresponsehandler.cpp

unix: TARGET=webserver_example.bin
DESTDIR=$${OUT_PWD}/..
INCLUDEPATH += $$PWD/..

include(../paths.pri)

include(../dependencies.pri)

unix: {
    LIBS += -Wl,-rpath=\\\$$ORIGIN
}
LIBS += -L$${OUT_PWD}/..
LIBS += -lasio_webserver
