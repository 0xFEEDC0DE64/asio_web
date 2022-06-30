#include <QLoggingCategory>

// esp-idf includes
#include <esp_log.h>
#include <asio.hpp>

// 3rdparty lib includes
#include <cpputils.h>
#include <cppmacros.h>

// local includes
#include "examplewebserver.h"

namespace {
constexpr const char * const TAG = "ASIO_WEBSERVER";
} // namespace

int main(int argc, char *argv[])
{
    CPP_UNUSED(argc)
    CPP_UNUSED(argv)

    qSetMessagePattern(QStringLiteral("%{time dd.MM.yyyy HH:mm:ss.zzz} "
                                      "["
                                      "%{if-debug}D%{endif}"
                                      "%{if-info}I%{endif}"
                                      "%{if-warning}W%{endif}"
                                      "%{if-critical}C%{endif}"
                                      "%{if-fatal}F%{endif}"
                                      "] "
                                      "%{function}(): "
                                      "%{message}"));

    asio::io_context io_context;
    ExampleWebserver server{io_context, (unsigned short)8080};

    ESP_LOGI(TAG, "running mainloop");

    io_context.run();
}
