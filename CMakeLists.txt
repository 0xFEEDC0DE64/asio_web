set(headers
    src/asio_webserver/clientconnection.h
    src/asio_webserver/responsehandler.h
    src/asio_webserver/webserver.h
)

set(sources
    src/asio_webserver/clientconnection.cpp
    src/asio_webserver/responsehandler.cpp
    src/asio_webserver/webserver.cpp
)

set(dependencies
    asio
    log
    cpputils
    espchrono
    espcpputils
    date
    espchrono
    expected
    fmt
)

idf_component_register(
    INCLUDE_DIRS
        src
    SRCS
        ${headers}
        ${sources}
    REQUIRES
        ${dependencies}
)

target_compile_options(${COMPONENT_TARGET}
    PRIVATE
        -fstack-reuse=all
        -fstack-protector-all
        -Wno-unused-function
        -Wno-deprecated-declarations
        -Wno-missing-field-initializers
        -Wno-parentheses
)
