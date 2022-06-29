ASIO_WEBSERVER_DIR = $$PWD/..

equals(CLONE_CPPUTILS, 1) {
    CPPUTILS_DIR = $$PWD/cpputils
    !exists($$CPPUTILS_DIR) {
        error("$$CPPUTILS_DIR not found, please check all dependencies")
    }
} else: exists($$PWD/../../cpputils/src) {
    CPPUTILS_DIR = $$PWD/../../cpputils
} else {
    error("cpputils not found, please check all dependencies")
}

equals(CLONE_DATE, 1) {
    DATE_DIR = $$PWD/date
    !exists($$DATE_DIR) {
        error("$$DATE_DIR not found, please check all dependencies")
    }
} else: exists($$PWD/../../date/include) {
    DATE_DIR = $$PWD/../../date
} else {
    error("date not found, please check all dependencies")
}

equals(CLONE_ESPCHRONO, 1) {
    ESPCHRONO_DIR = $$PWD/espchrono
    !exists($$ESPCHRONO_DIR) {
        error("$$ESPCHRONO_DIR not found, please check all dependencies")
    }
} else: exists($$PWD/../../espchrono/src) {
    ESPCHRONO_DIR = $$PWD/../../espchrono
} else {
    error("espchrono not found, please check all dependencies")
}

equals(CLONE_EXPECTED, 1) {
    EXPECTED_DIR = $$PWD/expected
    !exists($$EXPECTED_DIR) {
        error("$$EXPECTED_DIR not found, please check all dependencies")
    }
} else: exists($$PWD/../../expected/include) {
    EXPECTED_DIR = $$PWD/../../expected
} else {
    error("expected not found, please check all dependencies")
}

equals(CLONE_FMT, 1) {
    FMT_DIR = $$PWD/fmt
    !exists($$FMT_DIR) {
        error("$$FMT_DIR not found, please check all dependencies")
    }
} else: exists($$PWD/../../fmt/include) {
    FMT_DIR = $$PWD/../../fmt
} else {
    error("fmt not found, please check all dependencies")
}
