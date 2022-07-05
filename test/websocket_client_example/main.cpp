#include <QLoggingCategory>

// esp-idf includes
#include <esp_log.h>
#include <asio.hpp>
#include <asio/ssl.hpp>
#include <asio/buffer.hpp>

// 3rdparty lib includes
#include <cppmacros.h>

// local includes
#include "websocketclient.h"

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

    WebsocketClient c{io_context};
    c.start();

    ESP_LOGI(TAG, "running mainloop");

    io_context.run();
}
