isEmpty(ASIO_WEBSERVER_DIR): error("ASIO_WEBSERVER_DIR not set")
isEmpty(CPPUTILS_DIR): error("CPPUTILS_DIR not set")
isEmpty(DATE_DIR): error("DATE_DIR not set")
isEmpty(ESPCHRONO_DIR): error("ESPCHRONO_DIR not set")
isEmpty(EXPECTED_DIR): error("EXPECTED_DIR not set")
isEmpty(FMT_DIR): error("FMT_DIR not set")

include($$ASIO_WEBSERVER_DIR/asio_web.pri)
include($$CPPUTILS_DIR/cpputils.pri)
include($$CPPUTILS_DIR/test/cpputilstestutils.pri)
include($$DATE_DIR/date.pri)
include($$ESPCHRONO_DIR/espchrono.pri)
include($$ESPCHRONO_DIR/test/espchronotestutils.pri)
include($$EXPECTED_DIR/expected.pri)
include($$FMT_DIR/fmt.pri)

QMAKE_CXXFLAGS += -Wno-missing-field-initializers
