TEMPLATE = subdirs

equals(CLONE_CPPUTILS, 1) {
    CPPUTILS_DIR = $$PWD/cpputils

    message("Checking out cpputils...")
    exists($$CPPUTILS_DIR/.git) {
        system("git -C $$CPPUTILS_DIR pull")
    } else {
        isEmpty(CPPUTILS_URL) {
            CPPUTILS_URL = https://github.com/0xFEEDC0DE64/cpputils.git
        }
        system("git clone $$CPPUTILS_URL $$CPPUTILS_DIR")
    }
}

equals(CLONE_DATE, 1) {
    DATE_DIR = $$PWD/date

    message("Checking out date...")
    exists($$DATE_DIR/.git): {
        system("git -C $$DATE_DIR pull")
    } else {
        isEmpty(DATE_URL) {
            DATE_URL = https://github.com/0xFEEDC0DE64/date.git
        }
        system("git clone $$DATE_URL $$DATE_DIR")
    }
}

equals(CLONE_ESPCHRONO, 1) {
    ESPCHRONO_DIR = $$PWD/espchrono

    message("Checking out espchrono...")
    exists($$ESPCHRONO_DIR/.git): {
        system("git -C $$ESPCHRONO_DIR pull")
    } else {
        isEmpty(ESPCHRONO_URL) {
            ESPCHRONO_URL = https://github.com/0xFEEDC0DE64/espchrono.git
        }
        system("git clone $$ESPCHRONO_URL $$ESPCHRONO_DIR")
    }
}

equals(CLONE_EXPECTED, 1) {
    EXPECTED_DIR = $$PWD/expected

    message("Checking out expected...")
    exists($$EXPECTED_DIR/.git) {
        system("git -C $$EXPECTED_DIR pull")
    } else {
        isEmpty(EXPECTED_URL) {
            EXPECTED_URL = https://github.com/0xFEEDC0DE64/expected.git
        }
        system("git clone $$EXPECTED_URL $$EXPECTED_DIR")
    }
}

equals(CLONE_FMT, 1) {
    FMT_DIR = $$PWD/fmt

    message("Checking out fmt...")
    exists($$FMT_DIR/.git) {
        system("git -C $$FMT_DIR pull")
    } else {
        isEmpty(FMT_URL) {
            FMT_URL = https://github.com/0xFEEDC0DE64/fmt.git
        }
        system("git clone $$FMT_URL $$FMT_DIR")
    }
}

SUBDIRS += \
    asio_web.pro \
    webserver_example \
    websocket_client_example

sub-webserver_example.depends += sub-asio_web-pro
webserver_example.depends += sub-asio_web-pro
sub-websocket_client_example.depends += sub-asio_web-pro
websocket_client_example.depends += sub-asio_web-pro
