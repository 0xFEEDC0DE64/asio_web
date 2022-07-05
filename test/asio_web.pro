TEMPLATE = lib

QT += core testlib
CONFIG += c++latest

win32: DESTDIR=$${OUT_PWD}

include(paths.pri)

isEmpty(ASIO_WEBSERVER_DIR): error("ASIO_WEBSERVER_DIR not set")
isEmpty(CPPUTILS_DIR): error("CPPUTILS_DIR not set")
isEmpty(ESPCHRONO_DIR): error("ESPCHRONO_DIR not set")
isEmpty(FMT_DIR): error("FMT_DIR not set")

include(dependencies.pri)

include($$ASIO_WEBSERVER_DIR/asio_web_src.pri)
include($$CPPUTILS_DIR/cpputils_src.pri)
include($$CPPUTILS_DIR/test/cpputilstestutils_src.pri)
include($$ESPCHRONO_DIR/espchrono_src.pri)
include($$ESPCHRONO_DIR/test/espchronotestutils_src.pri)
include($$FMT_DIR/fmt_src.pri)

HEADERS += esp_log.h
