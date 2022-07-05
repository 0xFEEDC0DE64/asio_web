set(headers
    src/asio_web/clientconnection.h
    src/asio_web/responsehandler.h
    src/asio_web/webserver.h
    src/asio_web/websocketclientconnection.h
    src/asio_web/websocketstream.h
)

set(sources
    src/asio_web/clientconnection.cpp
    src/asio_web/responsehandler.cpp
    src/asio_web/webserver.cpp
    src/asio_web/websocketclientconnection.cpp
    src/asio_web/websocketstream.cpp
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
